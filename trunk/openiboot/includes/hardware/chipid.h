#ifndef HW_CHIPID_H
#define HW_CHIPID_H

#include "hardware/s5l8900.h"

// Device
#define CHIPID 0x3E500000

// Registers
#define SPICLOCKTYPE 0x4

// Values
#define GET_SPICLOCKTYPE(x) GET_BITS(x, 24, 4)

#endif

