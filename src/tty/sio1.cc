#include "sio1.h"

#include "system/pins.h"

#include <driver/gpio.h>
#include <stdbool.h>

SIO_State send_state;

bool dsr_state = false;
bool cts_state = false;
bool dtr_state = true;
bool rts_state = true;

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