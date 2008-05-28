#ifndef S5L8900_H
#define S5L8900_H

#include "hardware/s5l8900.h"

/*
 *	Macros
 */

#define GET_REG(x) (*((volatile uint32_t*)(x)))
#define SET_REG(x, y) (*((volatile uint32_t*)(x)) = (y))
#define GET_BITS(x, start, length) ((x << (32 - ((start) + (length)))) >> (32 - (length)))

/*
 *	Constants
 */

#define OpenIBootLoad 0x18000000
#define OpenIBootEnd 0x18021980
#define HeapStart 0x18026000
#define PageTable 0x180FC000
#define MemoryStart 0x00000000
#define MemoryEnd 0xFFFFFFFF
#define MemoryHigher 0x80000000
#define ExceptionVector MemoryStart

/*
 *	Devices
 */

#define PeripheralPort 0x38000000
#define AMC0 0x22000000
#define ROM 0x20000000

/*
 *	Values
 */

#define EDRAM_CLOCKGATE 0x1B

#endif

