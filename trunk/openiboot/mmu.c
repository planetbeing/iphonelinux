#include "openiboot.h"
#include "mmu.h"
#include "s5l8900.h"

uint32_t* CurrentPageTable;

static void initialize_pagetable();

void mmu_setup() {
	CurrentPageTable = (uint32_t*) PageTable;

	initialize_pagetable();

	// Implement the TLB

	WriteDomainAccessControlRegister(0x3);	// Disable checking TLB permissions for domain 0, which is the only domain we're using
						// So anyone can access anywhere in memory, since we trust ourselves.
	WriteTranslationTableBaseRegister0(CurrentPageTable);
	InvalidateUnifiedTLBUnlockedEntries();
	mmu_enable();
	InvalidateUnifiedTLBUnlockedEntries();
}

void mmu_enable() {
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | 0x1);
}

void initialize_pagetable() {
	// Initialize the page table with a default identity mapping
	mmu_map_section_range(0x00000000, 0xFFFFFFFF, 0x00000000, FALSE, FALSE);

	// Now we get to setup default mappings that we will need
	mmu_map_section_range(0x08000000, 0x10000000, 0x08000000, TRUE, TRUE);
	mmu_map_section(AMC0, AMC0, TRUE, TRUE);

	// Make our own code cacheable and bufferable
	mmu_map_section_range(OpenIBootLoad, OpenIBootEnd, 0x18000000, TRUE, TRUE);

	// Make ROM buffer cacheable and bufferable
	mmu_map_section(ROM, ROM, TRUE, TRUE);

	// Remap exception vector so we can actually get interrupts
	mmu_map_section(0x00000000, OpenIBootLoad, TRUE, TRUE);

	// Remap upper half of memory to the lower half
	mmu_map_section_range(0x80000000, 0xFFFFFFFF, 0x00000000, FALSE, FALSE);
}

void mmu_map_section(uint32_t section, uint32_t target, Boolean cacheable, Boolean bufferable) {
	CurrentPageTable[section >> 20] =
		(target & MMU_SECTION_MASK)
		| (cacheable ? 0x8 : 0x0)
		| (bufferable ? 0x4 : 0x0)
		| 0xC0				// set AP to 11 (APX is always 0, so this means R/W for everyone)
		| 0x10				//
		| 0x02;				// this is a section
}

void mmu_map_section_range(uint32_t rangeStart, uint32_t rangeEnd, uint32_t target, Boolean bufferable, Boolean cacheable) {
	uint32_t currentSection;
	uint32_t curTargetSection = target;
	for(currentSection = 0; currentSection < rangeEnd; currentSection += MMU_SECTION_SIZE) {
		mmu_map_section(currentSection, curTargetSection, bufferable, cacheable);
		curTargetSection += MMU_SECTION_SIZE;
	}
}
