#ifndef MMU_H
#define MMU_H

#include "openiboot.h"

#define MMU_SECTION_SIZE 0x100000
#define MMU_SECTION_MASK 0xFFF00000

void mmu_setup();
void mmu_enable();
void mmu_map_section(uint32_t section, uint32_t target, Boolean cacheable, Boolean bufferable);
void mmu_map_section_range(uint32_t rangeStart, uint32_t rangeEnd, uint32_t target, Boolean cacheable, Boolean bufferable);

#endif
