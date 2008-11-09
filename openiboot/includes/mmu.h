#ifndef MMU_H
#define MMU_H

#include "openiboot.h"

#define MMU_SECTION_SIZE 0x100000
#define MMU_SECTION_MASK 0xFFF00000

#define MMU_SECTION 0x2
#define MMU_AP_NOACCESS 0x0
#define MMU_AP_PRIVILEGEDONLY 0xA00
#define MMU_AP_BOTH 0xB00
#define MMU_AP_BOTHWRITE 0xC00
#define MMU_EXECUTENEVER 0x10
#define MMU_CACHEABLE 0x8
#define MMU_BUFFERABLE 0x4

extern uint32_t* CurrentPageTable;

int mmu_setup();
void mmu_enable();
void mmu_disable();
void mmu_map_section(uint32_t section, uint32_t target, Boolean cacheable, Boolean bufferable);
void mmu_map_section_range(uint32_t rangeStart, uint32_t rangeEnd, uint32_t target, Boolean cacheable, Boolean bufferable);

#endif
