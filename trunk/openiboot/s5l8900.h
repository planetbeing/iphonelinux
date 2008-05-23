#ifndef S5L8900_H
#define S5L8900_H

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

#define NUM_PLL 4
#define FREQUENCY_BASE 12000000
#define PLL0_INFREQ_DIV FREQUENCY_BASE /* 12 MHz */
#define PLL1_INFREQ_DIV FREQUENCY_BASE /* 12 MHz */
#define PLL2_INFREQ_DIV FREQUENCY_BASE /* 12 MHz */
#define PLL3_INFREQ_DIV 13500000 /* 13.5 MHz */
#define PLL0_INFREQ_MULT 0x8000
#define PLL1_INFREQ_MULT 0x8000
#define PLL2_INFREQ_MULT 0x8000
#define PLL3_INFREQ_MULT FREQUENCY_BASE

/*
 *	Devices
 */

#define PeripheralPort 0x38000000

#define VIC0 0x38E00000
#define VIC1 0x38E01000
#define EDGEIC 0x38E02000
#define POWER 0x39A00000	/* probably a part of the system controller */
#define AMC0 0x22000000
#define ROM 0x20000000
#define CLOCK0 0x38100000	/* the clocks are probably also parts of the system controller */
#define CLOCK1 0x3C500000

/*
 *	Registers
 */

// VIC
#define VICIRQSTATUS 0x000
#define VICADDRESS 0xF00

// EDGEIC
#define EDGEICLOWSTATUS 0x8
#define EDGEICHIGHSTATUS 0xC

// POWER
#define POWER_CONFIG0 0x0
#define POWER_CONFIG1 0x20
#define POWER_CONFIG2 0x6C
#define POWER_ONCTRL 0xC
#define POWER_OFFCTRL 0x10
#define POWER_SETSTATE 0x8
#define POWER_STATE 0x14
#define POWER_ID 0x44

// CLOCK0
#define CLOCK0_CONFIG 0x0

// CLOCK1
#define CLOCK1_CONFIG0 0x0
#define CLOCK1_CONFIG1 0x4
#define CLOCK1_CONFIG2 0x8
#define CLOCK1_PLL0CON 0x20
#define CLOCK1_PLL1CON 0x24
#define CLOCK1_PLL2CON 0x28
#define CLOCK1_PLL3CON 0x2C

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

#define ARM11_Control_INSTRUCTIONCACHE 0x1000
#define ARM11_Control_DATACACHE 0x4
#define ARM11_Control_BRANCHPREDICTION 0x800
#define ARM11_Control_RETURNSTACK 0x1
#define ARM11_Control_STRICTALIGNMENTCHECKING 0x2
#define ARM11_Control_UNALIGNEDDATAACCESS 0x400000

#define ARM11_AuxControl_RETURNSTACK 0x1
#define ARM11_AuxControl_DYNAMICBRANCHPREDICTION 0x2
#define ARM11_AuxControl_STATICBRANCHPREDICTION 0x4

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

#define ARM11_DomainAccessControl_D0_PRIVILEGED (0x01 << 0)
#define ARM11_DomainAccessControl_D0_ALL (0x11 << 0)
#define ARM11_DomainAccessControl_D1_PRIVILEGED (0x01 << 2)
#define ARM11_DomainAccessControl_D1_ALL (0x11 << 2)
#define ARM11_DomainAccessControl_D2_PRIVILEGED (0x01 << 4)
#define ARM11_DomainAccessControl_D2_ALL (0x11 << 4)
#define ARM11_DomainAccessControl_D3_PRIVILEGED (0x01 << 6)
#define ARM11_DomainAccessControl_D3_ALL (0x11 << 6)
#define ARM11_DomainAccessControl_D4_PRIVILEGED (0x01 << 8)
#define ARM11_DomainAccessControl_D4_ALL (0x11 << 8)
#define ARM11_DomainAccessControl_D5_PRIVILEGED (0x01 << 10)
#define ARM11_DomainAccessControl_D5_ALL (0x11 << 10)
#define ARM11_DomainAccessControl_D6_PRIVILEGED (0x01 << 12)
#define ARM11_DomainAccessControl_D6_ALL (0x11 << 12)
#define ARM11_DomainAccessControl_D7_PRIVILEGED (0x01 << 14)
#define ARM11_DomainAccessControl_D7_ALL (0x11 << 14)
#define ARM11_DomainAccessControl_D8_PRIVILEGED (0x01 << 16)
#define ARM11_DomainAccessControl_D8_ALL (0x11 << 16)
#define ARM11_DomainAccessControl_D9_PRIVILEGED (0x01 << 18)
#define ARM11_DomainAccessControl_D9_ALL (0x11 << 18)
#define ARM11_DomainAccessControl_D10_PRIVILEGED (0x01 << 20)
#define ARM11_DomainAccessControl_D10_ALL (0x11 << 20)
#define ARM11_DomainAccessControl_D11_PRIVILEGED (0x01 << 22)
#define ARM11_DomainAccessControl_D11_ALL (0x11 << 22)
#define ARM11_DomainAccessControl_D12_PRIVILEGED (0x01 << 24)
#define ARM11_DomainAccessControl_D12_ALL (0x11 << 24)
#define ARM11_DomainAccessControl_D13_PRIVILEGED (0x01 << 26)
#define ARM11_DomainAccessControl_D13_ALL (0x11 << 26)
#define ARM11_DomainAccessControl_D14_PRIVILEGED (0x01 << 28)
#define ARM11_DomainAccessControl_D14_ALL (0x11 << 28)
#define ARM11_DomainAccessControl_D15_PRIVILEGED (0x01 << 28)
#define ARM11_DomainAccessControl_D15_ALL (0x11 << 28)

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

