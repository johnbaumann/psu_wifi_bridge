#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct SIO_State
{
	bool data_updated;
	uint8_t data;
	bool dtr;
	bool rts;
} SIO_State;

extern SIO_State send_state;
extern bool dsr_state;
extern bool cts_state;
extern bool dtr_state;
extern bool rts_state;
extern bool prev_cts_state;
extern bool prev_dsr_state;


void Flow_InterruptHandler(void *arg);
void Toggle_DTR();
void Toggle_RTS();
void Set_DTR(bool value);
void Set_RTS(bool value);