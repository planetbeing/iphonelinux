#ifndef SYSCFG_H

#include "openiboot.h"

#define SCFG_PxCl 0x5078436C
#define SCFG_MtCl 0x4D74436C

int syscfg_setup();
uint8_t* syscfg_get_entry(uint32_t type, int* size);

#endif
