#ifndef TIMER_H
#define TIMER_H

#include "openiboot.h"

typedef void (*TimerHandler)(void);

typedef struct TimerRegisters {
	uint32_t	config;
	uint32_t	state;
	uint32_t	count_buffer;
	uint32_t	unknown1;
	uint32_t	unknown2;
	uint32_t	cur_count;
} TimerRegisters;

typedef struct TimerInfo {
	Boolean		option6;
	uint32_t	divider;
	uint32_t	unknown1;
	TimerHandler	handler1;
	TimerHandler	handler2;
	TimerHandler	handler3;
} TimerInfo;

extern const TimerRegisters HWTimers[];
extern TimerInfo Timers[7];

int timer_setup();
int timer_stop_all();
int timer_on_off(int timer_id, OnOff on_off);
int timer_setup_clk(int timer_id, int type, int divider, uint32_t unknown1);
int timer_init(int timer_id, uint32_t interval, uint32_t unknown2, uint32_t z, Boolean option24, Boolean option28, Boolean option11);

uint64_t timer_get_system_microtime();
void timer_get_rtc_ticks(uint64_t* ticks, uint64_t* sec_divisor);

void udelay(uint64_t delay);
int has_elapsed(uint64_t startTime, uint64_t elapsedTime);

extern int RTCHasInit;

#endif

