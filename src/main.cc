/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "bridge/bridge.h"
#include "network/file_server.h"
#include "network/siopayload.pb.h"
#include "network/tcp.h"
#include "network/wifi_client.h"
#include "system/log.h"
#include "system/pins.h"
#include "tty/serial.h"
#include "tty/sio1.h"

#include "nanopb/pb_encode.h"
#include "nanopb/pb_decode.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <stdio.h>

extern "C"
{
    void app_main(void);
}

bool write_string(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg)
{
    printf("Hello encode\n");

    //return pb_encode_string(stream, (uint8_t*)str, strlen(str));
    return true;
}

bool read_string(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    return true;
}

int nanopb_test()
{
    /* This is the buffer where we will store our message. */
    uint8_t buffer[128];
    size_t message_length;
    bool status;

    {
        SIOPayload payload = SIOPayload_init_zero;

        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        payload.type.data_send.length = 2;
        payload.type.data_send.data.funcs.encode = write_string;
        payload.which_type = SIOPayload_data_send_tag;
        status = pb_encode(&stream, SIOPayload_fields, &payload);
        message_length = stream.bytes_written;

        /* Then just check for any errors.. */
        if (!status)
        {
            printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }
    }

    {
        SIOPayload payload = SIOPayload_init_zero;

        pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);

        status = pb_decode(&stream, SIOPayload_fields, &payload);

        /* Check for errors... */
        if (!status)
        {
            printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }

        /* Print the data contained in the message. */
        printf("SIO Payload\n");
        printf("  which_type: %d\n", payload.which_type);
        switch(payload.which_type)
        {
            case SIOPayload_data_send_tag:
                printf("  data_send.length: %d\n", payload.type.data_send.length);
                break;

            case SIOPayload_data_receive_tag:
                printf("  data_receive.length: %d\n", payload.type.data_receive.length);
                //printf("  data_receive.data: %s\n", payload.type.data_receive.data.funcs->decode(payload.type.data_receive.data.state));
                break;

            case SIOPayload_control_receive_tag:
                printf("  receive\n");
                printf("    dsr: %d\n", payload.type.control_receive.dsr);
                printf("    cts: %d\n", payload.type.control_receive.cts);
                break;

            case SIOPayload_control_send_tag:
                printf("  send\n");
                printf("    dtr: %d\n", payload.type.control_send.dtr);
                printf("    rts: %d\n", payload.type.control_send.rts);
                break;
        }
    }

    return 0;
}

void setup_pins()
{
    gpio_reset_pin(kPin_DSR);
    // gpio_set_pull_mode(kPin_DSR, GPIO_PULLUP_ENABLE);
    gpio_set_direction(kPin_DSR, GPIO_MODE_INPUT);

    gpio_reset_pin(kPin_CTS);
    // gpio_set_pull_mode(kPin_CTS, GPIO_PULLUP_ENABLE);
    gpio_set_direction(kPin_CTS, GPIO_MODE_INPUT);

    gpio_reset_pin(kPin_DTR);
    gpio_set_direction(kPin_DTR, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_DTR, dtr_state);

    gpio_reset_pin(kPin_RTS);
    gpio_set_direction(kPin_RTS, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_RTS, rts_state);

    dsr_state = gpio_get_level(kPin_DSR);
    cts_state = gpio_get_level(kPin_CTS);
}

void actual_main(void)
{
    nanopb_test();

    setup_pins();

    // Init and connect to Wifi AP
    Init_Wifi();

    Init_Bridge();

    // Serial and TCP repeater
    xTaskCreate(Raw_Bridge_Task_Server, "tcp_serial_bridge", 1024 * 10, NULL, 5, NULL);

    ESP_ERROR_CHECK(start_file_server("/"));

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    actual_main();
}