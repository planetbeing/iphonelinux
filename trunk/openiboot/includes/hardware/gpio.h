#ifndef HW_GPIO_H
#define HW_GPIO_H

#include "hardware/s5l8900.h"

// Device
#define GPIO_POWER 0x39A00000	/* probably a part of the system controller */

// Registers
#define POWER_GPIO_CONFIG0 0xA0
#define POWER_GPIO_CONFIG1 0xC0

// Values
#define NUM_GPIO 7
#define POWER_GPIO_CONFIG0_RESET 0
#define POWER_GPIO_CONFIG1_RESET 0xFFFFFFFF

#define GPIO_CLOCKGATE 0x2C

#endif

