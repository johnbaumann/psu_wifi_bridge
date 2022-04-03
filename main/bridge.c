#include "bridge.h"
#include "serial.h"
#include "tcp.h"

#include <stdbool.h>

void Init_Bridge()
{
    Serial_Init();
    TCP_Init();
}

void Protocol_Bridge_Task_Server(void *pvParameters)
{
    while (1)
    {
        Serial_ProcessEvents();
        TCP_ProcessEvents();
    }

    TCP_Cleanup();
}


void Raw_Bridge_Task_Server(void *pvParameters)
{
    while (1)
    {
        Serial_ProcessEvents();
        TCP_ProcessEvents();
    }

    TCP_Cleanup();
}