// VIC
#define VIC_MaxInterrupt 0x40
#define VIC_InterruptSeparator 0x20

#define POWER_ID_EPOCH(x) GET_BITS((x), 24, 8)

// Clock0

#define CLOCK0_CONFIG_BOTTOMMASK 0x7
#define CLOCK0_CONFIG_C1VALUE 0x1
#define CLOCK0_CONFIG_C2VALUE 0x3
#define CLOCK0_CONFIG_C3VALUE 0x5

// Clock1
#define CLOCK1_CLOCKPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_CLOCKDIVIDER(x) (GET_BITS((x), 0, 4) + 1)
#define CLOCK1_CLOCKHASDIVIDER(x) GET_BITS((x), 8, 1)

#define CLOCK1_MEMORYPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_MEMORYDIVIDER(x) (GET_BITS((x), 16, 4) + 1)
#define CLOCK1_MEMORYHASDIVIDER(x) GET_BITS((x), 24, 1)

#define CLOCK1_BUSPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_BUSDIVIDER(x) (GET_BITS((x), 16, 4) + 1)
#define CLOCK1_BUSHASDIVIDER(x) GET_BITS((x), 24, 1)

#define CLOCK1_UNKNOWNPLL(x) GET_BITS((x), 12, 2)
#define CLOCK1_UNKNOWNDIVIDER1(x) (GET_BITS((x), 0, 4) + 1)
#define CLOCK1_UNKNOWNDIVIDER2(x) (GET_BITS((x), 4, 4) + 1)
#define CLOCK1_UNKNOWNDIVIDER(x) (CLOCK1_UNKNOWNDIVIDER1(x) * CLOCK1_UNKNOWNDIVIDER2(x))
#define CLOCK1_UNKNOWNHASDIVIDER(x) GET_BITS((x), 8, 1)

#define CLOCK1_PERIPHERALDIVIDER(x) GET_BITS((x), 9, 3)

#define CLOCK1_UNKNOWN2PLL(x) GET_BITS((x), 28, 3)
#define CLOCK1_UNKNOWN2DIVIDER(x) GET_BITS((x), 16, 4)
#define CLOCK1_UNKNOWN2HASDIVIDER(x) GET_BITS((x), 24, 1)

#define CLOCK1_PLLMODE 0x44
#define CLOCK1_PLLMODE_ONOFF(x, y) (((x) >> (y)) & 0x1)
#define CLOCK1_PLLMODE_DIVIDERMODE(x, y) (((x) >> (y + 4)) & 0x1)
#define CLOCK1_PLLMODE_DIVIDE 0
#define CLOCK1_PLLMODE_MULTIPLY 1

#define CLOCK1_MDIV(x) (((x) >> 8) & 0x3FF)
#define CLOCK1_PDIV(x) (((x) >> 24) & 0x3F)
#define CLOCK1_SDIV(x) ((x) & 0x3)


// Power
#define POWER_DEFAULT_DEVICES 0xEC
#define POWER_LCD 0x100
#define POWER_USB 0x200
#define POWER_VROM 0x1000

#define POWER_CONFIG0_RESET 0x1123009
#define POWER_CONFIG1_RESET 0x20
#define POWER_CONFIG2_RESET 0x0

#endif
