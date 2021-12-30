/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "bridge.h"
#include "log.h"
#include "serial.h"
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

void app_main(void)
{
    // Init and connect to Wifi AP
    Init_Wifi();

    // Serial and TCP repeater
    xTaskCreate(Bridge_Task_Server, "tcp_serial_bridge", 1024 * 10, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
