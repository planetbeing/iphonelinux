#include "openiboot.h"
#include "clock.h"
#include "s5l8900.h"

int clock_set_bottom_bits_38100000(Clock0ConfigCode code) {
	int bottomValue;

	switch(code) {
		case Clock0ConfigCode0:
			bottomValue = CLOCK0_CONFIG_C1VALUE;
			break;
		case Clock0ConfigCode1:
			bottomValue = CLOCK0_CONFIG_C2VALUE;
			break;
		case Clock0ConfigCode2:
			bottomValue = CLOCK0_CONFIG_C3VALUE;
			break;
		case Clock0ConfigCode3:
			bottomValue = 0;
			break;
		default:
			return -1;
	}

	SET_REG(CLOCK0 + CLOCK0_CONFIG, (GET_REG(CLOCK0 + CLOCK0_CONFIG) & CLOCK0_CONFIG_BOTTOMMASK) | bottomValue);

	return 0;
}

