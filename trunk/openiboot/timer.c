#include "openiboot.h"
#include "timer.h"
#include "clock.h"
#include "interrupt.h"
#include "hardware/timer.h"

const TimerRegisters HWTimers[] = {
		{	TIMER + TIMER_0 + TIMER_CONFIG, TIMER + TIMER_0 + TIMER_STATE, TIMER + TIMER_0 + TIMER_COUNT_BUFFER, 
			TIMER + TIMER_0 + TIMER_UNKNOWN1, TIMER + TIMER_0 + TIMER_UNKNOWN2, TIMER + TIMER_0 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_1 + TIMER_CONFIG, TIMER + TIMER_1 + TIMER_STATE, TIMER + TIMER_1 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_1 + TIMER_UNKNOWN1, TIMER + TIMER_1 + TIMER_UNKNOWN2, TIMER + TIMER_1 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_2 + TIMER_CONFIG, TIMER + TIMER_2 + TIMER_STATE, TIMER + TIMER_2 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_2 + TIMER_UNKNOWN1, TIMER + TIMER_2 + TIMER_UNKNOWN2, TIMER + TIMER_2 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_3 + TIMER_CONFIG, TIMER + TIMER_3 + TIMER_STATE, TIMER + TIMER_3 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_3 + TIMER_UNKNOWN1, TIMER + TIMER_3 + TIMER_UNKNOWN2, TIMER + TIMER_3 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_4 + TIMER_CONFIG, TIMER + TIMER_4 + TIMER_STATE, TIMER + TIMER_4 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_4 + TIMER_UNKNOWN1, TIMER + TIMER_4 + TIMER_UNKNOWN2, TIMER + TIMER_4 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_5 + TIMER_CONFIG, TIMER + TIMER_5 + TIMER_STATE, TIMER + TIMER_5 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_5 + TIMER_UNKNOWN1, TIMER + TIMER_5 + TIMER_UNKNOWN2, TIMER + TIMER_5 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_6 + TIMER_CONFIG, TIMER + TIMER_6 + TIMER_STATE, TIMER + TIMER_6 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_6 + TIMER_UNKNOWN1, TIMER + TIMER_6 + TIMER_UNKNOWN2, TIMER + TIMER_6 + TIMER_UNKNOWN3 }
	};

TimerInfo Timers[7];

static void timerIRQHandler(uint32_t token);
static void callTimerHandler(int timer_id, uint32_t flags);

int RTCHasInit;

static void timer_init_rtc() {
	SET_REG(TIMER + TIMER_UNKREG0, TIMER_UNKREG0_RESET1);
	SET_REG(TIMER + TIMER_UNKREG2, TIMER_UNKREG2_RESET);
	SET_REG(TIMER + TIMER_UNKREG1, TIMER_UNKREG1_RESET);
	SET_REG(TIMER + TIMER_UNKREG4, TIMER_UNKREG4_RESET);
	SET_REG(TIMER + TIMER_UNKREG3, TIMER_UNKREG3_RESET);
	SET_REG(TIMER + TIMER_UNKREG0, TIMER_UNKREG0_RESET2);
	RTCHasInit = TRUE;
}

int timer_setup() {

	/* timer needs clock signal */
	clock_gate_switch(TIMER_CLOCKGATE, ON);

	/* stop/cleanup any existing timers */
	timer_stop_all();

	/* do some voodoo */
	timer_init_rtc();

	int i;
	for(i = 0; i < NUM_TIMERS; i++) {
		timer_setup_clk(i, 1, 2, 0);
	}

	// In our implementation, event dispatch timer setup is handled by a subsequent function

	// Install the timer interrupt
	interrupt_install(TIMER_IRQ, timerIRQHandler, 1);
	interrupt_enable(TIMER_IRQ);

	return 0;
}

int timer_init(int timer_id, uint32_t interval, uint32_t unknown2, uint32_t z, Boolean option24, Boolean option28, Boolean option11) {
	if(timer_id >= NUM_TIMERS || timer_id < 0) {
		return -1;
	}

	/* need to turn it off, since we're messing with the settings */
	timer_on_off(timer_id, OFF);

	uint32_t config = 0x7000; /* set bits 12, 13, 14 */

	/* these two options are only supported on timers 4, 5, 6 */
	if(timer_id >= TIMER_Separator) {
		config |= (option24 ? (1 << 24) : 0) | (option28 ? 1 << 28: 0);
	}

	/* set the rest of the options */
	config |= (Timers[timer_id].divider << 8)
			| (z << 3)
			| (Timers[timer_id].option6 ? (1 << 6) : 0)
			| (option11 ? (1 << 11) : 0);

	SET_REG(HWTimers[timer_id].config, config);
	SET_REG(HWTimers[timer_id].count_buffer, interval);
	SET_REG(HWTimers[timer_id].unknown1, Timers[timer_id].unknown1);
	SET_REG(HWTimers[timer_id].unknown2, unknown2);

	// apply the settings
	SET_REG(HWTimers[timer_id].state, TIMER_STATE_MANUALUPDATE);

	return 0;
}

