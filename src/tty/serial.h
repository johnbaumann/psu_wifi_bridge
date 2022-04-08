#pragma once

#include <driver/uart.h>
#include <stdint.h>

extern int uart_baud_rate;
extern uart_config_t uart_config;

void Serial_Fast();
void Serial_SendData(int len, uint8_t *dataptr);
void Serial_Slow();
void Serial_Init();
void Serial_ProcessEvents();
