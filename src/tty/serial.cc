#include "serial.h"
#include "bridge/bridge.h"
#include "network/tcp.h"

#include <driver/uart.h>
#include <driver/gpio.h>
#include <system/pins.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <stdio.h>
#include "network/file_server.h"
#include <esp_log.h>
#include <esp_intr_alloc.h>
#include "nanopb/siopayload.pb.h"
#include "nanopb/pb_common.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"

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
status_t decodeStatus = SIZE;
CircularBuffer rxfifo;
CircularBuffer txfifo;
static uint8_t msglen = 0;
int uart_baud_rate = 2073600;
uart_config_t uart_config;
#define ECHO_TEST_TXD (GPIO_NUM_17)
#define ECHO_TEST_RXD (GPIO_NUM_16)
#define ECHO_UART_PORT_NUM (2)
#define BUF_SIZE (512)
uint8_t serial_data[BUF_SIZE];
uint8_t serial_byte;
static bool initialmsg = true;
void Serial_Slow()
{
    uart_baud_rate = 115200;
    uart_set_baudrate(ECHO_UART_PORT_NUM, uart_baud_rate);
}

void Serial_Fast()
{
    // uart_write_bytes(ECHO_UART_PORT_NUM, "FAST", 5);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // uart_baud_rate = 510000;
    // uart_set_baudrate(ECHO_UART_PORT_NUM, uart_baud_rate);
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

    cb_init(&rxfifo);
    cb_init(&txfifo);

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, NULL, NULL));
}

void Serial_SendData(int len, uint8_t *dataptr)
{
    uart_write_bytes(ECHO_UART_PORT_NUM, dataptr, len);
}

void Serial_TransmitFC() {
    size_t len;
    uint8_t tmpbuf[16];
    SIOPayload payload = SIOPayload_init_default;
    payload.has_data_transfer = false;
    payload.has_flow_control = true;
    payload.flow_control.dxr = dsr_state;
    payload.flow_control.xts = cts_state;
    pb_ostream_t output = pb_ostream_from_buffer(tmpbuf, sizeof(tmpbuf));
    pb_get_encoded_size(&len, SIOPayload_fields, &payload);
    pb_encode_delimited(&output, SIOPayload_fields, &payload);
    TCP_SendData(len+1, tmpbuf);
    initialmsg = false;
}

void Serial_TransmitData() {
    size_t len;
    uint8_t tmpbuf[16];
    SIOPayload payload = SIOPayload_init_default;
    payload.has_data_transfer = true;
    payload.has_flow_control = false;
    payload.data_transfer.data[1] = (pb_byte_t)serial_byte;
    pb_ostream_t output = pb_ostream_from_buffer(tmpbuf, sizeof(tmpbuf));
    pb_get_encoded_size(&len, SIOPayload_fields, &payload);
    pb_encode_delimited(&output, SIOPayload_fields, &payload);
    TCP_SendData(len + 1, tmpbuf);
}

void Serial_ProcessEvents()
{

    // Read data from the UART
    int len = 0;
    // Write data back to the UART
    if (!disable_uploads) {
        while ((len = uart_read_bytes(ECHO_UART_PORT_NUM, serial_data, BUF_SIZE, 1)) > 0) {
            TCP_SendData(len, serial_data);
        }
    } else if (connected) {
        dsr_state = gpio_get_level(kPin_DSR);
        cts_state = gpio_get_level(kPin_CTS);
        // ESP_LOGI("SIO", "dsr: %d xts: %d", dsr_state, cts_state);
        if (initialmsg) Serial_TransmitFC();
        if (prev_cts_state != cts_state || prev_dsr_state != dsr_state) {
            prev_cts_state = cts_state;
            prev_dsr_state = dsr_state;
            Serial_TransmitFC();
        }

        if ((len = uart_read_bytes(ECHO_UART_PORT_NUM, &serial_byte, 1, 1)) > 0) {
            Serial_TransmitData();
        }
    } else {
        if (!connected) {
            initialmsg = true;
            msglen = 0;
            cb_reset(&rxfifo);
        } 
    }
}





void Serial_ProcessMessage() {

}


void Serial_DecodeMessage() {
    uint8_t mybuf[64];
    cb_readbytes(&rxfifo, mybuf, msglen);
    pb_istream_t input = pb_istream_from_buffer(mybuf, msglen);
    SIOPayload payload = SIOPayload_init_default;
    if (!pb_decode(&input, SIOPayload_fields, &payload)) {
        ESP_LOGI("SIO", "Failed decode");
        cb_reset(&rxfifo);
        return;
    }
    if (!payload.has_flow_control) {
        Set_DTR(0);
        Set_RTS(0);
    } else {
        Set_DTR(payload.flow_control.dxr);
        Set_RTS(payload.flow_control.xts);
    }
    if (payload.has_data_transfer) {
        uart_write_bytes(ECHO_UART_PORT_NUM, payload.data_transfer.data, 1);
    }
}



void Serial_StateMachine() {
    if (cb_size(&rxfifo) < 1) return;
    // ESP_LOGI("SIO", "rxfifo size state machine %d", cb_size(&rxfifo));
    switch(decodeStatus) {
        case SIZE: while (cb_size(&rxfifo) >=1) {
            cb_read(&rxfifo, &msglen);
            // ESP_LOGI("SIO", "message size byte read %d", msglen);
            decodeStatus = DECODE;
        case DECODE:
            if (cb_size(&rxfifo) < msglen) return;
            Serial_DecodeMessage();
            decodeStatus = SIZE;
        }
    }
}