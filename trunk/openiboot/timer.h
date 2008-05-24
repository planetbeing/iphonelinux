#ifndef TIMER_H
#define TIMER_H

#include "openiboot.h"

int timer_setup();
int timer_stop_all();
int timer_on_off(int timer_id, OnOff on_off);


typedef struct TimerRegisters {
	uint32_t	config;
	uint32_t	state;
	uint32_t	count_buffer;
	uint32_t	unknown1;
	uint32_t	unknown2;
	uint32_t	unknown3;
} TimerRegisters;

typedef struct TimerInfo {
	Boolean		option0x40;
	uint32_t	divider;
	uint32_t	unknown1;
} TimerInfo;

extern TimerRegisters HWTimers[];

#endif
