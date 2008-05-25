#include "openiboot.h"
#include "timer.h"
#include "clock.h"
#include "s5l8900.h"
#include "interrupt.h"

TimerRegisters HWTimers[] = {
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

static void eventTimerHandler();
static void timerIRQHandler(uint32_t token);

int timer_setup() {
	/* timer needs clock signal */
	clock_gate_switch(TIMER_CLOCKGATE, ON);

	/* stop/cleanup any existing timers */
	timer_stop_all();

	/* do some voodoo */
	SET_REG(TIMER + TIMER_UNKREG0, TIMER_UNKREG0_RESET);
	SET_REG(TIMER + TIMER_UNKREG1, TIMER_UNKREG1_RESET);
	SET_REG(TIMER + TIMER_UNKREG2, TIMER_UNKREG2_RESET);
	SET_REG(TIMER + TIMER_UNKREG3, TIMER_UNKREG3_RESET);
	SET_REG(TIMER + TIMER_UNKREG4, TIMER_UNKREG4_RESET);

	int i;
	for(i = 0; i < NUM_TIMERS; i++) {
		timer_setup_clk(i, 1, 2, 0);
	}

	/* now it's time to set up the main event dispatch timer */

	// In our implementation, we set TicksPerSec when we setup the clock
	// so we don't have to do it here

	// Set the handler
	Timers[EventTimer].handler2 = eventTimerHandler;

	// Initialize the timer hardware for something that goes off once every 100 Hz.
	// The handler for the timer will reset it so it's periodic
	timer_init(EventTimer, TicksPerSec/100, 0, 0, FALSE, FALSE, FALSE);

	// Turn the timer on
	timer_on_off(EventTimer, ON);

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
	SET_REG(HWTimers[timer_id].state, TIMER_STATE_INIT);

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

void eventTimerHandler() {

}

void timerIRQHandler(uint32_t token) {

}

