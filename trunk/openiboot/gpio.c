#include "openiboot.h"
#include "gpio.h"
#include "hardware/gpio.h"

int gpio_setup() {
	int i;
	for(i = 0; i < NUM_GPIO; i++) {
		SET_REG(GPIO_POWER + POWER_GPIO_CONFIG0, POWER_GPIO_CONFIG0_RESET);
		SET_REG(GPIO_POWER + POWER_GPIO_CONFIG1, POWER_GPIO_CONFIG1_RESET);
	}

	// iBoot also sets up interrupt handlers, but those are never unmasked

	return 0;
}

