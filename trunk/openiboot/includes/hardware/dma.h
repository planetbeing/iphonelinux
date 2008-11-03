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
#define DMACConfiguration 0x30
#define DMAC0SrcAddress 0x100
#define DMAC0DestAddress 0x104
#define DMAC0LLI 0x108
#define DMAC0Control0 0x10C
#define DMAC0Configuration 0x110

// Values
#define DMAC0_INTERRUPT 0x10
#define DMAC1_INTERRUPT 0x11
#define DMAC0_CLOCKGATE 0x19
#define DMAC1_CLOCKGATE 0x1A

#define DMA_NUMCHANNELS 8
#define DMA_NUMCONTROLLERS 2

#define DMAChannelRegSize 32
#define DMACConfiguration_ENABLE 1
#define DMAC0Control0_SWIDTHSHIFT 18
#define DMAC0Control0_DWIDTHSHIFT 21
#define DMAC0Control0_DWIDTH(x) GET_BITS(x, 21, 3)
#define DMAC0Control0_WIDTHBYTE 0
#define DMAC0Control0_WIDTHHALFWORD 1
#define DMAC0Control0_WIDTHWORD 2
#define DMAC0Control0_SBSIZESHIFT 12
#define DMAC0Control0_DBSIZESHIFT 15
#define DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE 31
#define DMAC0Control0_SOURCEAHBMASTERSELECT 24
#define DMAC0Control0_SOURCEAHBMASTERSELECT 24
#define DMAC0Control0_SOURCEINCREMENT 26
#define DMAC0Control0_DESTINATIONINCREMENT 27
#define DMAC0Control0_SIZEMASK 0xFFF

#define DMAC0Configuration_SRCPERIPHERALSHIFT 1
#define DMAC0Configuration_DESTPERIPHERALSHIFT 6
#define DMAC0Configuration_FLOWCNTRLSHIFT 11
#define DMAC0Configuration_FLOWCNTRLMASK 0x7
#define DMAC0Configuration_FLOWCNTRL_M2M 0x0
#define DMAC0Configuration_FLOWCNTRL_M2P 0x1
#define DMAC0Configuration_FLOWCNTRL_P2M 0x2
#define DMAC0Configuration_FLOWCNTRL_P2P 0x3
#define DMAC0Configuration_CHANNELENABLED (1 << 0)
#define DMAC0Configuration_TERMINALCOUNTINTERRUPTMASK (1 << 15)

#endif

