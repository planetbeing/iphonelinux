#include "openiboot.h"
#include "nand.h"
#include "hardware/nand.h"
#include "timer.h"
#include "clock.h"
#include "util.h"
#include "openiboot-asmhelpers.h"
#include "dma.h"
#include "hardware/interrupt.h"

int HasNANDInit = FALSE;

static int banksTable[NAND_NUM_BANKS];

static int ECCType = 0;
static uint8_t WEHighHoldTime;
static uint8_t WPPulseTime;
static uint8_t NANDSetting3;
static uint8_t NANDSetting4;
static uint32_t TotalECCDataSize;
static uint32_t ECCType2;
static int NumValidBanks = 0;
static const int NoMultibankCmdStatus = 1;
static int LargePages;

static NANDData Geometry;
static NANDFTLData FTLData;

static uint8_t* aTemporaryReadEccBuf;
static uint8_t* aTemporarySBuf;

#define SECTOR_SIZE 512

static const NANDDeviceType SupportedDevices[] = {
	{0x2555D5EC, 8192, 128, 4, 64, 4, 2, 4, 2, 7744, 4, 6},
	{0xB614D5EC, 4096, 128, 8, 128, 4, 2, 4, 2, 3872, 4, 6},
	{0xB655D7EC, 8192, 128, 8, 128, 4, 2, 4, 2, 7744, 4, 6},
	{0xA514D3AD, 4096, 128, 4, 64, 4, 2, 4, 2, 3872, 4, 6},
	{0xA555D5AD, 8192, 128, 4, 64, 4, 2, 4, 2, 7744, 4, 6},
	{0xB614D5AD, 4096, 128, 8, 128, 4, 2, 4, 2, 3872, 4, 6},
	{0xB655D7AD, 8192, 128, 8, 128, 4, 2, 4, 2, 7744, 4, 6},
	{0xA585D598, 8320, 128, 4, 64, 6, 2, 4, 2, 7744, 4, 6},
	{0xBA94D598, 4096, 128, 8, 216, 6, 2, 4, 2, 3872, 8, 8},
	{0xBA95D798, 8192, 128, 8, 216, 6, 2, 4, 2, 7744, 8, 8},
	{0x3ED5D789, 8192, 128, 8, 216, 4, 2, 4, 2, 7744, 8, 8},
	{0x3E94D589, 4096, 128, 8, 216, 4, 2, 4, 2, 3872, 8, 8},
	{0x3ED5D72C, 8192, 128, 8, 216, 4, 2, 4, 2, 7744, 8, 8},
	{0x3E94D52C, 4096, 128, 8, 216, 4, 2, 4, 2, 3872, 8, 8},
	{0}
};

static int wait_for_ready(int timeout) {
	if((GET_REG(NAND + FMCSTAT) & FMCSTAT_READY) != 0) {
		return 0;
	}

	uint64_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + FMCSTAT) & FMCSTAT_READY) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	return 0;
}

static int wait_for_address_done(int timeout) {
	if((GET_REG(NAND + FMCSTAT) & (1 << 2)) != 0) {
		SET_REG(NAND + FMCSTAT, 1 << 2);
		return 0;
	}

	uint64_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + FMCSTAT) & (1 << 2)) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + FMCSTAT, 1 << 2);

	return 0;
}

static int wait_for_transfer_done(int timeout) {
	if((GET_REG(NAND + FMCSTAT) & (1 << 3)) != 0) {
		SET_REG(NAND + FMCSTAT, 1 << 3);
		return 0;
	}

	uint64_t startTime = timer_get_system_microtime();
	while((GET_REG(NAND + FMCSTAT) & (1 << 3)) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + FMCSTAT, 1 << 3);

	return 0;
}

