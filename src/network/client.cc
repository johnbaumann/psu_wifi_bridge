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
#include "system/pins.h"
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include "client.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "siopayload.pb.h"
#include "tty/sio1.h"

#define HOST_IP_ADDR "10.170.241.9"
#define HOST_PORT 3333

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

void tcp_client_task(void *pvParameters)
{
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1)
    {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(HOST_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, HOST_PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr,
                          sizeof(struct sockaddr_in6));
        if (err != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");
        bool dtr = dtr_state;
        bool rts = rts_state;
        int psx_rts = gpio_get_level(kPin_CTS);
        int psx_dtr = gpio_get_level(kPin_DSR);

        while (1)
        {

            if (psx_rts != gpio_get_level(kPin_CTS) ||
                psx_dtr != gpio_get_level(kPin_DSR))
            {
                psx_rts = gpio_get_level(kPin_CTS);
                psx_dtr = gpio_get_level(kPin_DSR);
                FlowControl ftcl = FlowControl_init_zero;
                ftcl.dxr = psx_dtr;
                ftcl.xts = psx_rts;

                SIOPayload payload = SIOPayload_init_zero;
                pb_ostream_t output = pb_ostream_from_socket(sock);

                payload.type.flow_control = ftcl;
                payload.which_type = SIOPayload_flow_control_tag;

                if (!pb_encode_delimited(&output, SIOPayload_fields, &payload))
                {
                    ESP_LOGE(TAG, "Encoding failed: %s\n", PB_GET_ERROR(&output));
                    break;
                }
                ESP_LOGI(TAG, "Encoding successful, data sent");
            }

            // crap way to check if any data is coming in on the socket to be
            // received... temporary
            int count = 0;
            ioctl(sock, FIONREAD, &count);
            // ESP_LOGW(TAG, "count: %d", count);
            if (count > 0)
            {
                ESP_LOGI(TAG, "Received some data...");

                SIOPayload payload = {};
                pb_istream_t input = pb_istream_from_socket(sock);

                if (!pb_decode_delimited(&input, SIOPayload_fields, &payload))
                {
                    ESP_LOGE(TAG, "Decode failed: %s\n", PB_GET_ERROR(&input));
                    break;
                }

                switch (payload.which_type)
                {

                case SIOPayload_data_transfer_tag:
                    ESP_LOGI(TAG, "data_transfer_tag");
                    break;

                case SIOPayload_flow_control_tag:
                    printf("  receive\n");
                    printf("Setting PSX DTR to: %d\n", payload.type.flow_control.dxr);
                    printf("Setting PSX CTS to: %d\n", payload.type.flow_control.xts);
                    break;
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}