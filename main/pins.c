#include "pins.h"

#include <driver/gpio.h>

const gpio_num_t kPin_DSR = GPIO_NUM_19; // IN
const gpio_num_t kPin_CTS = GPIO_NUM_21; // IN
const gpio_num_t kPin_DTR = GPIO_NUM_5; // OUT
const gpio_num_t kPin_RTS = GPIO_NUM_18; // OUT