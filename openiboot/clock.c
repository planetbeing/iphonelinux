#include "openiboot.h"
#include "clock.h"
#include "util.h"
#include "hardware/clock0.h"
#include "hardware/clock1.h"

uint32_t ClockPLL;
uint32_t PLLFrequencies[4];

uint32_t ClockFrequency;
uint32_t MemoryFrequency;
uint32_t BusFrequency;
uint32_t PeripheralFrequency;
uint32_t UnknownFrequency;   
uint32_t DisplayFrequency;  
uint32_t FixedFrequency;
uint32_t TimebaseFrequency;

uint32_t ClockSDiv;

uint32_t TicksPerSec;

int clock_setup() {
	uint32_t config;

	config = GET_REG(CLOCK1 + CLOCK1_CONFIG0);
	uint32_t clockPLL = CLOCK1_CLOCKPLL(config);
	uint32_t clockDivisor;

	if(CLOCK1_CLOCKHASDIVIDER(config)) {
		clockDivisor = CLOCK1_CLOCKDIVIDER(config);
	} else {
		clockDivisor = 1;
	}

	bufferPrintf("PLL %d: off.\n", clockPLL);

	uint32_t memoryPLL = CLOCK1_MEMORYPLL(config);
	uint32_t memoryDivisor;

	if(CLOCK1_MEMORYHASDIVIDER(config)) {
		memoryDivisor = CLOCK1_MEMORYDIVIDER(config);
	} else {
		memoryDivisor = 1;
	}


	config = GET_REG(CLOCK1 + CLOCK1_CONFIG1);
	uint32_t busPLL = CLOCK1_BUSPLL(config);
	uint32_t busDivisor;

	if(CLOCK1_BUSHASDIVIDER(config)) {
		busDivisor = CLOCK1_BUSDIVIDER(config);
	} else {
		busDivisor = 1;
	}


	uint32_t unknownPLL = CLOCK1_UNKNOWNPLL(config);
	uint32_t unknownDivisor;

	if(CLOCK1_UNKNOWNHASDIVIDER(config)) {
		unknownDivisor = CLOCK1_UNKNOWNDIVIDER(config);
	} else {
		unknownDivisor = 1;
	}

	uint32_t peripheralFactor = CLOCK1_PERIPHERALDIVIDER(config);

	config = GET_REG(CLOCK1 + CLOCK1_CONFIG2);
	uint32_t displayPLL = CLOCK1_DISPLAYPLL(config);
	uint32_t displayDivisor;

	if(CLOCK1_DISPLAYDIVIDER(config)) {
		displayDivisor = CLOCK1_DISPLAYHASDIVIDER(config) + 1;
	} else {
		displayDivisor = 1;
	}

	ClockPLL = clockPLL;

	/* Now we get to populate the PLLFrequencies table so we can calculate the various clock frequencies */
	int pll = 0;
	for(pll = 0; pll < NUM_PLL; pll++) {
		uint32_t pllStates = GET_REG(CLOCK1 + CLOCK1_PLLMODE);
		if(CLOCK1_PLLMODE_ONOFF(pllStates, pll)) {
			int dividerMode = CLOCK1_PLLMODE_DIVIDERMODE(pllStates, pll);
			int dividerModeInputFrequency = dividerMode;
			uint32_t inputFrequency;
			uint32_t inputFrequencyMult;
			uint32_t pllConReg;
			uint32_t pllCon;

			if(pll == 0) {
				inputFrequency = PLL0_INFREQ_DIV * 2;
				pllConReg = CLOCK1 + CLOCK1_PLL0CON;
				inputFrequencyMult = PLL0_INFREQ_MULT * 2;
			} else if(pll == 1) {
				inputFrequency = PLL1_INFREQ_DIV * 2;
				pllConReg = CLOCK1 + CLOCK1_PLL1CON;
				inputFrequencyMult = PLL1_INFREQ_MULT * 2;
			} else if(pll == 2) {
				inputFrequency = PLL2_INFREQ_DIV * 2;
				pllConReg = CLOCK1 + CLOCK1_PLL2CON;
				inputFrequencyMult = PLL2_INFREQ_MULT * 2;
			} else if(pll == 3) {
				inputFrequency = PLL3_INFREQ_DIV * 2;
				pllConReg = CLOCK1 + CLOCK1_PLL3CON;
				inputFrequencyMult = PLL3_INFREQ_MULT * 2;
				dividerMode = CLOCK1_PLLMODE_DIVIDE;
			} else {
				PLLFrequencies[pll] = 0;
				continue;
			}

			if(dividerModeInputFrequency == CLOCK1_PLLMODE_MULTIPLY) {
				inputFrequency = inputFrequencyMult;
			}

			pllCon = GET_REG(pllConReg);

			uint64_t afterMDiv = inputFrequency * CLOCK1_MDIV(pllCon);
			uint64_t afterPDiv;
			if(dividerMode == CLOCK1_PLLMODE_DIVIDE) {
				afterPDiv = afterMDiv / CLOCK1_PDIV(pllCon);
			} else {
				afterPDiv = afterMDiv * CLOCK1_PDIV(pllCon);
			}

			uint64_t afterSDiv;
			if(pll != clockPLL) {
				afterSDiv = afterPDiv / (((uint64_t)0x1) << CLOCK1_SDIV(pllCon));
			} else {
				afterSDiv = afterPDiv;
				ClockSDiv = CLOCK1_SDIV(pllCon);
			}

			PLLFrequencies[pll] = (uint32_t)afterSDiv;

			bufferPrintf("PLL %d: %d\n", pll, PLLFrequencies[pll]);
		} else {
			PLLFrequencies[pll] = 0;
			bufferPrintf("PLL %d: off.\n", pll);
			continue;
		}
	}

	ClockFrequency = PLLFrequencies[clockPLL] / clockDivisor;
	MemoryFrequency = PLLFrequencies[memoryPLL] / memoryDivisor;
	BusFrequency = PLLFrequencies[busPLL] / busDivisor;
	UnknownFrequency = PLLFrequencies[unknownPLL] / unknownDivisor;
	PeripheralFrequency = BusFrequency / (1 << peripheralFactor);
	DisplayFrequency = PLLFrequencies[displayPLL] / displayDivisor;
	FixedFrequency = FREQUENCY_BASE * 2;
	TimebaseFrequency = FREQUENCY_BASE / 2;

	TicksPerSec = FREQUENCY_BASE;

	return 0;
}

