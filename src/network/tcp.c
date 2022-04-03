#include "tcp.h"

#include "system/log.h"
#include "tty/serial.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_intr_alloc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <sdkconfig.h>
#include <stdbool.h>
#include <stdio.h>

#define ADDRESS ("0.0.0.0")
#define PORT ("6699")
#define INVALID_SOCK (-1)

const size_t max_socks = 1;
static uint8_t rx_buffer[1024];
struct addrinfo *address_info;
int listen_sock = INVALID_SOCK;
static int sock[1];
int test_sock = INVALID_SOCK;
int flags;

static void log_socket_error(const char *tag, const int sock, const int err, const char *message)
{
    ESP_LOGE(tag, "[sock=%d]: %s\n"
                  "error=%d: %s",
             sock, message, err, strerror(err));
}

static int try_receive(const char *tag, const int sock, uint8_t *data, size_t max_len)
{
    int len = recv(sock, data, max_len, 0);
    if (len < 0)
    {
        if (errno == EINPROGRESS || errno == EAGAIN /*|| errno == EWOULDBLOCK*/)
        {
            return 0; // Not an error
        }
        if (errno == ENOTCONN || errno == ECONNRESET)
        {
            ESP_LOGW(tag, "[sock=%d]: Connection closed", sock);
            return -2; // Socket has been disconnected
        }
        if (errno = 58)
        {
            ESP_LOGW(tag, "[sock=%d]: Unknown error", sock);
            return -3; // Unknown error, connection closed non-gracefully?
        }
        log_socket_error(tag, sock, errno, "Error occurred during receiving");
        return -1;
    }

    return len;
}

static inline char *get_clients_address(struct sockaddr_storage *source_addr)
{
    static char address_str[128];
    char *res = NULL;
    // Convert ip address to string
    if (source_addr->ss_family == PF_INET)
    {
        res = inet_ntoa_r(((struct sockaddr_in *)source_addr)->sin_addr, address_str, sizeof(address_str) - 1);
    }

    if (!res)
    {
        address_str[0] = '\0'; // Returns empty string if conversion didn't succeed
    }
    return address_str;
}

void TCP_SendData(int len, void *dataptr)
{
    int to_write = len;
 
   if (test_sock == INVALID_SOCK)
   {
       return; // Not connected
   }

    while (to_write > 0)
    {
        if (test_sock == INVALID_SOCK)
        {
            log_socket_error(kLogPrefix, test_sock, errno, "INVALID_SOCK during sending");
            return;
        }

        int written = send(test_sock, dataptr + (len - to_write), to_write, 0);
        if (written < 0 && errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            log_socket_error(kLogPrefix, test_sock, errno, "Error occurred during sending");
            //return -1;
            return;
        }
        to_write -= written;
    }
}

bool TCP_Init()
{
    struct addrinfo hints = {.ai_socktype = SOCK_STREAM};

    // Prepare a list of file descriptors to hold client's sockets, mark all of them as invalid, i.e. available
    for (int i = 0; i < max_socks; ++i)
    {
        sock[i] = INVALID_SOCK;
        test_sock = INVALID_SOCK;
    }

    // Translating the hostname or a string representation of an IP to address_info
    int res = getaddrinfo(ADDRESS, PORT, &hints, &address_info);
    if (res != 0 || address_info == NULL)
    {
        ESP_LOGE(kLogPrefix, "couldn't get hostname for `%s` "
                             "getaddrinfo() returns %d, addrinfo=%p",
                 ADDRESS, res, address_info);
        return false;
    }

    // Creating a listener socket
    listen_sock = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);

    if (listen_sock < 0)
    {
        log_socket_error(kLogPrefix, listen_sock, errno, "Unable to create socket");
        return false;
    }
    ESP_LOGI(kLogPrefix, "Listener socket created");

    // Marking the socket as non-blocking
    flags = fcntl(listen_sock, F_GETFL);
    if (fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        log_socket_error(kLogPrefix, listen_sock, errno, "Unable to set socket non blocking");
        return false;
    }
    ESP_LOGI(kLogPrefix, "Socket marked as non blocking");

    // Binding socket to the given address
    int err = bind(listen_sock, address_info->ai_addr, address_info->ai_addrlen);
    if (err != 0)
    {
        log_socket_error(kLogPrefix, listen_sock, errno, "Socket unable to bind");
        return false;
    }
    ESP_LOGI(kLogPrefix, "Socket bound on %s:%s", ADDRESS, PORT);

    // Set queue (backlog) of pending connections to one (can be more)
    err = listen(listen_sock, 1);
    if (err != 0)
    {
        log_socket_error(kLogPrefix, listen_sock, errno, "Error occurred during listen");
        return false;
    }
    ESP_LOGI(kLogPrefix, "Socket listening");

    return true;
}

void TCP_Cleanup()
{
    if (listen_sock != INVALID_SOCK)
    {
        close(listen_sock);
    }

    for (int i = 0; i < max_socks; ++i)
    {
        if (sock[i] != INVALID_SOCK)
        {
            close(sock[i]);
        }
    }

    free(address_info);
}

bool TCP_ProcessEvents()
{
    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);

    // Find a free socket
    int new_sock_index = 0;
    for (new_sock_index = 0; new_sock_index < max_socks; ++new_sock_index)
    {
        if (sock[new_sock_index] == INVALID_SOCK)
        {
            break;
        }
    }

    // We accept a new connection only if we have a free socket
    if (new_sock_index < max_socks)
    {
        // Try to accept a new connections
        sock[new_sock_index] = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);

        if (sock[new_sock_index] < 0)
        {
            if (errno == EWOULDBLOCK)
            { // The listener socket did not accepts any connection
                // continue to serve open connections and try to accept again upon the next iteration
                ESP_LOGV(kLogPrefix, "No pending connections...");
            }
            else
            {
                log_socket_error(kLogPrefix, listen_sock, errno, "Error when accepting connection");
                return false;
            }
        }
        else
        {
            // We have a new client connected -> print it's address
            ESP_LOGI(kLogPrefix, "[sock=%d]: Connection accepted from IP:%s", sock[new_sock_index], get_clients_address(&source_addr));
            test_sock = sock[new_sock_index];

            // ...and set the client's socket non-blocking
            flags = fcntl(sock[new_sock_index], F_GETFL);
            if (fcntl(sock[new_sock_index], F_SETFL, flags | O_NONBLOCK) == -1)
            {
                log_socket_error(kLogPrefix, sock[new_sock_index], errno, "Unable to set socket non blocking");
                return false;
            }
            ESP_LOGI(kLogPrefix, "[sock=%d]: Socket marked as non blocking", sock[new_sock_index]);
        }
    }

    // We serve all the connected clients in this loop
    for (int i = 0; i < max_socks; ++i)
    {
        if (sock[i] != INVALID_SOCK)
        {

            // This is an open socket -> try to serve it
            int len = try_receive(kLogPrefix, sock[i], rx_buffer, sizeof(rx_buffer));
            if (len < 0)
            {
                // Error occurred within this client's socket -> close and mark invalid
                ESP_LOGI(kLogPrefix, "[sock=%d]: try_receive() returned %d -> closing the socket", sock[i], len);
                close(sock[i]);
                sock[i] = INVALID_SOCK;
                test_sock = INVALID_SOCK;
            }
            else if (len > 0)
            {
                // Received some data -> echo back
                //ESP_LOGI(kLogPrefix, "[sock=%d]: Received %.*s", sock[i], len, rx_buffer);

                Serial_SendData(len, rx_buffer);
            }

        } // one client's socket
    }     // for all sockets

    return true;
}
