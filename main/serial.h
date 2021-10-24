#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>

extern int uart_baud_rate;

void Serial_Fast();
void Serial_SendData(int len, uint8_t *dataptr);
void Serial_Slow();
void Serial_Init();
void Serial_ProcessEvents();

#endif // _SERIAL_H