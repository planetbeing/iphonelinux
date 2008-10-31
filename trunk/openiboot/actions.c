#include "actions.h"
#include "openiboot-asmhelpers.h"
#include "arm.h"
#include "mmu.h"

void chainload(uint32_t address) {
	EnterCriticalSection();
	arm_disable_caches();
	mmu_disable();
	CallArm(address);
}
/*
void upgrade() {
	Image*
}*/
