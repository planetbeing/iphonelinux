#include "openiboot.h"
#include "dma.h"
#include "util.h"
#include "hardware/dma.h"
#include "clock.h"
#include "interrupt.h"

static DMARequest requests[DMA_NUMCONTROLLERS][DMA_NUMCHANNELS];

static void dispatchRequest(DMARequest *request);

static void dmaIRQHandler(uint32_t controller);

int dma_setup() {
	clock_gate_switch(DMAC0_CLOCKGATE, ON);
	clock_gate_switch(DMAC1_CLOCKGATE, ON);

	interrupt_install(DMAC0_INTERRUPT, dmaIRQHandler, 0);
	interrupt_install(DMAC1_INTERRUPT, dmaIRQHandler, 1);

	interrupt_enable(DMAC0_INTERRUPT);
	interrupt_enable(DMAC1_INTERRUPT);

	return 0;
}

static void dmaIRQHandler(uint32_t controller) {
	uint32_t intTCStatusReg;
	uint32_t intTCClearReg;
	uint32_t intTCStatus;

	if(controller == 0) {
		intTCStatusReg = DMAC0 + DMACIntTCStatus;
		intTCClearReg = DMAC0 + DMACIntTCClear;
	} else {
		intTCStatusReg = DMAC1 + DMACIntTCStatus;
		intTCClearReg = DMAC1 + DMACIntTCClear;
	}

	intTCStatus = GET_REG(intTCStatusReg);

	int channel;
	for(channel = 0; channel < DMA_NUMCHANNELS; channel++) {
		if((intTCStatusReg & (1 << channel)) != 0) {
			dispatchRequest(&requests[controller][channel]);
			SET_REG(intTCClearReg, intTCStatusReg & (1 << channel));
		}
		
	}
}

static void dispatchRequest(DMARequest *request) {
	// TODO: Implement this
}

