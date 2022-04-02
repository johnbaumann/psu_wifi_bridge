#ifndef SIO1_H
#define SIO1_H

#include <driver/gpio.h>

#define kPin_DSR GPIO_NUM_19 // IN
#define kPin_CTS GPIO_NUM_21 // IN
#define kPin_DTR GPIO_NUM_5  // OUT
#define kPin_RTS GPIO_NUM_18 // OUT

extern bool dsr_state;
extern bool cts_state;
extern bool dtr_state;
extern bool rts_state;

void Toggle_DTR();
void Toggle_RTS();

#endif // SIO1_H