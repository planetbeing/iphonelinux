#include "openiboot.h"
#include "timer.h"
#include "s5l8900.h"

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

	/* now it's time to set up the main event dispatch timer */

	// In our implementation, we set TicksPerSec when we setup the clock
	// so we don't have to do it here



	return 0;
}

int timer_stop_all() {
	int i;
	for(i = 0; i < NUM_TIMERS; i++) {
		timer_on_off(i, OFF);
	}
	timer_on_off(NUM_TIMERS, OFF);
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
	} else {
		/* invalid timer id */
		return -1;
	}
}
