#ifndef _SERIAL_H
#define _SERIAL_H

void Serial_SendData(int len, const void *dataptr);
void Serial_Init();
void Serial_ProcessEvents();

#endif // _SERIAL_H