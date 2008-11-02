#include "openiboot.h"
#include "nand.h"
#include "hardware/nand.h"
#include "timer.h"
#include "clock.h"
#include "util.h"

static int banksTable[NAND_NUM_BANKS];

static int NandSetting = 0;
static const uint8_t NANDSetting1 = 7;
static const uint8_t NANDSetting2 = 7;
static int NumValidBanks = 0;
static const int NANDBankResetSetting = 1;

static const NANDDeviceType SupportedDevices[] = {
	{0x2555D5EC, 8192, 0x80, 4, 64, 0x2040204, 7744, 4, 6},
	{0xB614D5EC, 4096, 0x80, 8, 128, 0x2040204, 3872, 4, 6},
	{0xB655D7EC, 8192, 0x80, 8, 128, 0x2040204, 7744, 4, 6},
	{0xA514D3AD, 4096, 0x80, 4, 64, 0x2040204, 3872, 4, 6},
	{0xA555D5AD, 8192, 0x80, 4, 64, 0x2040204, 7744, 4, 6},
	{0xA585D598, 8320, 0x80, 4, 64, 0x2040206, 7744, 4, 6},
	{0xBA94D598, 4096, 0x80, 8, 216, 0x2040206, 3872, 8, 8},
	{0xBA95D798, 8192, 0x80, 8, 216, 0x2040206, 7744, 8, 8},
	{0x3ED5D789, 8192, 0x80, 8, 216, 0x2040204, 7744, 8, 8},
	{0x3E94D589, 4096, 0x80, 8, 216, 0x2040204, 3872, 8, 8},
	{0x3ED5D72C, 8192, 0x80, 8, 216, 0x2040204, 7744, 8, 8},
	{0x3E94D52C, 4096, 0x80, 8, 216, 0x2040204, 3872, 8, 8},
	{0}
};

static int wait_for_ready(int timeout) {
	if((GET_REG(NAND + NAND_STATUS) & NAND_STATUS_READY) != 0) {
		return 0;
	}

	uint32_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + NAND_STATUS) & NAND_STATUS_READY) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	return 0;
}

static int wait_for_status_bit_2(int timeout) {
	if((GET_REG(NAND + NAND_STATUS) & (1 << 2)) != 0) {
		SET_REG(NAND + NAND_STATUS, 1 << 2);
		return 0;
	}

	uint32_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + NAND_STATUS) & (1 << 2)) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + NAND_STATUS, 1 << 2);

	return 0;
}

static int wait_for_status_bit_3(int timeout) {
	if((GET_REG(NAND + NAND_STATUS) & (1 << 3)) != 0) {
		SET_REG(NAND + NAND_STATUS, 1 << 3);
		return 0;
	}

	uint32_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + NAND_STATUS) & (1 << 3)) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + NAND_STATUS, 1 << 3);

	return 0;
}

static int bank_reset_helper(int bank, int timeout) {
	uint32_t startTime = timer_get_system_microtime();
	if(NANDBankResetSetting)
		bank = 0;
	else
		bank &= 0xffff;

	uint32_t toTest = 1 << (bank + 4);

	while((GET_REG(NAND + NAND_STATUS) & toTest) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + NAND_STATUS, toTest);

	return 0;
}

static int bank_reset(int bank, int timeout) {
	SET_REG(NAND + NAND_CONFIG,
			((NANDSetting1 & NAND_CONFIG_SETTING1MASK) << NAND_CONFIG_SETTING1SHIFT) | ((NANDSetting2 & NAND_CONFIG_SETTING2MASK) << NAND_CONFIG_SETTING2SHIFT)
			| (1 << (banksTable[bank] + 1)) | NAND_CONFIG_DEFAULTS);

	SET_REG(NAND + NAND_CONFIG2, NAND_CONFIG2_RESET);

	int ret = wait_for_ready(timeout);
	if(ret == 0) {
		ret = bank_reset_helper(bank, timeout);
		udelay(1000);
		return ret;
	} else {
		udelay(1000);
		return ret;
	}
}

int nand_setup() {
	clock_gate_switch(NAND_CLOCK_GATE1, ON);
	clock_gate_switch(NAND_CLOCK_GATE2, ON);

	int bank;
	for(bank = 0; bank < NAND_NUM_BANKS; bank++) {
		banksTable[bank] = bank;
	}

	NumValidBanks = 0;
	const NANDDeviceType* nandType = NULL;

	SET_REG(NAND + NAND_SETUP, 0);
	SET_REG(NAND + NAND_SETUP, GET_REG(NAND + NAND_SETUP) | (NandSetting << 4));

	for(bank = 0; bank < NAND_NUM_BANKS; bank++) {
		bank_reset(bank, 100);

		SET_REG(NAND + NAND_CON, NAND_CON_SETTING1);
		SET_REG(NAND + NAND_CONFIG,
			((NANDSetting1 & NAND_CONFIG_SETTING1MASK) << NAND_CONFIG_SETTING1SHIFT) | ((NANDSetting2 & NAND_CONFIG_SETTING2MASK) << NAND_CONFIG_SETTING2SHIFT)
			| (1 << (banksTable[bank] + 1)) | NAND_CONFIG_DEFAULTS);

		SET_REG(NAND + NAND_CONFIG2, NAND_CONFIG2_SETTING1);

		wait_for_ready(500);

		SET_REG(NAND + NAND_CONFIG4, 0);
		SET_REG(NAND + NAND_CONFIG3, 0);
		SET_REG(NAND + NAND_CON, (1 << 0));

		wait_for_status_bit_2(500);
		bank_reset_helper(bank, 100);

		SET_REG(NAND + NAND_CONFIG5, (1 << 3));
		SET_REG(NAND + NAND_CON, (1 << 1));

		wait_for_status_bit_3(500);
		uint32_t id = GET_REG(NAND + NAND_ID);
		const NANDDeviceType* candidate = SupportedDevices;
		while(candidate->id != 0) {
			if(candidate->id == id) {
				if(nandType == NULL) {
					nandType = candidate;
				} else if(nandType != candidate) {
					bufferPrintf("nand: Mismatched device IDs (0x%08x after 0x%08x)\r\n", id, nandType->id);
					return ERROR_ARG;
				}
				banksTable[NumValidBanks++] = bank;
			}
			candidate++;
		}

		SET_REG(NAND + NAND_CON, NAND_CON_SETTING1);
	}

	if(nandType == NULL) {
		bufferPrintf("nand: No supported NAND found\r\n");
		return ERROR_ARG;
	}

	return 0;
}

