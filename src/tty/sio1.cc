#include "sio1.h"

#include "system/pins.h"

#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <stdbool.h>
#include <stdint.h>

SIO_State send_state;

bool dsr_state = false;
bool cts_state = false;
bool dtr_state = true;
bool rts_state = true;

bool prev_cts_state = false;
bool prev_dsr_state = false;

void Flow_InterruptHandler(void *arg)
{
    dsr_state = gpio_get_level(kPin_DSR);
    cts_state = gpio_get_level(kPin_CTS);
}

void Toggle_DTR()
{
    dtr_state = !dtr_state;
    gpio_set_level(kPin_DTR, dtr_state);
}

void Toggle_RTS()
{
    rts_state = !rts_state;
    gpio_set_level(kPin_RTS, rts_state);
}