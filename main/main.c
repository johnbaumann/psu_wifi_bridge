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

/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/

const gpio_num_t kPin_Reset = GPIO_NUM_15;
const gpio_num_t kPin_PowerButton = GPIO_NUM_4;
const gpio_num_t kPin_LED = GPIO_NUM_2;
const gpio_num_t kPin_Power = GPIO_NUM_5;

static volatile int edge_intr_times = 0;
static volatile bool power_enabled = false;


static void Console_Toggle_Power()
{
    power_enabled = !power_enabled;
    gpio_set_level(kPin_Power, power_enabled);
    gpio_set_level(kPin_LED, power_enabled);
}

static void Console_Reset()
{
    gpio_set_direction(kPin_Reset, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_Reset, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(kPin_Reset, 1);
    gpio_set_direction(kPin_Reset, GPIO_MODE_INPUT);
}

void Task_PowerButton(void *pvParameter)
{
    bool input_ignore = false;
    uint8_t input_old = 0;
    uint8_t input_held = 0;
    uint8_t input_trig = 0;
    uint64_t input_pressed_timestamp = 0;

    while (1)
    {

        input_old = input_held;
        input_held = 0;

        if (!gpio_get_level(kPin_PowerButton))
        {
            input_held = true;
        }

        input_trig = ~input_old & input_held;

        if (input_trig)
        {
            input_pressed_timestamp = esp_log_timestamp();
            input_ignore = false;
        }

        if (!input_ignore)
        {
            if (power_enabled)
            {
                if (input_held && esp_log_timestamp() - input_pressed_timestamp > 1000)
                {
                    Console_Toggle_Power();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    input_ignore = true;
                }
            }
            else
            {
                if (input_held && esp_log_timestamp() - input_pressed_timestamp > 50)
                {
                    Console_Toggle_Power();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    input_ignore = true;
                }
            }
        }

        vTaskDelay(16 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    gpio_reset_pin(kPin_LED);
    gpio_set_direction(kPin_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_LED, power_enabled);

    gpio_reset_pin(kPin_Power);
    gpio_set_direction(kPin_Power, GPIO_MODE_OUTPUT);
    gpio_set_level(kPin_Power, power_enabled);

    gpio_reset_pin(kPin_Reset);
    gpio_set_direction(kPin_Reset, GPIO_MODE_INPUT);

    gpio_set_direction(kPin_PowerButton, GPIO_MODE_INPUT);
    gpio_set_pull_mode(kPin_PowerButton, GPIO_PULLUP_ONLY);

    xTaskCreate(Task_PowerButton, "Power Button Monitor", 2048, NULL, 1, NULL);
    Init_Wifi();
    //xTaskCreate(TCP_Task_Server, "tcp_server_task", 1024 * 10, NULL, 4, NULL);
    //xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    xTaskCreate(Bridge_Task_Server, "tcp_serial_bridge", 1024 * 10, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
