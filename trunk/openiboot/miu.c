#include "openiboot.h"
#include "miu.h"
#include "clock.h"
#include "hardware/power.h"

int miu_setup() {
	if(POWER_ID_EPOCH(*((uint8_t*)(POWER + POWER_ID))) != 3) {
		// Epoch mismatch
		return -1;
	}

	clock_set_bottom_bits_38100000(1);

	return 0;
}

