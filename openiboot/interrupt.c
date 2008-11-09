#include "openiboot.h"
#include "interrupt.h"
#include "hardware/interrupt.h"
#include "hardware/edgeic.h"
#include "util.h"
#include "openiboot-asmhelpers.h"

int interrupt_setup() {
	if((0xfff & (GET_REG(VIC0 + VICPERIPHID0) | (GET_REG(VIC0 + VICPERIPHID1) << 8) | (GET_REG(VIC0 + VICPERIPHID2) << 16) | (GET_REG(VIC0 + VICPERIPHID3) << 24))) != 0x192) {
		/* peripheral ID for vic0 doesn't match PL 192, which is the VIC we are expecting */
		return -1;
	}

	if((0xfff & (GET_REG(VIC1 + VICPERIPHID0) | (GET_REG(VIC1 + VICPERIPHID1) << 8) | (GET_REG(VIC1 + VICPERIPHID2) << 16) | (GET_REG(VIC1 + VICPERIPHID3) << 24))) != 0x192) {
		/* peripheral ID for vic1 doesn't match PL 192, which is the VIC we are expecting */
		return -1;
	}

	SET_REG(EDGEIC + EDGEICCONFIG0, EDGEIC_CONFIG0RESET);
	SET_REG(EDGEIC + EDGEICCONFIG1, EDGEIC_CONFIG1RESET);

	SET_REG(VIC0 + VICINTENCLEAR, 0xFFFFFFFF); // disable all interrupts
	SET_REG(VIC1 + VICINTENCLEAR, 0xFFFFFFFF);

	SET_REG(VIC0 + VICINTSELECT, 0); // 0 means to use IRQs, 1 means to use FIQs, so use all IRQs
	SET_REG(VIC1 + VICINTSELECT, 0);

	SET_REG(VIC0 + VICSWPRIORITYMASK, 0xffff); // unmask all 16 interrupt levels
	SET_REG(VIC1 + VICSWPRIORITYMASK, 0xffff);

	// Set interrupt vector addresses to the interrupt number. This will signal the interrupt handler to consult the handler tables
	int i;
	for(i = 0; i < VIC_InterruptSeparator; i++) {
		SET_REG(VIC0 + VICVECTADDRS + (i * 4), i);
		SET_REG(VIC1 + VICVECTADDRS + (i * 4), VIC_InterruptSeparator + i);
	}

	memset(InterruptHandlerTable, 0, sizeof(InterruptHandlerTable));

	return 0;
}

int interrupt_install(int irq_no, InterruptServiceRoutine handler, uint32_t token) {
	if(irq_no >= VIC_MaxInterrupt) {
		return -1;
	}

	EnterCriticalSection();
	InterruptHandlerTable[irq_no].handler = handler;
	InterruptHandlerTable[irq_no].token = token;
	InterruptHandlerTable[irq_no].useEdgeIC = 0;
	LeaveCriticalSection();

	return 0;
}

int interrupt_enable(int irq_no) {
	if(irq_no >= VIC_MaxInterrupt) {
		return -1;
	}

	EnterCriticalSection();
	if(irq_no < VIC_InterruptSeparator) {
		SET_REG(VIC0 + VICINTENABLE, GET_REG(VIC0 + VICINTENABLE) | (1 << irq_no));
	} else {
		SET_REG(VIC1 + VICINTENABLE, GET_REG(VIC1 + VICINTENABLE) | (1 << (irq_no - VIC_InterruptSeparator)));
	}
	LeaveCriticalSection();

	return 0;
}

int interrupt_disable(int irq_no) {
	if(irq_no >= VIC_MaxInterrupt) {
		return -1;
	}

	EnterCriticalSection();
	if(irq_no < VIC_InterruptSeparator) {
		SET_REG(VIC0 + VICINTENABLE, GET_REG(VIC0 + VICINTENABLE) & ~(1 << irq_no));
	} else {
		SET_REG(VIC1 + VICINTENABLE, GET_REG(VIC1 + VICINTENABLE) & ~(1 << (irq_no - VIC_InterruptSeparator)));
	}
	LeaveCriticalSection();

	return 0;
}

