#include "bridge.h"
#include "serial.h"
#include "tcp.h"

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

void Bridge_Task_Server(void *pvParameters)
{
    Serial_Init();
    TCP_Init();

    while (1)
    {
        if(serial_enabled)
        {
        Serial_ProcessEvents();
        TCP_ProcessEvents();
        }
        else
        {
            Serial_CheckToggle();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    TCP_Cleanup();
}
