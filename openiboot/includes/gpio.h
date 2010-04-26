#ifndef GPIO_H
#define GPIO_H

#include "openiboot.h"
#include "interrupt.h"

typedef enum
{
	GPIOPDDisabled,
	GPIOPDUp,
	GPIOPDDown
} GPIOPDSetting;

typedef struct GPIORegisters {
	volatile uint32_t CON;
	volatile uint32_t DAT;
	volatile uint32_t PUD1;	// (PUD1[x], PUD2[x]): (0, 0) = pull up/down for x disabled, (0, 1) = pull down, (1, 0) = pull up
	volatile uint32_t PUD2;
	volatile uint32_t CONSLP1;
	volatile uint32_t CONSLP2;
	volatile uint32_t PUDSLP1;
	volatile uint32_t PUDSLP2;
} GPIORegisters;

int gpio_setup();
int gpio_pin_state(int port);
void gpio_custom_io(int port, int bits);
void gpio_pin_use_as_input(int port);
void gpio_pin_output(int port, int bit);

void gpio_register_interrupt(uint32_t interrupt, int type, int level, int autoflip, InterruptServiceRoutine handler, uint32_t token);
void gpio_interrupt_enable(uint32_t interrupt);
void gpio_interrupt_disable(uint32_t interrupt);

void gpio_pulldown_configure(int port, GPIOPDSetting setting);

int pmu_gpio(int gpio, int is_output, int value);

#endif
