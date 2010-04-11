#include "openiboot.h"
#include "dma.h"
#include "util.h"
#include "hardware/dma.h"
#include "clock.h"
#include "timer.h"
#include "interrupt.h"
#include "openiboot-asmhelpers.h"

static const int ControllerLookupTable[] = {
	1, 1, 1, 1, 1,
	1, 1, 1, 1, 1,
	2, 2, 2, 2, 2,
	2, 2, 2, 2, 3,
	3, 3, 3, 3, 3,
	3
};

static const uint32_t AddressLookupTable[] = {
	0x3CC00024, 0x3CC00020, 0x3CC08024, 0x3CC08020, 0x3CC0C024,
	0x3CC0C020, 0x3CC10024, 0x3CC10020, 0x38A00080, 0x38300040,
	0x3CE00020, 0x3CE00010, 0x3D200020, 0x3D200010, 0x3CD00038,
	0x3CD00010, 0x3D400038, 0x3D400010, 0x3CB00010, 0x3CA00038,
	0x3CA00010, 0x3C300020, 0x3C300010, 0x3CC04024, 0x3CC04020,
	0
};

static const uint32_t PeripheralLookupTable[][DMA_NUMCONTROLLERS] = {
	{0x07, 0x10}, {0x06, 0x10}, {0x0B, 0x10}, {0x0A, 0x10}, {0x0D, 0x10},
	{0x0C, 0x10}, {0x0F, 0x10}, {0x0E, 0x10}, {0x02, 0x10}, {0x03, 0x10},
	{0x10, 0x0D}, {0x10, 0x0C}, {0x10, 0x0F}, {0x10, 0x0E}, {0x10, 0x03},
	{0x10, 0x02}, {0x10, 0x05}, {0x10, 0x04}, {0x10, 0x06}, {0x01, 0x01},
	{0x00, 0x00}, {0x05, 0x0B}, {0x04, 0x0A}, {0x09, 0x09}, {0x08, 0x08},
	{0x00, 0x00}
};

static volatile DMARequest requests[DMA_NUMCONTROLLERS][DMA_NUMCHANNELS];
static DMALinkedList* DMALists[DMA_NUMCONTROLLERS][DMA_NUMCHANNELS];

static DMALinkedList StaticDMALists[DMA_NUMCONTROLLERS][DMA_NUMCHANNELS];

static void dispatchRequest(volatile DMARequest *request, int controller, int channel);

static void dmaIRQHandler(uint32_t controller);

static volatile int Controller0FreeChannels[DMA_NUMCHANNELS] = {0};
static volatile int Controller1FreeChannels[DMA_NUMCHANNELS] = {0};

int dma_setup() {
	clock_gate_switch(DMAC0_CLOCKGATE, ON);
	clock_gate_switch(DMAC1_CLOCKGATE, ON);

	interrupt_install(DMAC0_INTERRUPT, dmaIRQHandler, 1);
	interrupt_install(DMAC1_INTERRUPT, dmaIRQHandler, 2);

	interrupt_enable(DMAC0_INTERRUPT);
	interrupt_enable(DMAC1_INTERRUPT);

	return 0;
}

static void dmaIRQHandler(uint32_t controller) {
	uint32_t intTCStatusReg;
	uint32_t intTCClearReg;
	uint32_t intTCStatus;

	if(controller == 1) {
		intTCStatusReg = DMAC0 + DMACIntTCStatus;
		intTCClearReg = DMAC0 + DMACIntTCClear;
	} else {
		intTCStatusReg = DMAC1 + DMACIntTCStatus;
		intTCClearReg = DMAC1 + DMACIntTCClear;
	}

	intTCStatus = GET_REG(intTCStatusReg);

	int channel;
	for(channel = 0; channel < DMA_NUMCHANNELS; channel++) {
		if((intTCStatus & (1 << channel)) != 0) {
			dispatchRequest(&requests[controller - 1][channel], controller, channel);
			SET_REG(intTCClearReg, 1 << channel);
		}
		
	}
}

static int getFreeChannel(int* controller, int* channel) {
	int i;

	EnterCriticalSection();

	if((*controller & (1 << 0)) != 0) {
		for(i = 0; i < DMA_NUMCHANNELS; i++) {
			if(Controller0FreeChannels[i] == 0) {
				*controller = 1;
				*channel = i;
				Controller0FreeChannels[i] = 1;
				LeaveCriticalSection();
				return 0;
			}
		}
	}

	if((*controller & (1 << 1)) != 0) {
		for(i = 0; i < DMA_NUMCHANNELS; i++) {
			if(Controller1FreeChannels[i] == 0) {
				*controller = 2;
				*channel = i;
				Controller1FreeChannels[i] = 1;
				LeaveCriticalSection();
				return 0;
			}
		}
	}

	LeaveCriticalSection();
	return ERROR_BUSY;
}