void clock_gate_switch(uint32_t gate, OnOff on_off) {
	uint32_t gate_register;
	uint32_t gate_flag;

	if(gate < CLOCK1_Separator) {
		gate_register = CLOCK1 + CLOCK1_CL2_GATES;
		gate_flag = gate;
	} else {
		gate_register = CLOCK1 + CLOCK1_CL3_GATES;
		gate_flag = gate - CLOCK1_Separator;
	}

	uint32_t gates = GET_REG(gate_register);

	if(on_off == ON) {
		gates &= ~(1 << gate_flag);
	} else {
		gates |= 1 << gate_flag;
	}

	SET_REG(gate_register, gates);

}

int clock_set_bottom_bits_38100000(Clock0ConfigCode code) {
	int bottomValue;

	switch(code) {
		case Clock0ConfigCode0:
			bottomValue = CLOCK0_CONFIG_C0VALUE;
			break;
		case Clock0ConfigCode1:
			bottomValue = CLOCK0_CONFIG_C1VALUE;
			break;
		case Clock0ConfigCode2:
			bottomValue = CLOCK0_CONFIG_C2VALUE;
			break;
		case Clock0ConfigCode3:
			bottomValue = CLOCK0_CONFIG_C3VALUE;
			break;
		default:
			return -1;
	}

	SET_REG(CLOCK0 + CLOCK0_CONFIG, (GET_REG(CLOCK0 + CLOCK0_CONFIG) & (~CLOCK0_CONFIG_BOTTOMMASK)) | bottomValue);

	return 0;
}

uint32_t clock_get_frequency(FrequencyBase freqBase) {
	switch(freqBase) {
		case FrequencyBaseClock:
			return ClockFrequency;
		case FrequencyBaseMemory:
			return MemoryFrequency;
		case FrequencyBaseBus:
			return BusFrequency;
		case FrequencyBasePeripheral:
			return PeripheralFrequency;
		case FrequencyBaseUnknown:
			return UnknownFrequency;
		case FrequencyBaseDisplay:
			return DisplayFrequency;
		case FrequencyBaseFixed:
			return FixedFrequency;
		case FrequencyBaseTimebase:
			return TimebaseFrequency;
		default:
			return 0;
	}
}

uint32_t clock_calculate_frequency(uint32_t pdiv, uint32_t mdiv, FrequencyBase freqBase) {
	unsigned int y = clock_get_frequency(freqBase) / (0x1 << ClockSDiv);
	uint64_t z = (((uint64_t) pdiv) * ((uint64_t) 1000000000)) / ((uint64_t) y);
	uint64_t divResult = ((uint64_t)(1000000 * mdiv)) / z;
	return divResult - 1;
}

static void clock0_reset_frequency() {
	SET_REG(CLOCK0 + CLOCK0_ADJ1, (GET_REG(CLOCK0 + CLOCK0_ADJ1) & CLOCK0_ADJ_MASK) | clock_calculate_frequency(0x200C, 0x40, FrequencyBaseMemory));
	SET_REG(CLOCK0 + CLOCK0_ADJ2, (GET_REG(CLOCK0 + CLOCK0_ADJ2) & CLOCK0_ADJ_MASK) | clock_calculate_frequency(0x1018, 0x4, FrequencyBaseBus));
}

void clock_set_sdiv(int sdiv) {
	int oldClockSDiv = ClockSDiv;

	if(sdiv < 1) {
		sdiv = 0;
	}

	if(sdiv <= 2) {
		ClockSDiv = sdiv;
	}

	if(oldClockSDiv < ClockSDiv) {
		clock0_reset_frequency();
	}

	uint32_t clockCon = 0;
	switch(ClockPLL) {
		case 0:
			clockCon = CLOCK1 + CLOCK1_PLL0CON;
			break;
		case 1:
			clockCon = CLOCK1 + CLOCK1_PLL1CON;
			break;
		case 2:
			clockCon = CLOCK1 + CLOCK1_PLL2CON;
			break;
		case 3:
			clockCon = CLOCK1 + CLOCK1_PLL3CON;
			break;
	}

	if(clockCon != 0) {
		SET_REG(clockCon, (GET_REG(clockCon) & ~0x7) | ClockSDiv);
	}

	if(oldClockSDiv >= ClockSDiv) {
		clock0_reset_frequency();
	}
}

