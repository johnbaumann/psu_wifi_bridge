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
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <driver/uart.h>
// NanoPB
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "siopayload.pb.h"
// Local Includes
#include "client.h"
#include "tty/sio1.h"
#include "bridge/bridge.h"
#include "file_server.h"
#include "system/pins.h"

#define SERVER_IP "10.170.241.9"
#define SERVER_PORT 6700

#define UART_PORT (2)
#define BUFSIZE (4096)

char server_ip[] = SERVER_IP;
int addr_family = 0;
int ip_protocol = 0;
int sock = -1;
uint8_t serbuf[BUFSIZE];
uint8_t recbuf[BUFSIZE];
mydata_t *mydata;

static const char *TAG = "Protobuf_Client";

static bool write_callback(pb_ostream_t *stream, const uint8_t *buf,
                           size_t count)
{
    int fd = (intptr_t)stream->state;
    return send(fd, buf, count, 0) == count;
}

static bool read_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    int fd = (intptr_t)stream->state;
    int result;

    if (count == 0)
        return true;

    result = recv(fd, buf, count, MSG_WAITALL);

    if (result == 0)
        stream->bytes_left = 0; /* EOF */

    return result == count;
}

bool DataTransfer_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{

    int strlen = stream->bytes_left;
    mydata_t data = {(size_t)strlen, recbuf};
    if (strlen > sizeof(recbuf) - 1)
        return false;

    if (!pb_read(stream, recbuf, strlen))
        return false;

    mydata = &data;
    return true;
}

bool msg_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    if (field->tag == SIOPayload_data_transfer_tag)
    {
        DataTransfer *msg = {};
        msg->data.funcs.decode = DataTransfer_callback;
    }
    else if (field->tag == SIOPayload_flow_control_tag)
    {
        printf("Flow Control:\n");
    }

    return true;
}

pb_ostream_t pb_ostream_from_socket(int fd)
{
    pb_ostream_t stream = {&write_callback, (void *)(intptr_t)fd, SIZE_MAX, 0};
    return stream;
}

pb_istream_t pb_istream_from_socket(int fd)
{
    pb_istream_t stream = {&read_callback, (void *)(intptr_t)fd, SIZE_MAX};
    return stream;
}

static bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field,
                         void *const *arg)
{

    mydata_t *mydata = (mydata_t *)*arg;
    if (!pb_encode_tag_for_field(stream, field))
        return false;

    return pb_encode_string(stream, mydata->buf, mydata->len);
}

static bool process_inbound()
{
    // Check is there is data in the socket rx buffer
    int ret1 = recv(sock, NULL, 2, MSG_PEEK | MSG_DONTWAIT);
    if (ret1 > 0)
    {
        {
            SIOPayload payload = SIOPayload_init_zero;
            payload.cb_type.funcs.decode = msg_callback;
            pb_istream_t stream = pb_istream_from_socket(sock);
            if (!pb_decode_delimited(&stream, SIOPayload_fields, &payload))
            {
                ESP_LOGE(TAG, "Decode failed: %s\n", PB_GET_ERROR(&stream));
                return false;
            }
            // ESP_LOGI(TAG, "Decoding successful, message received..");
            switch (payload.which_type)
            {

            case SIOPayload_data_transfer_tag:
                ESP_LOGI(TAG, "data_transfer_tag, writing data to serial");
                uart_write_bytes(UART_PORT, mydata->buf, mydata->len);
                break;

            case SIOPayload_flow_control_tag:
                Set_DTR(payload.type.flow_control.xts);
                ESP_LOGI(TAG, "Setting PSX DSR to: %d\n", payload.type.flow_control.dxr);
                Set_RTS(payload.type.flow_control.dxr);
                ESP_LOGI(TAG, "Setting PSX CTS to: %d\n", payload.type.flow_control.xts);
                break;
            default:
                ESP_LOGW(TAG, "Received message matched no known type");
                break;
            }
        }
    }

    return true;
}

static bool process_outbound()
{
    // Flow Control Messages
    if (cts_state != prev_cts_state ||
        dsr_state != prev_dsr_state)
    {
        prev_cts_state = cts_state;
        prev_dsr_state = dsr_state;

        SIOPayload payload = SIOPayload_init_zero;
        payload.which_type = SIOPayload_flow_control_tag;
        payload.type.flow_control.dxr = dsr_state;
        payload.type.flow_control.xts = cts_state;
        pb_ostream_t output = pb_ostream_from_socket(sock);

        if (!pb_encode_delimited(&output, SIOPayload_fields, &payload))
        {
            ESP_LOGE(TAG, "Encoding failed: %s\n", PB_GET_ERROR(&output));
            return false;
        }
        ESP_LOGI(TAG, "Encoding flow control message successful, data sent");
    }

    // Data Transfer Messages
    size_t len = 0;
    // If there is data in serial rx buffer, return how much
    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, &len));
    // If there is data, read it, encode it, and send it to the server
    if (len > 0 && disable_uploads)
    {
        uart_read_bytes(UART_PORT, serbuf, len, 1);
        mydata_t dataout = {len, serbuf};
        SIOPayload payload = SIOPayload_init_zero;
        payload.which_type = SIOPayload_data_transfer_tag;
        payload.type.data_transfer.data.funcs.encode = encode_bytes;
        payload.type.data_transfer.data.arg = &dataout;
        pb_ostream_t output = pb_ostream_from_socket(sock);
        if (!pb_encode_delimited(&output, SIOPayload_fields, &payload))
        {
            ESP_LOGE(TAG, "Encoding bytes failed: %s\n", PB_GET_ERROR(&output));
            return false;
        }
        ESP_LOGI(TAG, "Encoding data message successful, sent with length of: %d", len);
    }

    return true;
}

static bool tcp_client_init()
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(server_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return false;
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", server_ip, SERVER_PORT);

    int err = connect(sock, (struct sockaddr *)&dest_addr,
                      sizeof(struct sockaddr_in6));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        return false;
    }
    ESP_LOGI(TAG, "Successfully connected");

    return true;
}

void tcp_client_task(void *pvParameters)
{

    if (tcp_client_init())
    {
        while (1)
        {
            if (disable_uploads)
            {
                if (!process_outbound())
                {
                    ESP_LOGE(TAG, "process_outbound failed");
                    break;
                }
                if (!process_inbound())
                {
                    ESP_LOGE(TAG, "process_inbound failed");
                    break;
                }
            }
            else
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
    }
    ESP_LOGE(TAG, "TCP client task ended");
    if (sock != -1)
    {
        ESP_LOGE(TAG, "Shutting down socket...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}