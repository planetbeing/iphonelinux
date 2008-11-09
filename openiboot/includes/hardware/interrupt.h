#ifndef HW_INTERRUPT_H
#define HW_INTERRUPT_H

#include "hardware/s5l8900.h"

// This appears to be a PL192

// Constants

#define VIC_MaxInterrupt 0x40
#define VIC_InterruptSeparator 0x20

// Devices

#define VIC0 0x38E00000
#define VIC1 0x38E01000

// Registers

#define VICIRQSTATUS 0x000
#define VICRAWINTR 0x8
#define VICINTSELECT 0xC
#define VICINTENABLE 0x10
#define VICINTENCLEAR 0x14
#define VICSWPRIORITYMASK 0x24
#define VICVECTADDRS 0x100
#define VICADDRESS 0xF00
#define VICPERIPHID0 0xFE0
#define VICPERIPHID1 0xFE4
#define VICPERIPHID2 0xFE8
#define VICPERIPHID3 0xFEC

#endif

