/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "bridge.h"
#include "file_server.h"
#include "log.h"
#include "serial.h"
#include "sio1.h"
#include "tcp.h"
#include "wifi_client.h"

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <sdkconfig.h>
#include <esp_log.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

void setup_pins()
{
    gpio_reset_pin(kPin_DSR);
    //gpio_set_pull_mode(kPin_DSR, GPIO_PULLUP_ENABLE);
    gpio_set_direction(kPin_DSR, GPIO_MODE_INPUT);

    gpio_reset_pin(kPin_CTS);
    //gpio_set_pull_mode(kPin_CTS, GPIO_PULLUP_ENABLE);
    gpio_set_direction(kPin_CTS, GPIO_MODE_INPUT);

    gpio_reset_pin(kPin_DTR);
    gpio_set_direction(kPin_DTR, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_DTR, kPin_DTR);

    gpio_reset_pin(kPin_RTS);
    gpio_set_direction(kPin_RTS, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_RTS, kPin_RTS);

    dsr_state = gpio_get_level(kPin_DSR);
    cts_state = gpio_get_level(kPin_CTS);
}

void app_main(void)
{
    setup_pins();

    // Init and connect to Wifi AP
    Init_Wifi();

    // Serial and TCP repeater
    xTaskCreate(Bridge_Task_Server, "tcp_serial_bridge", 1024 * 10, NULL, 5, NULL);

    ESP_ERROR_CHECK(start_file_server("/"));

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
