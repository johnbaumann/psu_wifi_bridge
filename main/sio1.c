#include "sio1.h"

#include <driver/gpio.h>
#include <stdbool.h>

//const gpio_num_t kPin_DSR = GPIO_NUM_5; // IN
//const gpio_num_t kPin_CTS = GPIO_NUM_18; // IN
//const gpio_num_t kPin_DTR = GPIO_NUM_19; // OUT
//const gpio_num_t kPin_RTS = GPIO_NUM_21; // OUT

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