int dma_request(int Source, int SourceTransferWidth, int SourceBurstSize, int Destination, int DestinationTransferWidth, int DestinationBurstSize, int* controller, int* channel, DMAHandler handler) {
	if(*controller == 0) {
		*controller = ControllerLookupTable[Source] & ControllerLookupTable[Destination];

		if(*controller == 0) {
			return ERROR_DMA;
		}

		while(getFreeChannel(controller, channel) == ERROR_BUSY);
		requests[*controller - 1][*channel].started = TRUE;
		requests[*controller - 1][*channel].done = FALSE;
		requests[*controller - 1][*channel].handler = handler;
	}

	uint32_t DMACControl;
	uint32_t DMACConfig;

	if(*controller == 1) {
		DMACControl = DMAC0 + DMAC0Control0 + (*channel * DMAChannelRegSize);
		DMACConfig = DMAC0 + DMACConfiguration;
	} else if(*controller == 2) {
		DMACControl = DMAC1 + DMAC0Control0 + (*channel * DMAChannelRegSize);
		DMACConfig = DMAC1 + DMACConfiguration;
	} else {
		return ERROR_DMA;
	}

	uint32_t config = 0;

	SET_REG(DMACConfig, DMACConfiguration_ENABLE);

	config |= (SourceTransferWidth / 2) << DMAC0Control0_SWIDTHSHIFT; // 4 -> 2, 2 -> 1, 1 -> 0
	config |= (DestinationTransferWidth / 2) << DMAC0Control0_DWIDTHSHIFT; // 4 -> 2, 2 -> 1, 1 -> 0

	int x;
	for(x = 0; x < 7; x++) {
		if((1 << (x + 1)) >= SourceBurstSize) {
			config |= x << DMAC0Control0_SBSIZESHIFT;
			break;
		}
	}
	for(x = 0; x < 7; x++) {
		if((1 << (x + 1)) >= DestinationBurstSize) {
			config |= x << DMAC0Control0_DBSIZESHIFT;
			break;
		}
	}

	config |= 1 << DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE;
	config |= 1 << DMAC0Control0_SOURCEAHBMASTERSELECT;

	SET_REG(DMACControl, config);

	return 0;
}

