#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdbool.h>
#include <stdint.h>

extern int uart_baud_rate;
extern volatile bool serial_enabled;

void Serial_CheckToggle();
void Serial_Deinit();
void Serial_Fast();
void Serial_SendData(int len, uint8_t *dataptr);
void Serial_Slow();
void Serial_Init();
void Serial_ProcessEvents();
void Serial_Toggle();

#endif // _SERIAL_H