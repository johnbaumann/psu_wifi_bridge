#pragma once

#include <stdbool.h>

void TCP_Cleanup();
bool TCP_Init();
bool TCP_ProcessEvents();
void TCP_SendData(int len, void *dataptr);
void TCP_Task_Server(void *pvParameters);
