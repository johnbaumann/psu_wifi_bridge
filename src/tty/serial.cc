#include "serial.h"

#include "network/tcp.h"

#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <stdio.h>
#include "network/file_server.h"
/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD (GPIO_NUM_17)
#define ECHO_TEST_RXD (GPIO_NUM_16)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM (2)
#define BUF_SIZE (1024 * 10)

int uart_baud_rate = 115200;
uint8_t serial_data[BUF_SIZE];

uart_config_t uart_config;

void Serial_Slow()
{
    uart_baud_rate = 115200;
    uart_set_baudrate(ECHO_UART_PORT_NUM, uart_baud_rate);
}

void Serial_Fast()
{
    uart_write_bytes(ECHO_UART_PORT_NUM, "FAST", 5);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    uart_baud_rate = 510000;
    uart_set_baudrate(ECHO_UART_PORT_NUM, uart_baud_rate);
}

void Serial_Init()
{
    // Configure parameters of an UART driver,
    // communication pins and install the driver
    uart_config.baud_rate = uart_baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_2;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_APB;

    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
}

void Serial_ProcessEvents()
{

    // Read data from the UART
    int len = 0;
    // Write data back to the UART

    while ((len = uart_read_bytes(ECHO_UART_PORT_NUM, serial_data, BUF_SIZE, 1)) > 0)
    {
        TCP_SendData(len, serial_data);
    }
}

void Serial_SendData(int len, uint8_t *dataptr)
{
    uart_write_bytes(ECHO_UART_PORT_NUM, dataptr, len);
}
