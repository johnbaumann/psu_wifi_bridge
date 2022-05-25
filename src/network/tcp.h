#pragma once
#include "tty/serial.h"
#include <stdbool.h>
#include "fifo.h"
void TCP_Cleanup();
bool TCP_Init();
bool TCP_ProcessEvents();
void TCP_SendData(int len, void *dataptr);
void TCP_Task_Server(void *pvParameters);
extern bool connected;