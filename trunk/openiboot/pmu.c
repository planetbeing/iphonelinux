#include "openiboot.h"
#include "pmu.h"
#include "hardware/pmu.h"
#include "i2c.h"
#include "timer.h"

static uint32_t GPMemCachedPresent = 0;
static uint8_t GPMemCache[PMU_MAXREG + 1];

int pmu_setup() {
	return 0;
}

int pmu_get_gpmem_reg(int bus, int reg, uint8_t* out) {
	if(reg > PMU_MAXREG)
		return -1;

	if((GPMemCachedPresent & (0x1 << reg)) == 0) {
		uint8_t registers[1];

		registers[0] = reg + 0x67;

		if(i2c_rx(bus, PMU_GETADDR, registers, 1, &GPMemCache[reg], 1) != 0) {
			return -1;
		}

		GPMemCachedPresent |= (0x1 << reg);
	}

	*out = GPMemCache[reg];

	return 0;	
}

int pmu_get_reg(int bus, int reg) {
	uint8_t registers[1];
	uint8_t out[1];

	registers[0] = reg;

	i2c_rx(bus, PMU_GETADDR, registers, 1, out, 1);
	return out[0];
}

int pmu_write_reg(int bus, int reg, int data, int verify) {
	uint8_t command[2];

	command[0] = reg;
	command[1] = data;

	i2c_tx(bus, PMU_SETADDR, command, sizeof(command));

	if(!verify)
		return 0;

	uint8_t pmuReg = reg;
	uint8_t buffer = 0;
	i2c_rx(bus, PMU_GETADDR, &pmuReg, 1, &buffer, 1);

	if(buffer == data)
		return 0;
	else
		return -1;
}

int pmu_write_regs(const PMURegisterData* regs, int num) {
	int i;
	for(i = 0; i < num; i++) {
		pmu_write_reg(regs[i].bus, regs[i].reg, regs[i].data, 1);
	}

	return 0;
}

