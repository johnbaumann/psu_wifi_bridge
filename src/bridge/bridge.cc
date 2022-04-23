#include "bridge.h"

#include "network/tcp.h"
#include "tty/serial.h"

#include <stdbool.h>

volatile bool disable_uploads = true;

void Init_Bridge()
{
    Serial_Init();
    TCP_Init();
}

void Raw_Bridge_Task_Server(void *pvParameters)
{
    while (1)
    {
        if (!disable_uploads)
        {
            Serial_ProcessEvents();
            TCP_ProcessEvents();
        }
        else
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }

    TCP_Cleanup();
}
