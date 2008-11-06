#include "openiboot.h"
#include "ftl.h"
#include "nand.h"
#include "util.h"

#define FTL_ID 0x43303033

static NANDData* Data;

static int findDeviceInfoBBT(int bank, void* deviceInfoBBT) {
	uint8_t* buffer = malloc(Data->bytesPerPage);
	int lowestBlock = Data->blocksPerBank - (Data->blocksPerBank / 10);
	int block;
	for(block = Data->blocksPerBank - 1; block >= lowestBlock; block--) {
		int page;
		int badBlockCount = 0;
		for(page = 0; page < Data->pagesPerBlock; page++) {
			if(badBlockCount > 2)
				break;

			int ret = nand_read_alternate_ecc(bank, (block * Data->pagesPerBlock) + page, buffer);
			if(ret != 0) {
				if(ret == 1)
					badBlockCount++;

				continue;
			}

			if(memcmp(buffer, "DEVICEINFOBBT\0\0\0", 16) == 0) {
				if(deviceInfoBBT) {
					memcpy(deviceInfoBBT, buffer + 0x38, *((uint32_t*)(buffer + 0x34)));
				}

				free(buffer);
				return TRUE;
			}
		}
	}

	free(buffer);
	return FALSE;
}

static int hasDeviceInfoBBT() {
	int bank;
	int good = TRUE;
	for(bank = 0; bank < Data->banksTotal; bank++) {
		good = findDeviceInfoBBT(bank, NULL);
		if(!good)
			return FALSE;
	}

	return good;
}

int ftl_setup() {
	Data = nand_get_geometry();

	int i;
	int foundSignature = FALSE;

	uint8_t* buffer = malloc(Data->bytesPerPage);
	for(i = 0; i < Data->pagesPerBlock; i++) {
		if(nand_read_alternate_ecc(0, i, buffer) == 0 && *((uint32_t*) buffer) == FTL_ID) {
			foundSignature = TRUE;
		}
	}
	free(buffer);

	if(!foundSignature || !hasDeviceInfoBBT()) {
		return -1;
	}

	return 0;
}
