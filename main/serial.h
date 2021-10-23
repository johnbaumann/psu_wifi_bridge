#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>

void Serial_SendData(int len, uint8_t *dataptr);
void Serial_Init();
void Serial_ProcessEvents();

#endif // _SERIAL_H