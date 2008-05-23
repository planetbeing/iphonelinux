#include "openiboot.h"
#include "power.h"
#include "s5l8900.h"

int power_setup() {

	SET_REG(POWER + POWER_CONFIG0, POWER_CONFIG0_RESET);
	SET_REG(POWER + POWER_CONFIG1, POWER_CONFIG1_RESET);
	SET_REG(POWER + POWER_CONFIG2, POWER_CONFIG2_RESET);

	/* turn off everything */
	int toReset = POWER_DEFAULT_DEVICES | POWER_VROM | POWER_LCD;
	SET_REG(POWER + POWER_OFFCTRL, toReset);

	/* wait for the new state to take effect */
	while((GET_REG(POWER + POWER_SETSTATE) & toReset) != (GET_REG(POWER + POWER_STATE) & toReset));

	return 0;
}
