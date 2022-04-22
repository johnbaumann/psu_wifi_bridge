#include <driver/uart.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
// NanoPB
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "siopayload.pb.h"
// Local Includes
#include "bridge/bridge.h"
#include "client.h"
#include "file_server.h"
#include "system/pins.h"
#include "tty/sio1.h"

#define SERVER_IP "10.170.241.9"
#define SERVER_PORT 6700

#define UART_PORT (2)
#define BUFSIZE (512)
size_t uart_len = 0;
char server_ip[] = SERVER_IP;
int addr_family = 0;
int ip_protocol = 0;
int sock = -1;
uint8_t serbuf[BUFSIZE];
uint8_t recbuf[BUFSIZE];
mydata_t *mydata;
size_t gsize = 0;

static const char *TAG = "Protobuf_Client";

static bool write_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count) {
    int fd = (intptr_t)stream->state;
    return send(fd, buf, count, 0) == count;
}

static bool read_callback(pb_istream_t *stream, uint8_t *buf, size_t count) {
    int fd = (intptr_t)stream->state;
    int result;

    if (count == 0) return true;

    result = recv(fd, buf, count, MSG_WAITALL);

    if (result == 0) stream->bytes_left = 0; /* EOF */

    return result == count;
}

static bool encode_string(pb_ostream_t *stream, const pb_field_t *field,
                  void *const *arg) {
   //char *str = *(char **)arg;
   if (!pb_encode_tag_for_field(stream, field))
       return false;
   pb_encode_string(stream, serbuf, 1);
   //ESP_LOGI(TAG, "encoding string: %c\n", serbuf[1]);
   //ESP_LOGI(TAG, "byte written: %d\n",stream->bytes_written);
   return true;
}

bool DataTransfer_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    int strlen = stream->bytes_left;
    if (strlen > sizeof(recbuf) - 1) return false;
    if (!pb_read(stream, recbuf, strlen)) 
        return false;
    return true;
}

// bool msg_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
//     if (field->tag == SIOPayload_data_transfer_tag) {
//         DataTransfer *msg = (DataTransfer *)field->pData;
//         msg->data.funcs.decode = DataTransfer_callback;
//     } else if (field->tag == SIOPayload_flow_control_tag) {
//         printf("Flow Control:\n");
//     }

//     return true;
// }

pb_ostream_t pb_ostream_from_socket(int fd) {
    pb_ostream_t stream = {&write_callback, (void *)(intptr_t)fd, SIZE_MAX, 0};
    return stream;
}

pb_istream_t pb_istream_from_socket(int fd) {
    pb_istream_t stream = {&read_callback, (void *)(intptr_t)fd, SIZE_MAX};
    return stream;
}

static bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
    mydata_t *mydata = (mydata_t *)*arg;
    if (!pb_encode_tag_for_field(stream, field)) return false;

    return pb_encode_string(stream, mydata->buf, mydata->len);
}

static void process() {
     if (recv(sock, NULL, 2, MSG_PEEK | MSG_DONTWAIT) > 0) {
        SIOPayload payload = {};
        payload.data_transfer.data.funcs.decode = DataTransfer_callback;
        pb_istream_t input = pb_istream_from_socket(sock);
         if (!pb_decode_delimited(&input, SIOPayload_fields, &payload)) {
            ESP_LOGE(TAG, "Decode failed: %s\n", PB_GET_ERROR(&input));
         }

         if (payload.has_data_transfer) {
            uart_write_bytes(UART_PORT, recbuf, 1);
            //ESP_LOGI(TAG, "Wrote 1 byte to serial");
         } else {
             //ESP_LOGI(TAG, "Flow Control Only");
         }
         Set_DTR(payload.flow_control.dxr);
         Set_RTS(payload.flow_control.xts);
         //ESP_LOGI(TAG, "Setting PSX DTR to: %d\n", payload.flow_control.dxr);
         //ESP_LOGI(TAG, "Setting PSX CTS to: %d\n", payload.flow_control.xts);
    }

    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, &uart_len));
    if (uart_len > 0) {
        uart_read_bytes(UART_PORT, serbuf, 1, 1);
        SIOPayload send_payload = {};
        send_payload.data_transfer.data.funcs.encode = encode_string;
        send_payload.has_data_transfer = true;
        send_payload.has_flow_control = true;
        send_payload.flow_control.dxr = gpio_get_level(kPin_DSR);
        send_payload.flow_control.xts = gpio_get_level(kPin_CTS);
        pb_ostream_t output = pb_ostream_from_socket(sock);
        if (!pb_encode_delimited(&output, SIOPayload_fields, &send_payload)) {
            ESP_LOGE(TAG, "Encoding bytes failed: %s\n", PB_GET_ERROR(&output));
            return;
        }
        //ESP_LOGI(TAG, "Sent 1 byte and Flow Control DTR: %d RTS: %d", dsr_state, cts_state );
    } else {
        if (cts_state != prev_cts_state || dsr_state != prev_dsr_state) {
            prev_cts_state = cts_state;
            prev_dsr_state = dsr_state;
            SIOPayload send_payload = {};
            send_payload.has_data_transfer = false;
            send_payload.has_flow_control = true;
            send_payload.flow_control.dxr = dsr_state;
            send_payload.flow_control.xts = cts_state;
            pb_ostream_t output = pb_ostream_from_socket(sock);
            if (!pb_encode_delimited(&output, SIOPayload_fields, &send_payload)) {
                ESP_LOGE(TAG, "Encoding bytes failed: %s\n", PB_GET_ERROR(&output));
                return;
            }
            //ESP_LOGI(TAG, "Flow Control DTR: %d RTS: %d", dsr_state, cts_state);
        }   
    }

   
}


static bool tcp_client_init() {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(server_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return false;
    }
    //ESP_LOGI(TAG, "Socket created, connecting to %s:%d", server_ip, SERVER_PORT);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        return false;
    }
    //ESP_LOGI(TAG, "Successfully connected");

    return true;
}

void tcp_client_task(void *pvParameters) {
    if (tcp_client_init()) {
        while (1) {
            if (disable_uploads) {
                process();
            } else {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
    }
    ESP_LOGE(TAG, "TCP client task ended");
    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}