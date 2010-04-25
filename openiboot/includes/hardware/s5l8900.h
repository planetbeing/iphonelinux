#ifndef S5L8900_H
#define S5L8900_H

#include "hardware/s5l8900.h"

/*
 *	Constants
 */

#define MemoryStart 0x00000000
#define MemoryEnd 0xFFFFFFFF
#define LargeMemoryStart 0x08000000				/* FIXME: This is an ugly hack to get around iBoot's memory rearrangement. Linux boot will only work for installed openiboot! */
#define RAMEnd 0x08000000
#define MemoryHigher 0x80000000
#define ExceptionVector MemoryStart
#ifdef SMALL
#define PageTable (OpenIBootLoad + 0x24000)
#define HeapStart (PageTable + 0x4000)
#else
#define OpenIBootLoad 0x00000000
#define GeneralStack ((PageTable - 4) + LargeMemoryStart)
#define HeapStart (LargeMemoryStart + 0x02000000)
#define PageTable (RAMEnd - 0x4000)
#endif

/*
 *	Devices
 */

#define PeripheralPort 0x38000000
#ifdef CONFIG_3G
#define AMC0 0x38500000
#define ROM 0x50000000
#else
#define AMC0 0x22000000
#define ROM 0x20000000
#endif

#define WDT_CTRL 0x3E300000
#define WDT_CNT 0x3E300004

#define WDT_INT 0x33
/*
 *	Values
 */

#define EDRAM_CLOCKGATE 0x1B
#define WDT_ENABLE 0x100000
#define WDT_PRE_SHIFT 16
#define WDT_PRE_MASK 0xF
#define WDT_CS_SHIFT 12
#define WDT_CS_MASK 0x7
#define WDT_CLR 0xA00
#define WDT_DIS 0xA5
#define WDT_INT_EN 0x8000

#define DMA_ALIGN 0x40

#endif

