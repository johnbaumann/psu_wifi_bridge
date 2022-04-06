#pragma once

typedef struct mydata_t { 
    size_t len; 
    uint8_t *buf; 
}; 

void tcp_client_task(void *pvParameters);