int nand_read_status()
{
	int status;

	SET_REG(NAND + NAND_REG_44, GET_REG(NAND + NAND_REG_44) & ~(1 << 4));
	SET_REG(NAND + FMCTRL1, FMCTRL1_CLEARALL);
	SET_REG(NAND + NAND_CMD, NAND_CMD_READSTATUS);
	SET_REG(NAND + FMDNUM, 0);
	SET_REG(NAND + FMCTRL1, FMCTRL1_CLEARALL | FMCTRL1_DOREADDATA);

	wait_for_transfer_done(500);

	status = GET_REG(NAND + FMFIFO);
	SET_REG(NAND + FMCTRL1, FMCTRL1_CLEARALL);
	SET_REG(NAND + FMCSTAT, 1 << 3);
	SET_REG(NAND + NAND_REG_44, GET_REG(NAND + NAND_REG_44) | (1 << 2));

	return status;
}

static int wait_for_command_done(int bank, int timeout) {
	uint64_t startTime = timer_get_system_microtime();
	if(NoMultibankCmdStatus)
		bank = 0;
	else
		bank &= 0xffff;

	uint32_t toTest = 1 << (bank + 4);

	while((GET_REG(NAND + FMCSTAT) & toTest) == 0) {
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	SET_REG(NAND + FMCSTAT, toTest);

	return 0;
}

int nand_bank_reset(int bank, int timeout) {
	SET_REG(NAND + FMCTRL0,
			((WEHighHoldTime & FMCTRL_TWH_MASK) << FMCTRL_TWH_SHIFT) | ((WPPulseTime & FMCTRL_TWP_MASK) << FMCTRL_TWP_SHIFT)
			| (1 << (banksTable[bank] + 1)) | FMCTRL0_ON | FMCTRL0_WPB);

	SET_REG(NAND + NAND_CMD, NAND_CMD_RESET);

	int ret = wait_for_ready(timeout);
	if(ret == 0) {
		ret = wait_for_command_done(bank, timeout);
		udelay(1000);
		return ret;
	} else {
		udelay(1000);
		return ret;
	}
}


static int wait_for_nand_bank_ready(int bank) {
	SET_REG(NAND + FMCTRL0,
			((WEHighHoldTime & FMCTRL_TWH_MASK) << FMCTRL_TWH_SHIFT) | ((WPPulseTime & FMCTRL_TWP_MASK) << FMCTRL_TWP_SHIFT)
			| (1 << (banksTable[bank] + 1)) | FMCTRL0_ON | FMCTRL0_WPB);

	uint32_t toTest = 1 << (bank + 4);
	if((GET_REG(NAND + FMCSTAT) & toTest) != 0) {
		SET_REG(NAND + FMCSTAT, toTest);
	}

	SET_REG(NAND + FMCTRL1, FMCTRL1_FLUSHFIFOS); 
	SET_REG(NAND + NAND_CMD, NAND_CMD_READSTATUS);
	wait_for_ready(500);

	uint64_t startTime = timer_get_system_microtime();
	while(TRUE) {
		SET_REG(NAND + FMDNUM, 0);
		SET_REG(NAND + FMCTRL1, FMCTRL1_DOREADDATA);

		if(wait_for_transfer_done(500) != 0) {
			bufferPrintf("nand: wait_for_nand_bank_ready: wait for transfer done timed out\r\n");
			return ERROR_TIMEOUT;
		}


		uint32_t data = GET_REG(NAND + FMFIFO);
		SET_REG(NAND + FMCTRL1, FMCTRL1_FLUSHRXFIFO);
		if((data & (1 << 6)) == 0) {
			if(has_elapsed(startTime, 500 * 1000)) {
				bufferPrintf("nand: wait_for_nand_bank_ready: wait for bit 6 of DMA timed out\r\n");
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
	if(HasNANDInit)
		return 0;

	WEHighHoldTime = 7;
	WPPulseTime = 7;
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

	SET_REG(NAND + RSCTRL, 0);
	SET_REG(NAND + RSCTRL, GET_REG(NAND + RSCTRL) | (ECCType << 4));

	for(bank = 0; bank < NAND_NUM_BANKS; bank++) {
		nand_bank_reset(bank, 100);

		SET_REG(NAND + FMCTRL1, FMCTRL1_FLUSHFIFOS);
		SET_REG(NAND + FMCTRL0,
			((WEHighHoldTime & FMCTRL_TWH_MASK) << FMCTRL_TWH_SHIFT) | ((WPPulseTime & FMCTRL_TWP_MASK) << FMCTRL_TWP_SHIFT)
			| (1 << (banksTable[bank] + 1)) | FMCTRL0_ON | FMCTRL0_WPB);

		SET_REG(NAND + NAND_CMD, NAND_CMD_ID);

		wait_for_ready(500);

		SET_REG(NAND + FMANUM, 0);
		SET_REG(NAND + FMADDR0, 0);
		SET_REG(NAND + FMCTRL1, FMCTRL1_DOTRANSADDR);

		wait_for_address_done(500);
		wait_for_command_done(bank, 100);

		SET_REG(NAND + FMDNUM, 8);
		SET_REG(NAND + FMCTRL1, FMCTRL1_DOREADDATA);

		wait_for_transfer_done(500);
		uint32_t id = GET_REG(NAND + FMFIFO);
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

		SET_REG(NAND + FMCTRL1, FMCTRL1_FLUSHFIFOS);
	}

	if(nandType == NULL) {
		bufferPrintf("nand: No supported NAND found\r\n");
		return ERROR_ARG;
	}

	Geometry.DeviceID = nandType->id;
	Geometry.banksTable = banksTable;

	WPPulseTime = (((clock_get_frequency(FrequencyBaseBus) * (nandType->WPPulseTime + 1)) + 99999999)/100000000) - 1;
	WEHighHoldTime = (((clock_get_frequency(FrequencyBaseBus) * (nandType->WEHighHoldTime + 1)) + 99999999)/100000000) - 1;
	NANDSetting3 = (((clock_get_frequency(FrequencyBaseBus) * (nandType->NANDSetting3 + 1)) + 99999999)/100000000) - 1;
	NANDSetting4 = (((clock_get_frequency(FrequencyBaseBus) * (nandType->NANDSetting4 + 1)) + 99999999)/100000000) - 1;

	if(WPPulseTime > 7)
		WPPulseTime = 7;

	if(WEHighHoldTime > 7)
		WEHighHoldTime = 7;

	if(NANDSetting3 > 7)
		NANDSetting3 = 7;

	if(NANDSetting4 > 7)
		NANDSetting4 = 7;

	Geometry.blocksPerBank = nandType->blocksPerBank;
	Geometry.banksTotal = NumValidBanks;
	Geometry.sectorsPerPage = nandType->sectorsPerPage;
	Geometry.userSuBlksTotal = nandType->userSuBlksTotal;
	Geometry.bytesPerSpare = nandType->bytesPerSpare;
	Geometry.field_2E = 4;
	Geometry.field_2F = 3;
	Geometry.pagesPerBlock = nandType->pagesPerBlock;

	if(Geometry.sectorsPerPage > 4) {
		LargePages = TRUE;
	} else {
		LargePages = FALSE;
	}

	if(nandType->ecc1 == 6) {
		ECCType = 4;
		TotalECCDataSize = Geometry.sectorsPerPage * 15;
	} else if(nandType->ecc1 == 8) {
		ECCType = 8;
		TotalECCDataSize = Geometry.sectorsPerPage * 20;
	} else if(nandType->ecc1 == 4) {
		ECCType = 0;
		TotalECCDataSize = Geometry.sectorsPerPage * 10;
	}

	if(nandType->ecc2 == 6) {
		ECCType2 = 4;
	} else if(nandType->ecc2 == 8) {
		ECCType2 = 8;
	} else if(nandType->ecc2 == 4) {
		ECCType2 = 0;
	}

	Geometry.field_4 = 5;
	Geometry.bytesPerPage = SECTOR_SIZE * Geometry.sectorsPerPage;
	Geometry.pagesPerBank = Geometry.pagesPerBlock * Geometry.blocksPerBank;
	Geometry.pagesTotal = Geometry.pagesPerBank * Geometry.banksTotal;
	Geometry.pagesPerSuBlk = Geometry.pagesPerBlock * Geometry.banksTotal;
	Geometry.userPagesTotal = Geometry.userSuBlksTotal * Geometry.pagesPerSuBlk;
	Geometry.suBlksTotal = (Geometry.banksTotal * Geometry.blocksPerBank) / Geometry.banksTotal;

	FTLData.field_2 = Geometry.suBlksTotal - Geometry.userSuBlksTotal - 28;
	FTLData.sysSuBlks = FTLData.field_2 + 4;
	FTLData.field_4 = FTLData.field_2 + 5;
	FTLData.field_6 = 3;
	FTLData.field_8 = 23;
	if(FTLData.field_8 == 0)
		Geometry.field_22 = 0;

	int bits = 0;
	int i = FTLData.field_8;
	while((i <<= 1) != 0) {
		bits++;
	}

	Geometry.field_22 = bits;

	bufferPrintf("nand: DEVICE: %08x\r\n", Geometry.DeviceID);
	bufferPrintf("nand: BANKS_TOTAL: %d\r\n", Geometry.banksTotal);
	bufferPrintf("nand: BLOCKS_PER_BANK: %d\r\n", Geometry.blocksPerBank);
	bufferPrintf("nand: SUBLKS_TOTAL: %d\r\n", Geometry.suBlksTotal);
	bufferPrintf("nand: USER_SUBLKS_TOTAL: %d\r\n", Geometry.userSuBlksTotal);
	bufferPrintf("nand: PAGES_PER_SUBLK: %d\r\n", Geometry.pagesPerSuBlk);
	bufferPrintf("nand: PAGES_PER_BANK: %d\r\n", Geometry.pagesPerBank);
	bufferPrintf("nand: SECTORS_PER_PAGE: %d\r\n", Geometry.sectorsPerPage);
	bufferPrintf("nand: BYTES_PER_SPARE: %d\r\n", Geometry.bytesPerSpare);
	bufferPrintf("nand: BYTES_PER_PAGE: %d\r\n", Geometry.bytesPerPage);
	bufferPrintf("nand: PAGES_PER_BLOCK: %d\r\n", Geometry.pagesPerBlock);

	aTemporaryReadEccBuf = (uint8_t*) malloc(Geometry.bytesPerPage);
	memset(aTemporaryReadEccBuf, 0xFF, SECTOR_SIZE);

	aTemporarySBuf = (uint8_t*) malloc(Geometry.bytesPerSpare);

	HasNANDInit = TRUE;

	return 0;
}

static int transferFromFlash(void* buffer, int size) {
	int controller = 0;
	int channel = 0;

	if((((uint32_t)buffer) & 0x3) != 0) {
		// the buffer needs to be aligned for DMA, last two bits have to be clear
		return ERROR_ALIGN;
	}

	SET_REG(NAND + FMCTRL0, GET_REG(NAND + FMCTRL0) | (1 << FMCTRL0_DMASETTINGSHIFT));
	SET_REG(NAND + FMDNUM, size - 1);
	SET_REG(NAND + FMCTRL1, FMCTRL1_DOREADDATA);

	CleanCPUDataCache();

	dma_request(DMA_NAND, 4, 4, DMA_MEMORY, 4, 4, &controller, &channel, NULL);
	dma_perform(DMA_NAND, (uint32_t)buffer, size, 0, &controller, &channel);

	if(dma_finish(controller, channel, 500) != 0) {
		bufferPrintf("nand: dma timed out\r\n");
		return ERROR_TIMEOUT;
	}

	if(wait_for_transfer_done(500) != 0) {
		bufferPrintf("nand: waiting for transfer done timed out\r\n");
		return ERROR_TIMEOUT;
	}

	SET_REG(NAND + FMCTRL1, FMCTRL1_FLUSHFIFOS);

	CleanAndInvalidateCPUDataCache();

	return 0;
}

static int transferToFlash(void* buffer, int size) {
	int controller = 0;
	int channel = 0;

	if((((uint32_t)buffer) & 0x3) != 0) {
		// the buffer needs to be aligned for DMA, last two bits have to be clear
		return ERROR_ALIGN;
	}

	SET_REG(NAND + FMCTRL0, GET_REG(NAND + FMCTRL0) | (1 << FMCTRL0_DMASETTINGSHIFT));
	SET_REG(NAND + FMDNUM, size - 1);
	SET_REG(NAND + FMCTRL1, 0x7F4);

	CleanCPUDataCache();

	dma_request(DMA_MEMORY, 4, 4, DMA_NAND, 4, 4, &controller, &channel, NULL);
	dma_perform((uint32_t)buffer, DMA_NAND, size, 0, &controller, &channel);

	if(dma_finish(controller, channel, 500) != 0) {
		bufferPrintf("nand: dma timed out\r\n");
		return ERROR_TIMEOUT;
	}

	if(wait_for_transfer_done(500) != 0) {
		bufferPrintf("nand: waiting for transfer done timed out\r\n");
		return ERROR_TIMEOUT;
	}

	SET_REG(NAND + FMCTRL1, FMCTRL1_FLUSHFIFOS);

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

static void ecc_generate(int setting, int sectors, uint8_t* sectorData, uint8_t* eccData) {
	SET_REG(NANDECC + NANDECC_CLEARINT, 1);
	SET_REG(NANDECC + NANDECC_SETUP, ((sectors - 1) & 0x3) | setting);
	SET_REG(NANDECC + NANDECC_DATA, (uint32_t) sectorData);
	SET_REG(NANDECC + NANDECC_ECC, (uint32_t) eccData);

	CleanAndInvalidateCPUDataCache();

	SET_REG(NANDECC + NANDECC_START, 2);
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
	if((ret = wait_for_ecc_interrupt(500)) != 0)
		return ret;

	if((GET_REG(NANDECC + NANDECC_STATUS) & 0x1) != 0)
		return ERROR_ECC;

	return 0;
}

static int generateECC(int setting, uint8_t* data, uint8_t* ecc) {
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
	int sectorsLeft = Geometry.sectorsPerPage;

	while(sectorsLeft > 0) {
		int toCheck;
		if(sectorsLeft > 4)
			toCheck = 4;
		else
			toCheck = sectorsLeft;

		ecc_generate(setting, toCheck, dataPtr, eccPtr);
		ecc_finish();

		if(LargePages) {
			// If there are more than 4 sectors in a page...
			int i;
			for(i = 0; i < toCheck; i++) {
				// loop through each sector that we have generated this time's ECC
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

		dataPtr += toCheck * SECTOR_SIZE;
		eccPtr += toCheck * eccSize;
		sectorsLeft -= toCheck;
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
	int sectorsLeft = Geometry.sectorsPerPage;

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

static int isEmptyBlock(uint8_t* buffer, int size) {
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

int nand_erase(int bank, int block) {
	int pageAddr;

	if(bank >= Geometry.banksTotal)
		return ERROR_ARG;

	if(block >= Geometry.blocksPerBank)
		return ERROR_ARG;

	pageAddr = block * Geometry.pagesPerBlock;

	SET_REG(NAND + FMCTRL0,
		((WEHighHoldTime & FMCTRL_TWH_MASK) << FMCTRL_TWH_SHIFT) | ((WPPulseTime & FMCTRL_TWP_MASK) << FMCTRL_TWP_SHIFT)
		| (1 << (banksTable[bank] + 1)) | FMCTRL0_ON | FMCTRL0_WPB);

	SET_REG(NAND + FMCTRL1, FMCTRL1_CLEARALL);
	SET_REG(NAND + NAND_CMD, 0x60);

	SET_REG(NAND + FMANUM, 2);
	SET_REG(NAND + FMADDR0, pageAddr);
	SET_REG(NAND + FMCTRL1, FMCTRL1_DOTRANSADDR);

	if(wait_for_address_done(500) != 0) {
		bufferPrintf("nand (nand_erase): wait for address complete failed\r\n");
		goto FIL_erase_error;
	}

	SET_REG(NAND + NAND_CMD, 0xD0);
	wait_for_ready(500);

	while((nand_read_status() & (1 << 6)) == 0);

	if(nand_read_status() & 0x1)
		return -1;
	else
		return 0;

FIL_erase_error:
	return -1;

}

int nand_calculate_ecc(uint8_t* data, uint8_t* ecc) {
	return generateECC(ECCType, data, ecc);
}

int nand_read(int bank, int page, uint8_t* buffer, uint8_t* spare, int doECC, int checkBlank) {
	if(bank >= Geometry.banksTotal)
		return ERROR_ARG;

	if(page >= Geometry.pagesPerBank)
		return ERROR_ARG;

	if(buffer == NULL && spare == NULL)
		return ERROR_ARG;

	SET_REG(NAND + FMCTRL0,
		((WEHighHoldTime & FMCTRL_TWH_MASK) << FMCTRL_TWH_SHIFT) | ((WPPulseTime & FMCTRL_TWP_MASK) << FMCTRL_TWP_SHIFT)
		| (1 << (banksTable[bank] + 1)) | FMCTRL0_ON | FMCTRL0_WPB);

	SET_REG(NAND + NAND_CMD, 0);
	if(wait_for_ready(500) != 0) {
		bufferPrintf("nand: bank setting failed\r\n");
		goto FIL_read_error;
	}

	SET_REG(NAND + FMANUM, FMANUM_TRANSFERSETTING);

	if(buffer) {
		SET_REG(NAND + FMADDR0, page << 16); // lower bits of the page number to the upper bits of CONFIG3
		SET_REG(NAND + FMADDR1, (page >> 16) & 0xFF); // upper bits of the page number

	} else {
		SET_REG(NAND + FMADDR0, (page << 16) | Geometry.bytesPerPage); // lower bits of the page number to the upper bits of CONFIG3
		SET_REG(NAND + FMADDR1, (page >> 16) & 0xFF); // upper bits of the page number	
	}

	SET_REG(NAND + FMCTRL1, FMCTRL1_DOTRANSADDR);
	if(wait_for_address_done(500) != 0) {
		bufferPrintf("nand: sending address failed\r\n");
		goto FIL_read_error;
	}
	
	SET_REG(NAND + NAND_CMD, NAND_CMD_READ);
	if(wait_for_ready(500) != 0) {
		bufferPrintf("nand: sending read command failed\r\n");
		goto FIL_read_error;
	}

	if(wait_for_nand_bank_ready(bank) != 0) {
		bufferPrintf("nand: nand bank not ready after a long time\r\n");
		goto FIL_read_error;
	}

	if(buffer) {
		if(transferFromFlash(buffer, Geometry.bytesPerPage) != 0) {
			bufferPrintf("nand: transferFromFlash failed\r\n");
			goto FIL_read_error;
		}
	}

	if(transferFromFlash(aTemporarySBuf, Geometry.bytesPerSpare) != 0) {
		bufferPrintf("nand: transferFromFlash for spare failed\r\n");
		goto FIL_read_error;
	}

	int eccFailed = 0;
	if(doECC) {
		if(buffer) {
			eccFailed = (checkECC(ECCType, buffer, aTemporarySBuf + sizeof(SpareData)) != 0);
		}

		memcpy(aTemporaryReadEccBuf, aTemporarySBuf, sizeof(SpareData));
		ecc_perform(ECCType, 1, aTemporaryReadEccBuf, aTemporarySBuf + sizeof(SpareData) + TotalECCDataSize);
		if(ecc_finish() != 0) {
			memset(aTemporaryReadEccBuf, 0xFF, SECTOR_SIZE);
			eccFailed |= 1;
		}
	}

	if(spare) {
		if(doECC) {
			// We can only copy the first 12 bytes because the rest is probably changed by the ECC check routine
			memcpy(spare, aTemporaryReadEccBuf, sizeof(SpareData));
		} else {
			memcpy(spare, aTemporarySBuf, Geometry.bytesPerSpare);
		}
	}

	if(eccFailed || checkBlank) {
		if(isEmptyBlock(aTemporarySBuf, Geometry.bytesPerSpare) != 0) {
			return ERROR_EMPTYBLOCK;
		} else if(eccFailed) {
			return ERROR_NAND;
		}
	}

	return 0;

FIL_read_error:
	nand_bank_reset(bank, 100);
	return ERROR_NAND;
}

int nand_write(int bank, int page, uint8_t* buffer, uint8_t* spare, int doECC) {
	if(bank >= Geometry.banksTotal)
		return ERROR_ARG;

	if(page >= Geometry.pagesPerBank)
		return ERROR_ARG;

	if(buffer == NULL && spare == NULL)
		return ERROR_ARG;

	if(doECC) {
		memcpy(aTemporarySBuf, spare, sizeof(SpareData));
		if(generateECC(ECCType, buffer, aTemporarySBuf + sizeof(SpareData)) != 0) {
			bufferPrintf("nand: Unexpected error during ECC generation\r\n");
			return ERROR_ARG;
		}

		memset(aTemporaryReadEccBuf, 0xFF, SECTOR_SIZE);
		memcpy(aTemporaryReadEccBuf, spare, sizeof(SpareData));

		ecc_generate(ECCType, 1, aTemporaryReadEccBuf, aTemporarySBuf + sizeof(SpareData) + TotalECCDataSize);
		ecc_finish();
	}

	SET_REG(NAND + FMCTRL0,
		((WEHighHoldTime & FMCTRL_TWH_MASK) << FMCTRL_TWH_SHIFT) | ((WPPulseTime & FMCTRL_TWP_MASK) << FMCTRL_TWP_SHIFT)
		| (1 << (banksTable[bank] + 1)) | FMCTRL0_ON | FMCTRL0_WPB);

	SET_REG(NAND + NAND_CMD, 0x80);
	if(wait_for_ready(500) != 0) {
		bufferPrintf("nand: bank setting failed\r\n");
		goto FIL_write_error;
	}

	SET_REG(NAND + FMANUM, FMANUM_TRANSFERSETTING);

	if(buffer) {
		SET_REG(NAND + FMADDR0, page << 16); // lower bits of the page number to the upper bits of CONFIG3
		SET_REG(NAND + FMADDR1, (page >> 16) & 0xFF); // upper bits of the page number
	} else {
		SET_REG(NAND + FMADDR0, (page << 16) | Geometry.bytesPerPage); // lower bits of the page number to the upper bits of CONFIG3
		SET_REG(NAND + FMADDR1, (page >> 16) & 0xFF); // upper bits of the page number	
	}

	SET_REG(NAND + FMCTRL1, FMCTRL1_DOTRANSADDR);
	if(wait_for_address_done(500) != 0) {
		bufferPrintf("nand: setup transfer failed\r\n");
		goto FIL_write_error;
	}

	if(buffer) {
		if(transferToFlash(buffer, Geometry.bytesPerPage) != 0) {
			bufferPrintf("nand: transferToFlash failed\r\n");
			goto FIL_write_error;
		}
	}

	if(transferToFlash(aTemporarySBuf, Geometry.bytesPerSpare) != 0) {
		bufferPrintf("nand: transferToFlash for spare failed\r\n");
		goto FIL_write_error;
	}

	SET_REG(NAND + NAND_CMD, 0x10);
	wait_for_ready(500);

	while((nand_read_status() & (1 << 6)) == 0);

	if(nand_read_status() & 0x1)
		return -1;
	else
		return 0;

	return 0;

FIL_write_error:
	nand_bank_reset(bank, 100);
	return ERROR_NAND;
}

NANDData* nand_get_geometry() {
	return &Geometry;
}

NANDFTLData* nand_get_ftl_data() {
	return &FTLData;
}

int nand_read_multiple(uint16_t* bank, uint32_t* pages, uint8_t* main, SpareData* spare, int pagesCount) {
	int i;
	unsigned int ret;
	for(i = 0; i < pagesCount; i++) {
		ret = nand_read(bank[i], pages[i], main, (uint8_t*) &spare[i], TRUE, TRUE);
		if(ret > 1)
			return ret;

		main += Geometry.bytesPerPage;
	}

	return 0;
}

int nand_read_alternate_ecc(int bank, int page, uint8_t* buffer) {
	int ret;
	if((ret = nand_read(bank, page, buffer, aTemporarySBuf, FALSE, TRUE)) != 0) {
		DebugPrintf("nand: Raw read failed.\r\n");
		return ret;
	}

	if(checkECC(ECCType2, buffer, aTemporarySBuf) != 0) {
		DebugPrintf("nand: Alternate ECC check failed, but raw read succeeded.\r\n");
		return ERROR_NAND;
	}

	return 0;
}