int dma_perform(uint32_t Source, uint32_t Destination, int size, int continueList, int* controller, int* channel) {
	uint32_t regSrcAddress;
	uint32_t regDestAddress;
	uint32_t regLLI;
	uint32_t regControl0;
	uint32_t regConfiguration;

	const uint32_t regControl0Mask = ~(DMAC0Control0_SIZEMASK | DMAC0Control0_SOURCEINCREMENT | DMAC0Control0_DESTINATIONINCREMENT);

	uint32_t regOffset = (*channel * DMAChannelRegSize);

	if(*controller == 1) {
		regOffset += DMAC0;
	} else if(*controller == 2) {
		regOffset += DMAC1;
	}

	regSrcAddress = regOffset + DMAC0SrcAddress;
	regDestAddress = regOffset + DMAC0DestAddress;
	regLLI = regOffset + DMAC0LLI;
	regControl0 = regOffset + DMAC0Control0;
	regConfiguration = regOffset + DMAC0Configuration;

	int transfers = size/(1 << DMAC0Control0_DWIDTH(GET_REG(regControl0)));

	uint32_t sourcePeripheral = 0;
	uint32_t destPeripheral = 0;
	uint32_t flowControl = 0;
	uint32_t sourceIncrement = 0;
	uint32_t destinationIncrement = 0;

	if(Source <= (sizeof(AddressLookupTable)/sizeof(uint32_t))) {
		if(Destination <= (sizeof(AddressLookupTable)/sizeof(uint32_t))) {
			SET_REG(regSrcAddress, AddressLookupTable[Source]);
			SET_REG(regDestAddress, AddressLookupTable[Destination]);
			sourcePeripheral = PeripheralLookupTable[Source][*controller - 1];
			destPeripheral = PeripheralLookupTable[Destination][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_P2P;
		} else {
			SET_REG(regSrcAddress, AddressLookupTable[Source]);
			SET_REG(regDestAddress, Destination);
			sourcePeripheral = PeripheralLookupTable[Source][*controller - 1];
			destPeripheral = PeripheralLookupTable[DMA_MEMORY][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_P2M;
			destinationIncrement = 1 << DMAC0Control0_DESTINATIONINCREMENT;
		}
	} else {
		if(Destination <= (sizeof(AddressLookupTable)/sizeof(uint32_t))) {
			SET_REG(regSrcAddress, Source);
			SET_REG(regDestAddress, AddressLookupTable[Destination]);
			sourcePeripheral = PeripheralLookupTable[DMA_MEMORY][*controller - 1];
			destPeripheral = PeripheralLookupTable[Destination][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_M2P;
			sourceIncrement = 1 << DMAC0Control0_SOURCEINCREMENT;
		} else {
			SET_REG(regSrcAddress, Source);
			SET_REG(regDestAddress, Destination);
			sourcePeripheral = PeripheralLookupTable[DMA_MEMORY][*controller - 1];
			destPeripheral = PeripheralLookupTable[DMA_MEMORY][*controller - 1];
			flowControl =  DMAC0Configuration_FLOWCNTRL_M2M;
			sourceIncrement = 1 << DMAC0Control0_SOURCEINCREMENT;
			destinationIncrement = 1 << DMAC0Control0_DESTINATIONINCREMENT;
		}
	}

	if(!continueList) {
		uint32_t src = GET_REG(regSrcAddress);
		uint32_t dest = GET_REG(regDestAddress);
		if(transfers > 0xFFF) {
			SET_REG(regControl0, GET_REG(regControl0) & ~(1 << DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE));

			if(DMALists[*controller - 1][*channel])
				free(DMALists[*controller - 1][*channel]);

			DMALinkedList* item = DMALists[*controller - 1][*channel] = malloc(((transfers + 0xDFF) / 0xE00) * sizeof(DMALinkedList));
			SET_REG(regLLI, (uint32_t)item);
			do {
				transfers -= 0xE00;
				
				if(sourceIncrement != 0) {
					src += 0xE00 << DMAC0Control0_DWIDTH(GET_REG(regControl0));
				}
				if(destinationIncrement != 0) {
					dest += 0xE00 << DMAC0Control0_DWIDTH(GET_REG(regControl0));
				}

				item->source = src;
				item->destination = dest;

				if(transfers <= 0xE00) {
					item->control = destinationIncrement | sourceIncrement | (regControl0Mask & GET_REG(regControl0)) | (transfers & DMAC0Control0_SIZEMASK)
						| (1 << DMAC0Control0_TERMINALCOUNTINTERRUPTENABLE);
					item->next = NULL;
				} else {
					item->control = destinationIncrement | sourceIncrement | (regControl0Mask & GET_REG(regControl0)) | 0xE00;
					item->next = item + 1;
					item = item->next;
				}
			} while(transfers > 0xE00);

			CleanAndInvalidateCPUDataCache();
			transfers = 0xE00;
		} else {
			SET_REG(regLLI, 0);
		}
	} else {
		StaticDMALists[*controller - 1][*channel].control = GET_REG(regControl0);
		SET_REG(regLLI, (uint32_t)&StaticDMALists[*controller - 1][*channel]);
	}

	SET_REG(regControl0, (GET_REG(regControl0) & regControl0Mask) | destinationIncrement | sourceIncrement | (transfers & DMAC0Control0_SIZEMASK));
	SET_REG(regConfiguration, DMAC0Configuration_CHANNELENABLED | DMAC0Configuration_TERMINALCOUNTINTERRUPTMASK
			| (flowControl << DMAC0Configuration_FLOWCNTRLSHIFT)
			| (destPeripheral << DMAC0Configuration_DESTPERIPHERALSHIFT)
			| (sourcePeripheral << DMAC0Configuration_SRCPERIPHERALSHIFT));

	return 0;
}

int dma_finish(int controller, int channel, int timeout) {
	uint64_t startTime = timer_get_system_microtime();
	while(!requests[controller - 1][channel].done) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return -1;
		}
	}

	EnterCriticalSection();
	requests[controller - 1][channel].started = FALSE;
	requests[controller - 1][channel].done = FALSE;
	if(controller == 1)
		Controller0FreeChannels[channel] = 0;
	else if(controller == 2)
		Controller1FreeChannels[channel] = 0;
	LeaveCriticalSection();

	return 0;
}

int dma_shutdown()
{
	SET_REG(DMAC0 + DMACConfiguration, ~DMACConfiguration_ENABLE);
	SET_REG(DMAC1 + DMACConfiguration, ~DMACConfiguration_ENABLE);
	return 0;
}

uint32_t dma_dstpos(int controller, int channel) {
	uint32_t regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	return GET_REG(regOffset + DMAC0DestAddress);
}

uint32_t dma_srcpos(int controller, int channel) {
	uint32_t regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	return GET_REG(regOffset + DMAC0SrcAddress);
}

void dma_pause(int controller, int channel) {
	uint32_t regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	SET_REG(regOffset + DMAC0Configuration, GET_REG(regOffset + DMAC0Configuration) & ~DMAC0Configuration_CHANNELENABLED);
}

void dma_resume(int controller, int channel) {
	uint32_t regOffset = channel * DMAChannelRegSize;

	if(controller == 1) {
		regOffset += DMAC0;
	} else if(controller == 2) {
		regOffset += DMAC1;
	}

	SET_REG(regOffset + DMAC0Configuration, GET_REG(regOffset + DMAC0Configuration) | DMAC0Configuration_CHANNELENABLED);
}


static void dispatchRequest(volatile DMARequest *request, int controller, int channel) {
	// TODO: Implement this
	request->done = TRUE;
	if(request->handler)
		request->handler(1, controller, channel);
}

