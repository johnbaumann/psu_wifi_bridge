#include "serial.h"

#include "log.h"
#include "tcp.h"

#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

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

volatile static bool serial_toggled = false;
volatile bool serial_enabled = false;

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

void Serial_CheckToggle()
{
    if (serial_toggled)
    {
        serial_toggled = false;
        if (serial_enabled)
        {
            ESP_LOGI(kLogPrefix, "Serial Deinit");
            Serial_Deinit();
        }
        else
        {
            ESP_LOGI(kLogPrefix, "Serial Init");
            Serial_Init();
        }
    }
}

void Serial_Deinit()
{
    uart_driver_delete(ECHO_UART_PORT_NUM);
    gpio_set_direction(ECHO_TEST_RXD, GPIO_MODE_INPUT);
    gpio_set_direction(ECHO_TEST_TXD, GPIO_MODE_INPUT);
    serial_enabled = false;
}

void Serial_Init()
{
    // Configure parameters of an UART driver,
    // communication pins and install the driver
    uart_config_t uart_config = {
        .baud_rate = uart_baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    serial_enabled = true;
}

void Serial_Toggle()
{
    serial_toggled = true;
}

void Serial_ProcessEvents()
{
    // Read data from the UART
    // Write data back to TCP
    int len = 0;

    Serial_CheckToggle();

    if (serial_enabled)
    {

        while (((len = uart_read_bytes(ECHO_UART_PORT_NUM, serial_data, BUF_SIZE, 1)) > 0) && serial_enabled)
        {
            TCP_SendData(len, serial_data);
        }
    }
}

void Serial_SendData(int len, uint8_t *dataptr)
{
    if (serial_enabled)
    {
        uart_write_bytes(ECHO_UART_PORT_NUM, dataptr, len);
    }
}
