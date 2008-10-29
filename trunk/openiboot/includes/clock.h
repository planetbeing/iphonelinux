#ifndef CLOCK_H
#define CLOCK_H

#include "openiboot.h"
#include "hardware/clock1.h"

typedef enum Clock0ConfigCode {
	Clock0ConfigCode0 = 0,
	Clock0ConfigCode1 = 1,
	Clock0ConfigCode2 = 2,
	Clock0ConfigCode3 = 3
} Clock0ConfigCode;

extern uint32_t ClockPLL;
extern uint32_t PLLFrequencies[NUM_PLL];

extern uint32_t ClockFrequency;
extern uint32_t MemoryFrequency;
extern uint32_t BusFrequency;
extern uint32_t PeripheralFrequency;
extern uint32_t UnknownFrequency;
extern uint32_t DisplayFrequency;
extern uint32_t FixedFrequency;
extern uint32_t TimebaseFrequency;

extern uint32_t ClockSDiv;

extern uint32_t TicksPerSec;

typedef enum FrequencyBase {
	FrequencyBaseClock,
	FrequencyBaseMemory,
	FrequencyBaseBus,
	FrequencyBasePeripheral,
	FrequencyBaseUnknown,
	FrequencyBaseDisplay,
	FrequencyBaseFixed,
	FrequencyBaseTimebase
} FrequencyBase;

int clock_set_bottom_bits_38100000(Clock0ConfigCode code);
int clock_setup();
void clock_gate_switch(uint32_t gate, OnOff on_off);
uint32_t clock_get_frequency(FrequencyBase freqBase);
uint32_t clock_calculate_frequency(uint32_t pdiv, uint32_t mdiv, FrequencyBase freqBase);
void clock_set_sdiv(int sdiv);

#endif
