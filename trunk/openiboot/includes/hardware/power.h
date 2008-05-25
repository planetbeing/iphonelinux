#ifndef HW_POWER_H
#define HW_POWER_H

#include "hardware/s5l8900.h"

// Device
#define POWER 0x39A00000	/* probably a part of the system controller */

// Power
#define POWER_DEFAULT_DEVICES 0xEC
#define POWER_LCD 0x100
#define POWER_USB 0x200
#define POWER_VROM 0x1000

// Registers
#define POWER_CONFIG0 0x0
#define POWER_CONFIG1 0x20
#define POWER_CONFIG2 0x6C
#define POWER_ONCTRL 0xC
#define POWER_OFFCTRL 0x10
#define POWER_SETSTATE 0x8
#define POWER_STATE 0x14
#define POWER_ID 0x44
#define POWER_ID_EPOCH(x) ((x) >> 24)

// Values
#define POWER_CONFIG0_RESET 0x1123009
#define POWER_CONFIG1_RESET 0x20
#define POWER_CONFIG2_RESET 0x0


#endif

