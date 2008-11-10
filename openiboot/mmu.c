#include "openiboot.h"
#include "mmu.h"
#include "hardware/s5l8900.h"
#include "hardware/arm.h"
#include "openiboot-asmhelpers.h"

uint32_t* CurrentPageTable;

static void initialize_pagetable();

int mmu_setup() {
	CurrentPageTable = (uint32_t*) PageTable;

	// Initialize the page table
	
	initialize_pagetable();

	// Implement the page table

	// Disable checking TLB permissions for domain 0, which is the only domain we're using so anyone can access
	// anywhere in memory, since we trust ourselves.
	WriteDomainAccessControlRegister(ARM11_DomainAccessControl_D0_ALL);
						
	WriteTranslationTableBaseRegister0(CurrentPageTable);
	InvalidateUnifiedTLBUnlockedEntries();
	mmu_enable();
	InvalidateUnifiedTLBUnlockedEntries();

	return 0;
}

void mmu_enable() {
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | 0x1);
}

void mmu_disable() {
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() & ~0x4);
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() & ~0x1);
}

void initialize_pagetable() {
	// Initialize the page table with a default identity mapping
	mmu_map_section_range(MemoryStart, MemoryEnd, MemoryStart, FALSE, FALSE);

	// Now we get to setup default mappings that we will need
	mmu_map_section_range(0x08000000, 0x10000000, 0x08000000, TRUE, TRUE);	// unknown, but mapped by iPhone
	mmu_map_section(AMC0, AMC0, TRUE, TRUE);

	// Make our own code cacheable and bufferable
	//mmu_map_section_range(OpenIBootLoad, (uint32_t) &OpenIBootEnd, OpenIBootLoad, TRUE, TRUE);

	// Make ROM buffer cacheable and bufferable
	mmu_map_section(ROM, ROM, TRUE, TRUE);

	// Remap exception vector so we can actually get interrupts
	//mmu_map_section(ExceptionVector, OpenIBootLoad, TRUE, TRUE);

	// Remap upper half of memory to the lower half
	mmu_map_section_range(MemoryHigher, MemoryEnd, MemoryStart, FALSE, FALSE);
}

void mmu_map_section(uint32_t section, uint32_t target, Boolean cacheable, Boolean bufferable) {
	uint32_t* sectionEntry = &CurrentPageTable[section >> 20];

	*sectionEntry =
		(target & MMU_SECTION_MASK)
		| (cacheable ? MMU_CACHEABLE : 0x0)
		| (bufferable ? MMU_BUFFERABLE : 0x0)
		| MMU_AP_BOTHWRITE		// set AP to 11 (APX is always 0, so this means R/W for everyone)
		| MMU_EXECUTENEVER
		| MMU_SECTION;			// this is a section

	//bufferPrintf("map section (%x): %x -> %x, %d %d (%x)\r\n", sectionEntry, section, target, cacheable, bufferable, *sectionEntry);

	CleanDataCacheLineMVA(sectionEntry);
	InvalidateUnifiedTLBUnlockedEntries();
}

void mmu_map_section_range(uint32_t rangeStart, uint32_t rangeEnd, uint32_t target, Boolean bufferable, Boolean cacheable) {
	uint32_t currentSection;
	uint32_t curTargetSection = target;
	Boolean started = FALSE;

	for(currentSection = rangeStart; currentSection < rangeEnd; currentSection += MMU_SECTION_SIZE) {
		if(started && currentSection == 0) {
			// We need this check because if rangeEnd is 0xFFFFFFFF, currentSection
			// will always be < rangeEnd, since we overflow the uint32.
			break;
		}
		started = TRUE;
		mmu_map_section(currentSection, curTargetSection, bufferable, cacheable);
		curTargetSection += MMU_SECTION_SIZE;
	}
}

