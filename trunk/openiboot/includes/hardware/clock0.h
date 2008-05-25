#ifndef HW_CLOCK0_H
#define HW_CLOCK0_H

#include "hardware/s5l8900.h"

#define CLOCK0 0x38100000	/* the clocks are probably also parts of the system controller */

// Registers
#define CLOCK0_CONFIG 0x0

// Values
#define CLOCK0_CONFIG_BOTTOMMASK 0x7
#define CLOCK0_CONFIG_C1VALUE 0x1
#define CLOCK0_CONFIG_C2VALUE 0x3
#define CLOCK0_CONFIG_C3VALUE 0x5

#endif

