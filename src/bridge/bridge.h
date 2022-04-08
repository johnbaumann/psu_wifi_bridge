#pragma once

enum BridgeMode
{
    kRaw,
    kProtocol
};

extern volatile bool disable_uploads;

void Init_Bridge();
void Protocol_Bridge_Task_Server(void *pvParameters);
void Raw_Bridge_Task_Server(void *pvParameters);