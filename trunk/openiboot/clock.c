#include "openiboot.h"
#include "clock.h"
#include "s5l8900.h"

uint32_t ClockPLL;
uint32_t PLLFrequencies[4];

uint32_t ClockFrequency;
uint32_t MemoryFrequency;
uint32_t BusFrequency;
uint32_t PeripheralFrequency;
uint32_t UnknownFrequency;   
uint32_t Unknown2Frequency;  
uint32_t FixedFrequency;
uint32_t TimebaseFrequency;

uint32_t ClockSDiv;

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
	uint32_t unknown2PLL = CLOCK1_UNKNOWN2PLL(config);
	uint32_t unknown2Divisor;

	if(CLOCK1_UNKNOWN2DIVIDER(config)) {
		unknown2Divisor = CLOCK1_UNKNOWN2HASDIVIDER(config) + 1;
	} else {
		unknown2Divisor = 1;
	}

	ClockPLL = clockPLL;

	/* Now we get to populate the PLLFrequencies table so we can calculate the various clock frequencies */
	int pll = 0;
	for(pll = 0; pll < NUM_PLL; pll++) {
		uint32_t pllStates = GET_REG(CLOCK1 + CLOCK1_PLLMODE);
		if(!CLOCK1_PLLMODE_ONOFF(pllStates, pll)) {
			int dividerMode = CLOCK1_PLLMODE_DIVIDERMODE(pllStates, pll);
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
			} else {
				PLLFrequencies[pll] = 0;
				continue;
			}

			if(dividerMode == CLOCK1_PLLMODE_MULTIPLY) {
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

		} else {
			PLLFrequencies[pll] = 0;
			continue;
		}
	}

	ClockFrequency = PLLFrequencies[clockPLL] / clockDivisor;
	MemoryFrequency = PLLFrequencies[memoryPLL] / memoryDivisor;
	BusFrequency = PLLFrequencies[busPLL] / busDivisor;
	UnknownFrequency = PLLFrequencies[unknownPLL] / unknownDivisor;
	PeripheralFrequency = BusFrequency / (1 << peripheralFactor);
	Unknown2Frequency = PLLFrequencies[unknown2PLL] / unknown2Divisor;
	FixedFrequency = FREQUENCY_BASE * 2;
	TimebaseFrequency = FREQUENCY_BASE / 2;

	return 0;
}

void clock_gate_switch(uint32_t gate, int on_off) {
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

	if(on_off) {
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
			bottomValue = CLOCK0_CONFIG_C1VALUE;
			break;
		case Clock0ConfigCode1:
			bottomValue = CLOCK0_CONFIG_C2VALUE;
			break;
		case Clock0ConfigCode2:
			bottomValue = CLOCK0_CONFIG_C3VALUE;
			break;
		case Clock0ConfigCode3:
			bottomValue = 0;
			break;
		default:
			return -1;
	}

	SET_REG(CLOCK0 + CLOCK0_CONFIG, (GET_REG(CLOCK0 + CLOCK0_CONFIG) & CLOCK0_CONFIG_BOTTOMMASK) | bottomValue);

	return 0;
}

