#ifndef _TCP_H
#define _TCP_H

#include <stdbool.h>

void TCP_Cleanup();
bool TCP_Init();
bool TCP_ProcessEvents();
//bool TCP_Retransmit();
void TCP_SendData(int len, void *dataptr);
//void TCP_Task_Server(void *pvParameters);
void tcp_server_task(void *pvParameters);

#endif // _TCP_H