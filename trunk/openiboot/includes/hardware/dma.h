#ifndef HW_DMA_H
#define HW_DMA_H

#include "hardware/s5l8900.h"

// This thing appears to be a PL080

// Device
#define DMAC0 0x38200000
#define DMAC1 0x39900000

// Registers
#define DMACIntStatus 0
#define DMACIntTCStatus 0x4
#define DMACIntTCClear 0x8

// Values
#define DMAC0_INTERRUPT 0x10
#define DMAC1_INTERRUPT 0x11
#define DMAC0_CLOCKGATE 0x19
#define DMAC1_CLOCKGATE 0x1A

#define DMA_NUMCHANNELS 8
#define DMA_NUMCONTROLLERS 2

#endif

