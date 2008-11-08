#include "openiboot.h"
#include "nand.h"
#include "hardware/nand.h"
#include "timer.h"
#include "clock.h"
#include "util.h"
#include "openiboot-asmhelpers.h"
#include "dma.h"
#include "hardware/interrupt.h"

static int banksTable[NAND_NUM_BANKS];

static int ECCType = 0;
static uint8_t NANDSetting1;
static uint8_t NANDSetting2;
static uint8_t NANDSetting3;
static uint8_t NANDSetting4;
static uint32_t TotalECCDataSize;
static uint32_t ECCType2;
static int NumValidBanks = 0;
static const int NANDBankResetSetting = 1;
static int LargePages;

static NANDData Data;
static UnknownNANDType Data2;

static uint8_t* aTemporaryReadEccBuf;
static uint8_t* aTemporarySBuf;

#define SECTOR_SIZE 512

static const NANDDeviceType SupportedDevices[] = {
	{0x2555D5EC, 8192, 0x80, 4, 64, 4, 2, 4, 2, 7744, 4, 6},
	{0xB614D5EC, 4096, 0x80, 8, 128, 4, 2, 4, 2, 3872, 4, 6},
	{0xB655D7EC, 8192, 0x80, 8, 128, 4, 2, 4, 2, 7744, 4, 6},
	{0xA514D3AD, 4096, 0x80, 4, 64, 4, 2, 4, 2, 3872, 4, 6},
	{0xA555D5AD, 8192, 0x80, 4, 64, 4, 2, 4, 2, 7744, 4, 6},
	{0xA585D598, 8320, 0x80, 4, 64, 6, 2, 4, 2, 7744, 4, 6},
	{0xBA94D598, 4096, 0x80, 8, 216, 6, 2, 4, 2, 3872, 8, 8},
	{0xBA95D798, 8192, 0x80, 8, 216, 6, 2, 4, 2, 7744, 8, 8},
	{0x3ED5D789, 8192, 0x80, 8, 216, 4, 2, 4, 2, 7744, 8, 8},
	{0x3E94D589, 4096, 0x80, 8, 216, 4, 2, 4, 2, 3872, 8, 8},
	{0x3ED5D72C, 8192, 0x80, 8, 216, 4, 2, 4, 2, 7744, 8, 8},
	{0x3E94D52C, 4096, 0x80, 8, 216, 4, 2, 4, 2, 3872, 8, 8},
	{0}
};

static int wait_for_ready(int timeout) {
	if((GET_REG(NAND + NAND_STATUS) & NAND_STATUS_READY) != 0) {
		return 0;
	}

	uint64_t startTime = timer_get_system_microtime();
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

	uint64_t startTime = timer_get_system_microtime();
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

	uint64_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + NAND_STATUS) & (1 << 3)) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + NAND_STATUS, 1 << 3);

	return 0;
}

