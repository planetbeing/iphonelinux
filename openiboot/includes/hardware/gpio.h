#ifndef HW_GPIO_H
#define HW_GPIO_H

#include "hardware/s5l8900.h"

// gpioicBase
// Device
#define GPIOIC 0x39A00000	/* probably a part of the system controller */

// gpioBaseAddress
#define GPIO 0x3E400000

// Registers
#define GPIO_INTLEVEL 0x80
#define GPIO_INTSTAT 0xA0
#define GPIO_INTEN 0xC0
#define GPIO_INTTYPE 0xE0
#define GPIO_FSEL 0x320

// Values
#define GPIO_NUMINTGROUPS 7
#define GPIO_INTSTAT_RESET 0xFFFFFFFF
#define GPIO_INTEN_RESET 0

#define GPIO_FSEL_MAJSHIFT 16
#define GPIO_FSEL_MAJMASK 0x1F
#define GPIO_FSEL_MINSHIFT 8
#define GPIO_FSEL_MINMASK 0x7
#define GPIO_FSEL_USHIFT 0
#define GPIO_FSEL_UMASK 0xF

#define GPIO_CLOCKGATE 0x2C

#endif

