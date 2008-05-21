#ifndef S5L8900_H
#define S5L8900_H

/*
 *	Constants
 */

#define OpenIBootLoad 0x18000000
#define OpenIBootEnd 0x18021980
#define HeapStart 0x18026000
#define PageTable 0x180FC000
#define ROMLocation 0x20000000

/*
 *	Devices
 */

#define PeripheralPort 0x38000000

#define VIC0 0x38E00000
#define VIC1 0x38E01000
#define EDGEIC 0x38E02000
#define AMC0 0x22000000
#define ROM 0x20000000

/*
 *	Registers
 */

// VIC
#define VICIRQSTATUS 0x000
#define VICADDRESS 0xF00

// EDGEIC
#define EDGEICLOWSTATUS 0x8
#define EDGEICHIGHSTATUS 0xC

/*
 *	Register values
 */

// ARM11
#define ARM11_CPSR_FIQDISABLE 0x40
#define ARM11_CPSR_IRQDISABLE 0x80
#define ARM11_CPSR_MODEMASK 0x1F
#define ARM11_CPSR_IRQMODE 0x12
#define ARM11_CPSR_FIQMODE 0x11
#define ARM11_CPSR_ABORTMODE 0x17
#define ARM11_CPSR_UNDEFINEDMODE 0x1B
#define ARM11_CPSR_SUPERVISORMODE 0x13

#define ARM11_AccessControl_CP0_PRIVILEGED (0x01 << 0)
#define ARM11_AccessControl_CP0_ALL (0x11 << 0)
#define ARM11_AccessControl_CP1_PRIVILEGED (0x01 << 2)
#define ARM11_AccessControl_CP1_ALL (0x11 << 2)
#define ARM11_AccessControl_CP2_PRIVILEGED (0x01 << 4)
#define ARM11_AccessControl_CP2_ALL (0x11 << 4)
#define ARM11_AccessControl_CP3_PRIVILEGED (0x01 << 6)
#define ARM11_AccessControl_CP3_ALL (0x11 << 6)
#define ARM11_AccessControl_CP4_PRIVILEGED (0x01 << 8)
#define ARM11_AccessControl_CP4_ALL (0x11 << 8)
#define ARM11_AccessControl_CP5_PRIVILEGED (0x01 << 10)
#define ARM11_AccessControl_CP5_ALL (0x11 << 10)
#define ARM11_AccessControl_CP6_PRIVILEGED (0x01 << 12)
#define ARM11_AccessControl_CP6_ALL (0x11 << 12)
#define ARM11_AccessControl_CP7_PRIVILEGED (0x01 << 14)
#define ARM11_AccessControl_CP7_ALL (0x11 << 14)
#define ARM11_AccessControl_CP8_PRIVILEGED (0x01 << 16)
#define ARM11_AccessControl_CP8_ALL (0x11 << 16)
#define ARM11_AccessControl_CP9_PRIVILEGED (0x01 << 18)
#define ARM11_AccessControl_CP9_ALL (0x11 << 18)
#define ARM11_AccessControl_CP10_PRIVILEGED (0x01 << 20)
#define ARM11_AccessControl_CP10_ALL (0x11 << 20)
#define ARM11_AccessControl_CP11_PRIVILEGED (0x01 << 22)
#define ARM11_AccessControl_CP11_ALL (0x11 << 22)
#define ARM11_AccessControl_CP12_PRIVILEGED (0x01 << 24)
#define ARM11_AccessControl_CP12_ALL (0x11 << 24)
#define ARM11_AccessControl_CP13_PRIVILEGED (0x01 << 26)
#define ARM11_AccessControl_CP13_ALL (0x11 << 26)

#define ARM11_VFP_Enable #0x40000000

#define ARM11_PeripheralPortSize0KB 0,
#define ARM11_PeripheralPortSize4KB 2
#define ARM11_PeripheralPortSize8KB 3
#define ARM11_PeripheralPortSize16KB 4
#define ARM11_PeripheralPortSize32KB 5
#define ARM11_PeripheralPortSize64KB 6
#define ARM11_PeripheralPortSize128KB 7
#define ARM11_PeripheralPortSize256KB 8
#define ARM11_PeripheralPortSize512KB 9
#define ARM11_PeripheralPortSize1MB 10
#define ARM11_PeripheralPortSize2MB 11
#define ARM11_PeripheralPortSize4MB 12
#define ARM11_PeripheralPortSize8MB 13
#define ARM11_PeripheralPortSize16MB 14
#define ARM11_PeripheralPortSize32MB 15
#define ARM11_PeripheralPortSize64MB 16
#define ARM11_PeripheralPortSize128MB 17
#define ARM11_PeripheralPortSize256MB 18
#define ARM11_PeripheralPortSize512MB 19
#define ARM11_PeripheralPortSize1GB 20
#define ARM11_PeripheralPortSize2GB 21

#define VIC_MaxInterrupt 0x40
#define VIC_InterruptSeparator 0x20

#endif
