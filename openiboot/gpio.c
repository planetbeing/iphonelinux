#include "openiboot.h"
#include "gpio.h"
#include "hardware/gpio.h"
#include "clock.h"

static GPIORegisters* GPIORegs;

int gpio_setup() {
	int i;

	GPIORegs = (GPIORegisters*) GPIO;

	for(i = 0; i < NUM_GPIO; i++) {
		SET_REG(GPIO_POWER + POWER_GPIO_CONFIG0, POWER_GPIO_CONFIG0_RESET);
		SET_REG(GPIO_POWER + POWER_GPIO_CONFIG1, POWER_GPIO_CONFIG1_RESET);
	}

	// iBoot also sets up interrupt handlers, but those are never unmasked
	
	clock_gate_switch(GPIO_CLOCKGATE, ON);

	return 0;
}

int gpio_pin_state(int port) {
	return ((GPIORegs[GET_BITS(port, 8, 5)].DAT & (1 << GET_BITS(port, 0, 3))) != 0);
}

void gpio_custom_io(int port, int bits) {
	SET_REG(GPIO + GPIO_IO, ((GET_BITS(port, 8, 5) & GPIO_IO_MAJMASK) << GPIO_IO_MAJSHIFT)
				| ((GET_BITS(port, 0, 3) & GPIO_IO_MINMASK) << GPIO_IO_MINSHIFT)
				| ((bits & GPIO_IO_UMASK) << GPIO_IO_USHIFT));
}

void gpio_pin_reset(int port) {
	gpio_custom_io(port, 0);
}

void gpio_pin_output(int port, int bit) {
	gpio_custom_io(port, 0xE | bit); // 0b111U, where U is the argument
}

int gpio_detect_configuration() {
	static int hasDetected = FALSE;
	static int detectedConfig = 0;

	if(hasDetected) {
		return detectedConfig;
	}

	detectedConfig = (gpio_pin_state(GPIO_DETECT3) ? 1 : 0) | ((gpio_pin_state(GPIO_DETECT2) ? 1 : 0) << 1) | ((gpio_pin_state(GPIO_DETECT1) ? 1 : 0) << 2);
	hasDetected = TRUE;
	return detectedConfig;
}