static int bank_reset_helper(int bank, int timeout) {
	uint64_t startTime = timer_get_system_microtime();
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

	SET_REG(NAND + NAND_CMD, NAND_CMD_RESET);

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

static int bank_setup(int bank) {
	SET_REG(NAND + NAND_CONFIG,
			((NANDSetting1 & NAND_CONFIG_SETTING1MASK) << NAND_CONFIG_SETTING1SHIFT) | ((NANDSetting2 & NAND_CONFIG_SETTING2MASK) << NAND_CONFIG_SETTING2SHIFT)
			| (1 << (banksTable[bank] + 1)) | NAND_CONFIG_DEFAULTS);

	uint32_t toTest = 1 << (bank + 4);
	if((GET_REG(NAND + NAND_STATUS) & toTest) != 0) {
		SET_REG(NAND + NAND_STATUS, toTest);
	}

	SET_REG(NAND + NAND_CON, NAND_CON_SETTING1); 
	SET_REG(NAND + NAND_CMD, NAND_CMD_SETTING2);
	wait_for_ready(500);

	uint64_t startTime = timer_get_system_microtime();
	while(TRUE) {
		SET_REG(NAND + NAND_TRANSFERSIZE, 0);
		SET_REG(NAND + NAND_CON, NAND_CON_BEGINTRANSFER);

		if(wait_for_status_bit_3(500) != 0) {
			bufferPrintf("nand: bank_setup: wait for status bit 3 timed out\r\n");
			return ERROR_TIMEOUT;
		}


		uint32_t data = GET_REG(NAND + NAND_DMA_SOURCE);
		SET_REG(NAND + NAND_CON, NAND_CON_SETTING2);
		if((data & (1 << 6)) == 0) {
			if(has_elapsed(startTime, 500 * 1000)) {
				bufferPrintf("nand: bank_setup: wait for bit 6 of DMA timed out\r\n");
				return ERROR_TIMEOUT;
			}
		} else {
			break;
		}
	}

	SET_REG(NAND + NAND_CMD, 0);
	wait_for_ready(500);
	return 0;
}

int nand_setup() {
	NANDSetting1 = 7;
	NANDSetting2 = 7;
	NANDSetting3 = 7;
	NANDSetting4 = 7;

	bufferPrintf("nand: Probing flash controller...\r\n");

	clock_gate_switch(NAND_CLOCK_GATE1, ON);
	clock_gate_switch(NAND_CLOCK_GATE2, ON);

	int bank;
	for(bank = 0; bank < NAND_NUM_BANKS; bank++) {
		banksTable[bank] = bank;
	}

	NumValidBanks = 0;
	const NANDDeviceType* nandType = NULL;

	SET_REG(NAND + NAND_SETUP, 0);
	SET_REG(NAND + NAND_SETUP, GET_REG(NAND + NAND_SETUP) | (ECCType << 4));

	for(bank = 0; bank < NAND_NUM_BANKS; bank++) {
		bank_reset(bank, 100);

		SET_REG(NAND + NAND_CON, NAND_CON_SETTING1);
		SET_REG(NAND + NAND_CONFIG,
			((NANDSetting1 & NAND_CONFIG_SETTING1MASK) << NAND_CONFIG_SETTING1SHIFT) | ((NANDSetting2 & NAND_CONFIG_SETTING2MASK) << NAND_CONFIG_SETTING2SHIFT)
			| (1 << (banksTable[bank] + 1)) | NAND_CONFIG_DEFAULTS);

		SET_REG(NAND + NAND_CMD, NAND_CMD_ID);

		wait_for_ready(500);

		SET_REG(NAND + NAND_CONFIG4, 0);
		SET_REG(NAND + NAND_CONFIG3, 0);
		SET_REG(NAND + NAND_CON, NAND_CON_SETUPTRANSFER);

		wait_for_status_bit_2(500);
		bank_reset_helper(bank, 100);

		SET_REG(NAND + NAND_TRANSFERSIZE, 8);
		SET_REG(NAND + NAND_CON, NAND_CON_BEGINTRANSFER);

		wait_for_status_bit_3(500);
		uint32_t id = GET_REG(NAND + NAND_DMA_SOURCE);
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

	Data.DeviceID = nandType->id;

	NANDSetting2 = (((clock_get_frequency(FrequencyBaseBus) * (nandType->NANDSetting2 + 1)) + 99999999)/100000000) - 1;
	NANDSetting1 = (((clock_get_frequency(FrequencyBaseBus) * (nandType->NANDSetting1 + 1)) + 99999999)/100000000) - 1;
	NANDSetting3 = (((clock_get_frequency(FrequencyBaseBus) * (nandType->NANDSetting3 + 1)) + 99999999)/100000000) - 1;
	NANDSetting4 = (((clock_get_frequency(FrequencyBaseBus) * (nandType->NANDSetting4 + 1)) + 99999999)/100000000) - 1;

	if(NANDSetting2 > 7)
		NANDSetting2 = 7;

	if(NANDSetting1 > 7)
		NANDSetting1 = 7;

	if(NANDSetting3 > 7)
		NANDSetting3 = 7;

	if(NANDSetting4 > 7)
		NANDSetting4 = 7;

	Data.blocksPerBank = nandType->blocksPerBank;
	Data.banksTotal = NumValidBanks;
	Data.sectorsPerPage = nandType->sectorsPerPage;
	Data.userSubBlksTotal = nandType->userSubBlksTotal;
	Data.bytesPerSpare = nandType->bytesPerSpare;
	Data.field_2E = 4;
	Data.field_2F = 3;
	Data.pagesPerBlock = nandType->pagesPerBlock;

	if(Data.sectorsPerPage >= 4) {
		LargePages = TRUE;
	} else {
		LargePages = FALSE;
	}

	if(nandType->ecc1 == 6) {
		ECCType = 4;
		TotalECCDataSize = Data.sectorsPerPage * 15;
	} else if(nandType->ecc1 == 8) {
		ECCType = 8;
		TotalECCDataSize = Data.sectorsPerPage * 20;
	} else if(nandType->ecc1 == 4) {
		ECCType = 0;
		TotalECCDataSize = Data.sectorsPerPage * 10;
	}

	if(nandType->ecc2 == 6) {
		ECCType2 = 4;
	} else if(nandType->ecc2 == 8) {
		ECCType2 = 8;
	} else if(nandType->ecc2 == 4) {
		ECCType2 = 0;
	}

	Data.field_4 = 5;
	Data.bytesPerPage = SECTOR_SIZE * Data.sectorsPerPage;
	Data.pagesPerBank = Data.pagesPerBlock * Data.blocksPerBank;
	Data.pagesTotal = Data.pagesPerBank * Data.banksTotal;
	Data.pagesPerSubBlk = Data.pagesPerBlock * Data.banksTotal;
	Data.userPagesTotal = Data.userSubBlksTotal * Data.pagesPerSubBlk;
	Data.subBlksTotal = (Data.banksTotal * Data.blocksPerBank) / Data.banksTotal;

	Data2.field_2 = Data.subBlksTotal - Data.userSubBlksTotal - 28;
	Data2.field_4 = 4 + Data2.field_2;
	Data2.field_6 = 3;
	Data2.field_8 = 23;
	if(Data2.field_8 == 0)
		Data.field_22 = 0;

	int bits = 0;
	int i = Data2.field_8;
	while((i <<= 1) != 0) {
		bits++;
	}

	Data.field_22 = bits;

	bufferPrintf("nand: DEVICE: %08x\r\n", Data.DeviceID);
	bufferPrintf("nand: BANKS_TOTAL: %d\r\n", Data.banksTotal);
	bufferPrintf("nand: BLOCKS_PER_BANK: %d\r\n", Data.blocksPerBank);
	bufferPrintf("nand: SUBLKS_TOTAL: %d\r\n", Data.subBlksTotal);
	bufferPrintf("nand: USER_SUBLKS_TOTAL: %d\r\n", Data.userSubBlksTotal);
	bufferPrintf("nand: PAGES_PER_SUBLK: %d\r\n", Data.pagesPerSubBlk);
	bufferPrintf("nand: PAGES_PER_BANK: %d\r\n", Data.pagesPerBank);
	bufferPrintf("nand: SECTORS_PER_PAGE: %d\r\n", Data.sectorsPerPage);
	bufferPrintf("nand: BYTES_PER_SPARE: %d\r\n", Data.bytesPerSpare);
	bufferPrintf("nand: BYTES_PER_PAGE: %d\r\n", Data.bytesPerPage);

	aTemporaryReadEccBuf = (uint8_t*) malloc(Data.bytesPerPage);
	memset(aTemporaryReadEccBuf, 0xFF, SECTOR_SIZE);

	aTemporarySBuf = (uint8_t*) malloc(Data.bytesPerSpare);

	return 0;
}

static int transferFromFlash(void* buffer, int size) {
	int controller = 0;
	int channel = 0;

	if((((uint32_t)buffer) & 0x3) != 0) {
		// the buffer needs to be aligned for DMA, last two bits have to be clear
		return ERROR_ALIGN;
	}

	SET_REG(NAND + NAND_CONFIG, GET_REG(NAND + NAND_CONFIG) | (1 << NAND_CONFIG_DMASETTINGSHIFT));
	SET_REG(NAND + NAND_TRANSFERSIZE, size - 1);
	SET_REG(NAND + NAND_CON, NAND_CON_BEGINTRANSFER);

	CleanCPUDataCache();

	dma_request(DMA_NAND, 4, 4, DMA_MEMORY, 4, 4, &controller, &channel);
	dma_perform(DMA_NAND, (uint32_t)buffer, size, 0, &controller, &channel);

	if(dma_finish(controller, channel, 500) != 0) {
		bufferPrintf("nand: dma timed out\r\n");
		return ERROR_TIMEOUT;
	}

	if(wait_for_status_bit_3(500) != 0) {
		bufferPrintf("nand: waiting for status bit 3 timed out\r\n");
		return ERROR_TIMEOUT;
	}

	SET_REG(NAND + NAND_CON, NAND_CON_SETTING1);

	CleanAndInvalidateCPUDataCache();

	return 0;
}

static void ecc_perform(int setting, int sectors, uint8_t* sectorData, uint8_t* eccData) {
	SET_REG(NANDECC + NANDECC_CLEARINT, 1);
	SET_REG(NANDECC + NANDECC_SETUP, ((sectors - 1) & 0x3) | setting);
	SET_REG(NANDECC + NANDECC_DATA, (uint32_t) sectorData);
	SET_REG(NANDECC + NANDECC_ECC, (uint32_t) eccData);

	CleanCPUDataCache();

	SET_REG(NANDECC + NANDECC_START, 1);
}

static int wait_for_ecc_interrupt(int timeout) {
	uint64_t startTime = timer_get_system_microtime();
	uint32_t mask = (1 << (NANDECC_INT - VIC_InterruptSeparator));
	while((GET_REG(VIC1 + VICRAWINTR) & mask) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NANDECC + NANDECC_CLEARINT, 1);

	if((GET_REG(VIC1 + VICRAWINTR) & mask) == 0) {
		return 0;
	} else {
		return ERROR_TIMEOUT;
	}
}

static int ecc_finish() {
	int ret;
	if((ret = wait_for_ecc_interrupt(500)) != 0) {
		return ret;
	}

	if((GET_REG(NANDECC + NANDECC_STATUS) & 0x1) != 0) {
		return ERROR_ECC;
	}

	return 0;
}

static int checkECC(int setting, uint8_t* data, uint8_t* ecc) {
	int eccSize = 0;

	if(setting == 4) {
		eccSize = 15;
	} else if(setting == 8) {
		eccSize = 20;
	} else if(setting == 0) {
		eccSize = 10;
	} else {
		return ERROR_ECC;
	}

	uint8_t* dataPtr = data;
	uint8_t* eccPtr = ecc;
	int sectorsLeft = Data.sectorsPerPage;

	while(sectorsLeft > 0) {
		int toCheck;
		if(sectorsLeft > 4)
			toCheck = 4;
		else
			toCheck = sectorsLeft;

		if(LargePages) {
			// If there are more than 4 sectors in a page...
			int i;
			for(i = 0; i < toCheck; i++) {
				// loop through each sector that we have to check this time's ECC
				uint8_t* x = &eccPtr[eccSize * i]; // first byte of ECC
				uint8_t* y = x + eccSize - 1; // last byte of ECC
				while(x < y) {
					// swap the byte order of them
					uint8_t t = *y;
					*y = *x;
					*x = t;
					x++;
					y--;
				}
			}
		}

		ecc_perform(setting, toCheck, dataPtr, eccPtr);
		if(ecc_finish() != 0)
			return ERROR_ECC;

		dataPtr += toCheck * SECTOR_SIZE;
		eccPtr += toCheck * eccSize;
		sectorsLeft -= toCheck;
	}

	return 0;
}

static int isBadBlock(uint8_t* buffer, int size) {
	int i;
	int found = 0;
	for(i = 0; i < size; i++) {
		if(buffer[i] != 0xFF) {
			found++;
		}
	}

	if(found <= 1)
		return 1;
	else
		return 0;
}

int nand_read(int bank, int page, uint8_t* buffer, uint8_t* spare, int doECC, int checkBadBlocks) {
	if(bank >= Data.banksTotal)
		return ERROR_ARG;

	if(page >= Data.pagesPerBank)
		return ERROR_ARG;

	if(buffer == NULL && spare == NULL)
		return ERROR_ARG;

	SET_REG(NAND + NAND_CONFIG,
		((NANDSetting1 & NAND_CONFIG_SETTING1MASK) << NAND_CONFIG_SETTING1SHIFT) | ((NANDSetting2 & NAND_CONFIG_SETTING2MASK) << NAND_CONFIG_SETTING2SHIFT)
		| (1 << (banksTable[bank] + 1)) | NAND_CONFIG_DEFAULTS);

	SET_REG(NAND + NAND_CMD, 0);
	if(wait_for_ready(500) != 0) {
		bufferPrintf("nand: bank setting failed\r\n");
		goto FIL_read_error;
	}

	SET_REG(NAND + NAND_CONFIG4, NAND_CONFIG4_TRANSFERSETTING);

	if(buffer) {
		SET_REG(NAND + NAND_CONFIG3, page << 16); // lower bits of the page number to the upper bits of CONFIG3
		SET_REG(NAND + NAND_CONFIG5, (page >> 16) & 0xFF); // upper bits of the page number

	} else {
		SET_REG(NAND + NAND_CONFIG3, (page << 16) | Data.bytesPerPage); // lower bits of the page number to the upper bits of CONFIG3
		SET_REG(NAND + NAND_CONFIG5, (page >> 16) & 0xFF); // upper bits of the page number	
	}

	SET_REG(NAND + NAND_CON, NAND_CON_SETUPTRANSFER);
	if(wait_for_status_bit_2(500) != 0) {
		bufferPrintf("nand: setup transfer failed\r\n");
		goto FIL_read_error;
	}
	
	SET_REG(NAND + NAND_CMD, NAND_CMD_READ);
	if(wait_for_ready(500) != 0) {
		bufferPrintf("nand: setting config2 failed\r\n");
		goto FIL_read_error;
	}

	if(bank_setup(bank) != 0) {
		bufferPrintf("nand: bank setup failed\r\n");
		goto FIL_read_error;
	}

	if(buffer) {
		if(transferFromFlash(buffer, Data.bytesPerPage) != 0) {
			bufferPrintf("nand: transferFromFlash failed\r\n");
			goto FIL_read_error;
		}
	}

	if(transferFromFlash(aTemporarySBuf, Data.bytesPerSpare) != 0) {
		bufferPrintf("nand: transferFromFlash for spare failed\r\n");
		goto FIL_read_error;
	}

	int eccFailed = 0;
	if(doECC) {
		if(buffer) {
			eccFailed = (checkECC(ECCType, buffer, aTemporarySBuf + 0xC) != 0);
		}

		memcpy(aTemporaryReadEccBuf, aTemporarySBuf, 0xC);
		ecc_perform(ECCType, 1, aTemporaryReadEccBuf, aTemporarySBuf + 0xC + TotalECCDataSize);
		if(ecc_finish() != 0) {
			memset(aTemporaryReadEccBuf, 0xFF, SECTOR_SIZE);
			eccFailed |= 1;
		}
	}

	if(spare) {
		if(doECC) {
			// We can only copy the first 12 bytes because the rest is probably changed by the ECC check routine
			memcpy(spare, aTemporaryReadEccBuf, 0xC);
		} else {
			memcpy(spare, aTemporarySBuf, Data.bytesPerSpare);
		}
	}

	if(eccFailed || checkBadBlocks) {
		if(isBadBlock(aTemporarySBuf, Data.bytesPerSpare) != 0) {
			return ERROR_BADBLOCK;
		} else if(eccFailed) {
			return ERROR_NAND;
		}
	}

	return 0;

FIL_read_error:
	bank_reset(bank, 100);
	return ERROR_NAND;
}

NANDData* nand_get_geometry() {
	return &Data;
}

UnknownNANDType* nand_get_data() {
	return &Data2;
}

int nand_read_alternate_ecc(int bank, int page, uint8_t* buffer) {
	int ret;
	if((ret = nand_read(bank, page, buffer, aTemporarySBuf, FALSE, TRUE)) != 0)
		return ret;

	if(checkECC(ECCType2, buffer, aTemporarySBuf) != 0)
		return ERROR_NAND;

	return 0;
}

