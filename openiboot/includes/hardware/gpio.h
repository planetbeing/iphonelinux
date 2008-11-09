#ifndef HW_GPIO_H
#define HW_GPIO_H

#include "hardware/s5l8900.h"

// Device
#define GPIO_POWER 0x39A00000	/* probably a part of the system controller */
#define GPIO 0x3E400000

// Registers
#define POWER_GPIO_CONFIG0 0xA0
#define POWER_GPIO_CONFIG1 0xC0
#define GPIO_IO 0x320

// Values
#define NUM_GPIO 7
#define POWER_GPIO_CONFIG0_RESET 0
#define POWER_GPIO_CONFIG1_RESET 0xFFFFFFFF

#define GPIO_IO_MAJSHIFT 16
#define GPIO_IO_MAJMASK 0x1F
#define GPIO_IO_MINSHIFT 8
#define GPIO_IO_MINMASK 0x7
#define GPIO_IO_USHIFT 0
#define GPIO_IO_UMASK 0xF

#define GPIO_CLOCKGATE 0x2C

#endif

