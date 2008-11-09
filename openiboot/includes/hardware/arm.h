#ifndef HW_ARM_H
#define HW_ARM_H

#include "hardware/s5l8900.h"

// Control registers
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


#endif