int timer_setup_clk(int timer_id, int type, int divider, uint32_t unknown1) {
	if(type == 2) {
		Timers[timer_id].option6 = FALSE;
		Timers[timer_id].divider = 6;
	} else {
		if(type == 1) {
			Timers[timer_id].option6 = TRUE;
		} else {
			Timers[timer_id].option6 = FALSE;
		}

		/* translate divider into divider code */
		switch(divider) {
			case 1:
				Timers[timer_id].divider = TIMER_DIVIDER1;
				break;
			case 2:
				Timers[timer_id].divider = TIMER_DIVIDER2;
				break;
			case 4:
				Timers[timer_id].divider = TIMER_DIVIDER4;
				break;
			case 16:
				Timers[timer_id].divider = TIMER_DIVIDER16;
				break;
			case 64:
				Timers[timer_id].divider = TIMER_DIVIDER64;
				break;
			default:
				/* invalid divider */
				return -1;
		}
	}

	Timers[timer_id].unknown1 = unknown1;

	return 0;
}

int timer_stop_all() {
	int i;
	for(i = 0; i < NUM_TIMERS; i++) {
		timer_on_off(i, OFF);
	}
	timer_on_off(NUM_TIMERS, OFF);

	return 0;
}

int timer_on_off(int timer_id, OnOff on_off) {
	if(timer_id < NUM_TIMERS) {
		if(on_off == ON) {
			SET_REG(HWTimers[timer_id].state, TIMER_STATE_START);
		} else {
			SET_REG(HWTimers[timer_id].state, TIMER_STATE_STOP);
		}

		return 0;
	} else if(timer_id == NUM_TIMERS) {
		if(on_off == ON) {
			// clear bits 0, 1, 2, 3
			SET_REG(TIMER + TIMER_UNKREG0, GET_REG(TIMER + TIMER_UNKREG0) & ~(0xF));
		} else {
			// set bits 1, 3
			SET_REG(TIMER + TIMER_UNKREG0, GET_REG(TIMER + TIMER_UNKREG0) | 0xA);
		}
		return 0;
	} else {
		/* invalid timer id */
		return -1;
	}
}

static void timerIRQHandler(uint32_t token) {
	//dump_memory(0x3e200000, 0x100);

	/* this function does not implement incrementing a counter at dword_18022B28 like Apple's */
	uint32_t stat = GET_REG(TIMER + TIMER_IRQSTAT);

	/* signal timer is being handled */
	volatile register uint32_t discard = GET_REG(TIMER + TIMER_IRQLATCH); discard --;

	if(stat & TIMER_SPECIALTIMER_BIT0) {
		SET_REG(TIMER + TIMER_UNKREG0, GET_REG(TIMER + TIMER_UNKREG0) | TIMER_SPECIALTIMER_BIT0);
	}

	if(stat & TIMER_SPECIALTIMER_BIT1) {
		SET_REG(TIMER + TIMER_UNKREG0, GET_REG(TIMER + TIMER_UNKREG0) | TIMER_SPECIALTIMER_BIT1);
	}

	int i;
	for(i = TIMER_Separator; i < NUM_TIMERS; i++) {
		callTimerHandler(i, stat >> (8 * (NUM_TIMERS - i - 1)));
	}

	/* signal timer has been handled */
	SET_REG(TIMER + TIMER_IRQLATCH, stat);
}

static void callTimerHandler(int timer_id, uint32_t flags) {
	if((flags & (1 << 2)) != 0) {
		if(Timers[timer_id].handler1)
			Timers[timer_id].handler1();
	}

	if((flags & (1 << 1)) != 0) {
		if(Timers[timer_id].handler3)
			Timers[timer_id].handler3();
	}

	if((flags & (1 << 0)) != 0) {
		if(Timers[timer_id].handler2)
			Timers[timer_id].handler2();
	}
}

uint64_t timer_get_system_microtime() {
        uint64_t ticks;
        uint64_t sec_divisor;

        timer_get_rtc_ticks(&ticks, &sec_divisor);          
        return (ticks * uSecPerSec)/sec_divisor;
}

void timer_get_rtc_ticks(uint64_t* ticks, uint64_t* sec_divisor) {
	register uint32_t ticksHigh;
	register uint32_t ticksLow;
	register uint32_t ticksHigh2;

	/* try to get a good read where the lower bits remain the same after reading the higher bits */
	do {
		ticksHigh = GET_REG(TIMER + TIMER_TICKSHIGH);
		ticksLow = GET_REG(TIMER + TIMER_TICKSLOW);
		ticksHigh2 = GET_REG(TIMER + TIMER_TICKSHIGH);
	} while(ticksHigh != ticksHigh2);

	*ticks = (((uint64_t)ticksHigh) << 32) | ticksLow;
	*sec_divisor = TicksPerSec;
}

void udelay(uint64_t delay) {
	if(!RTCHasInit) {
		return;
	}

	uint64_t startTime = timer_get_system_microtime();

	// loop while elapsed time is less than requested delay
	while((timer_get_system_microtime() - startTime) < delay);
}

int has_elapsed(uint64_t startTime, uint64_t elapsedTime) {
	if((timer_get_system_microtime() - startTime) >= elapsedTime)
		return TRUE;
	else
		return FALSE;
}

