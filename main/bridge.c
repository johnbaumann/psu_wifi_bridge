#include "bridge.h"
#include "serial.h"
#include "tcp.h"

#include <stdbool.h>

void Bridge_Task_Server(void *pvParameters)
{
    Serial_Init();
    //TCP_Init();

    while (1)
    {
        //TCP_ProcessEvents();
        Serial_ProcessEvents();
    }

    //TCP_Cleanup();
}
