#include "tcp.h"

#include "log.h"
#include "serial.h"

#include <stdbool.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <esp_intr_alloc.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define MAX_TIMEOUT_SECONDS 10

#define PORT 6699
#define KEEPALIVE_IDLE 5
#define KEEPALIVE_INTERVAL 5
#define KEEPALIVE_COUNT 3

int tcp_sock;
int listen_sock;
char addr_str[128];

int keepAlive = 1;
int keepIdle = KEEPALIVE_IDLE;
int keepInterval = KEEPALIVE_INTERVAL;
int keepCount = KEEPALIVE_COUNT;

uint8_t rx_buffer[1024];

void TCP_Cleanup()
{
    close(listen_sock);
    vTaskDelete(NULL);
}

void TCP_CloseSocket()
{
    shutdown(tcp_sock, 0);
    close(tcp_sock);
    tcp_sock = 0;
}

void TCP_Init()
{
    struct sockaddr_storage dest_addr;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(PORT);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0)
    {
        ESP_LOGE(kLogPrefix, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(kLogPrefix, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(kLogPrefix, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(kLogPrefix, "IPPROTO: %d", AF_INET);
        TCP_Cleanup();
    }
    ESP_LOGI(kLogPrefix, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(kLogPrefix, "Error occurred during listen: errno %d", errno);
        TCP_Cleanup();
    }
}

void TCP_SendData(int len, void *dataptr)
{
    int to_write = len;
    if (tcp_sock <= 0)
        return;

    while (to_write > 0)
    {
        int written = send(tcp_sock, dataptr + (len - to_write), to_write, 0);
        if (written < 0)
        {
            ESP_LOGE(kLogPrefix, "Error occurred during sending: errno %d", errno);
        }
        to_write -= written;
    }
}

bool TCP_Retransmit()
{
    int len;

    do
    {
        len = recv(tcp_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(kLogPrefix, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(kLogPrefix, "Connection closed");
        }
        else
        {
            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            //TCP_SendData(len, rx_buffer);
            Serial_SendData(len, rx_buffer);

            printf("PC: ");
            for (int i = 0; i < len; i++)
            {
                printf("0x%2X", rx_buffer[i]);
            }
            printf("\n");
        }
    } while (len > 0);

    return true;
}

bool TCP_ProcessEvents()
{
    ESP_LOGI(kLogPrefix, "Socket listening");

    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);
    tcp_sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (tcp_sock < 0)
    {
        ESP_LOGE(kLogPrefix, "Unable to accept connection: errno %d", errno);
        return false;
    }

    // Set tcp keepalive option
    setsockopt(tcp_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(tcp_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(tcp_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(tcp_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
    // Convert ip address to string
    if (source_addr.ss_family == PF_INET)
    {
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    }

    ESP_LOGI(kLogPrefix, "Socket accepted ip address: %s", addr_str);

    TCP_Retransmit();

    TCP_CloseSocket();

    return true;
}

void TCP_Task_Server(void *pvParameters)
{
    TCP_Init();

    while (1)
    {
        if (TCP_ProcessEvents() == false)
            break;

        TCP_CloseSocket();            
    }
    
    TCP_Cleanup();    
}