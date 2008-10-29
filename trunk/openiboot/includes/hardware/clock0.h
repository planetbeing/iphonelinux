#ifndef HW_CLOCK0_H
#define HW_CLOCK0_H

#include "hardware/s5l8900.h"

#define CLOCK0 0x38100000	/* the clocks are probably also parts of the system controller */

// Registers
#define CLOCK0_CONFIG 0x0
#define CLOCK0_ADJ1 0x8
#define CLOCK0_ADJ2 0x404

// Values
#define CLOCK0_ADJ_MASK 0xFFFFF000

#define CLOCK0_CONFIG_BOTTOMMASK 0x7
#define CLOCK0_CONFIG_C0VALUE 0x1
#define CLOCK0_CONFIG_C1VALUE 0x3
#define CLOCK0_CONFIG_C2VALUE 0x5
#define CLOCK0_CONFIG_C3VALUE 0

#endif

