#pragma once

#include <driver/uart.h>
#include <stdint.h>
#include "network/fifo.h"
#include "sio1.h"


extern int uart_baud_rate;
extern uart_config_t uart_config;

void Serial_Fast();
void Serial_SendData(int len, uint8_t *dataptr);
void Serial_Slow();
void Serial_Init();
void Serial_ProcessEvents();
void Serial_StateMachine();
void Serial_DecodeMessage();
void Serial_ProcessMessage();
void Serial_TransmitData();
void Serial_TransmitFC();
extern CircularBuffer rxfifo;
extern CircularBuffer txfifo;
typedef enum status_t {
    SIZE = 1,
    DECODE = 2,
};

