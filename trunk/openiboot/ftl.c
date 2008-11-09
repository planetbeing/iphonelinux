#include "openiboot.h"
#include "ftl.h"
#include "nand.h"
#include "util.h"

#define FTL_ID 0x43303034

static NANDData* Data;
static UnknownNANDType* Data2;

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

static VFLData1Type VFLData1;
static VFLCxt* pstVFLCxt = NULL;
static uint8_t* pstBBTArea = NULL;
static void* VFLData2 = NULL;
static void* VFLData3 = NULL;
static int VFLData4 = 0;

static int VFL_Init() {
	memset(&VFLData1, 0, sizeof(VFLData1));
	if(pstVFLCxt == NULL) {
		pstVFLCxt = malloc(Data->banksTotal * sizeof(VFLCxt));
		if(pstVFLCxt == NULL)
			return -1;
	}

	if(pstBBTArea == NULL) {
		pstBBTArea = (uint8_t*) malloc((Data->blocksPerBank + 7) / 8);
		if(pstBBTArea == NULL)
			return -1;
	}

	if(VFLData2 == NULL && VFLData3 == NULL) {
		VFLData2 = malloc(Data->pagesPerSubBlk * 4);
		VFLData3 = malloc(Data->pagesPerSubBlk * 4);
		if(VFLData2 == NULL || VFLData3 == NULL)
			return -1;
	}

	VFLData4 = 0;

	return 0;
}

static uint8_t FTLData1[0x58];
static void** FTLData2;
static void** FTLData3;
static void* FTLData4;
static void* FTLData5;
static uint8_t* StoreCxt;

static int FTL_Init() {
	int numPagesToWriteInStoreCxt = 0;

	int x = ((Data->userSubBlksTotal + 23) * 2) / Data->bytesPerPage;
	if((((Data->userSubBlksTotal + 23) * 2) % Data->bytesPerPage) != 0) {
		x++;
	}

	numPagesToWriteInStoreCxt = x * 2;

	int y = (Data->userSubBlksTotal * 2) / Data->bytesPerPage;
	if(((Data->userSubBlksTotal * 2) % Data->bytesPerPage) != 0) {
		y++;
	}

	numPagesToWriteInStoreCxt += y;

	int z = (Data->pagesPerSubBlk * 34) / Data->bytesPerPage;
	if(((Data->pagesPerSubBlk * 34) % Data->bytesPerPage) != 0) {
		z++;
	}

	numPagesToWriteInStoreCxt += z + 2;

	if(numPagesToWriteInStoreCxt >= Data->pagesPerSubBlk) {
		bufferPrintf("nand: error - FTL_NUM_PAGES_TO_WRITE_IN_STORECXT >= PAGES_PER_SUBLK\r\n");
		return -1;
	}

	int pagesPerSimpleMergeBuffer = Data->pagesPerSubBlk / 8;
	if((pagesPerSimpleMergeBuffer * 2) >= Data->pagesPerSubBlk) {
		bufferPrintf("nand: error - (PAGES_PER_SIMPLE_MERGE_BUFFER * 2) >=  PAGES_PER_SUBLK\r\n");
		return -1;
	}

	memset(FTLData1, 0, 0x58);

	if(FTLData2 == NULL) {
		FTLData2 = FTLData3 = (void**) malloc(512 * sizeof(void*));
		if(FTLData2 == NULL)
			return -1;
		memset(&FTLData2[246], 0, 264 * sizeof(uint32_t)); 
	}

	FTLData2[199] = NULL;

	int someIndex1 = 408;
	FTLData2[someIndex1/4] = malloc(Data->userSubBlksTotal * 2);
	int someIndex2 = 416;
	FTLData2[someIndex2/4] = malloc(Data->pagesPerSubBlk * 36);
	int someIndex3 = 412;
	FTLData2[someIndex3/4] = malloc((Data->userSubBlksTotal + 23) * 2);
	int someIndex4 = 944;
	FTLData2[someIndex4/4] = malloc((Data->userSubBlksTotal + 23) * 2);

	FTLData4 = malloc(Data->pagesPerSubBlk * 12);

	if((Data->pagesPerSubBlk / 8) >= numPagesToWriteInStoreCxt) {
		numPagesToWriteInStoreCxt = Data->pagesPerSubBlk / 8;
	}

	StoreCxt = malloc(Data->bytesPerPage * numPagesToWriteInStoreCxt);
	FTLData5 = malloc(Data->pagesPerSubBlk * 4);

	if(!FTLData2[someIndex1/4] || !FTLData2[someIndex2/4] || !FTLData2[someIndex3/4] || !FTLData3[someIndex4/4] | ! FTLData4 | !StoreCxt | !FTLData5)
		return -1;

	int i;
	for(i = 0; i < 18; i++) {
		*((void**)(((uint8_t*)(FTLData2)) + (i * 0x14) + 428)) = FTLData2[someIndex2/4] + (i * Data->pagesPerSubBlk * 2);
		memset(*((void**)(((uint8_t*)(FTLData2)) + (i * 0x14) + 428)), 0xFF, Data->pagesPerSubBlk * 2);
		*((uint32_t*)(((uint8_t*)(FTLData2)) + (i * 0x14) + 436)) = 1;
		*((uint16_t*)(((uint8_t*)(FTLData2)) + (i * 0x14) + 432)) = 0;
		*((uint16_t*)(((uint8_t*)(FTLData2)) + (i * 0x14) + 434)) = 0;
	}

	return 0;
}

// pageBuffer and spareBuffer are represented by single BUF struct within Whimory
static int vfl_read_page(int bank, int block, int page, uint8_t* pageBuffer, uint8_t* spareBuffer) {
	int i;
	for(i = 0; i < 8; i++) {
		if(nand_read(bank, (block * Data->pagesPerBlock) + page + i, pageBuffer, spareBuffer, TRUE, TRUE) == 0) {
			SpareData* spareData = (SpareData*) spareBuffer;
			if(spareData->field_8 == 0 && spareData->field_9 == 0x80)
				return TRUE;
		}
	}
	return FALSE;
}

static void vfl_checksum(void* data, int size, uint32_t* a, uint32_t* b) {
	int i;
	uint32_t* buffer = (uint32_t*) data;
	uint32_t x = 0;
	uint32_t y = 0;
	for(i = 0; i < (size / 4); i++) {
		x += buffer[i];
		y ^= buffer[i];
	}

	*a = x + 0xAABBCCDD;
	*b = y ^ 0xAABBCCDD;
}

static int vfl_gen_checksum(int bank) {
	vfl_checksum(&pstVFLCxt[bank], (uint32_t)&pstVFLCxt[bank].checksum1 - (uint32_t)&pstVFLCxt[bank], &pstVFLCxt[bank].checksum1, &pstVFLCxt[bank].checksum2);
	return FALSE;
}

static int vfl_check_checksum(int bank) {
	static int counter = 0;

	counter++;

	uint32_t checksum1;
	uint32_t checksum2;
	vfl_checksum(&pstVFLCxt[bank], (uint32_t)&pstVFLCxt[bank].checksum1 - (uint32_t)&pstVFLCxt[bank], &checksum1, &checksum2);

	// Yeah, this looks fail, but this is actually the logic they use
	if(checksum1 == pstVFLCxt[bank].checksum1)
		return TRUE;

	if(checksum2 != pstVFLCxt[bank].checksum2)
		return TRUE;

	return FALSE;
}

static void virtual_page_number_to_virtual_address(uint32_t dwVpn, int* virtualBank, int* virtualBlock, int* virtualPage) {
	*virtualBank = dwVpn % Data->banksTotal;
	*virtualBlock = dwVpn / Data->pagesPerSubBlk;
	*virtualPage = (dwVpn / Data->banksTotal) % Data->pagesPerBlock;
}

// badBlockTable is a bit array with 8 virtual blocks in one bit entry
static int isGoodBlock(uint8_t* badBlockTable, uint32_t virtualBlock) {
	int index = virtualBlock/8;
	return ((badBlockTable[index / 8] >> (7 - (index % 8))) & 0x1) == 0x1;
}

static int virtual_block_to_physical_block(int virtualBank, int virtualBlock) {
	if(isGoodBlock(pstVFLCxt[virtualBank].badBlockTable, virtualBlock))
		return virtualBlock;

	int pwDesPbn;
	for(pwDesPbn = 0; pwDesPbn < pstVFLCxt[virtualBank].numReservedBlocks; pwDesPbn++) {
		if(pstVFLCxt[virtualBank].reservedBlockPoolMap[pwDesPbn] == virtualBlock) {
			if(pwDesPbn >= Data->blocksPerBank) {
				bufferPrintf("ftl: Destination physical block for remapping is greater than number of blocks per bank!");
			}
			return pstVFLCxt[virtualBank].reservedBlockPoolStart + pwDesPbn;
		}
	}

	return virtualBlock;
}

int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* did_error) {
	if(did_error) {
		*did_error = FALSE;
	}

	VFLData1.field_8++;
	VFLData1.field_20++;

	uint32_t dwVpn = virtualPageNumber + (Data->pagesPerSubBlk * Data2->field_4);
	if(dwVpn >= Data->pagesTotal) {
		bufferPrintf("ftl: dwVpn overflow: %d\r\n", dwVpn);
		return ERROR_ARG;
	}

	if(dwVpn < Data->pagesPerSubBlk) {
		bufferPrintf("ftl: dwVpn underflow: %d\r\n", dwVpn);
	}

	int virtualBank;
	int virtualBlock;
	int virtualPage;
	int physicalBlock;

	virtual_page_number_to_virtual_address(dwVpn, &virtualBank, &virtualBlock, &virtualPage);
	physicalBlock = virtual_block_to_physical_block(virtualBank, virtualBlock);

	int page = physicalBlock * Data->pagesPerBlock + virtualPage;

	bufferPrintf("ftl: mapping %d to %d, %d (%d), %d - %d\r\n", dwVpn, virtualBank, physicalBlock, virtualBlock, virtualPage, page);

	int ret = nand_read(virtualBank, page, buffer, spare, TRUE, TRUE);

	if(!empty_ok && ret == ERROR_EMPTYBLOCK) {
		ret = ERROR_NAND;
	}

	if(did_error) {
		if((Data->field_2F > 0 && ret == 0) || ret == ERROR_NAND) {
			*did_error = TRUE;
		}
	}

	if(ret == ERROR_ARG || ret == ERROR_NAND) {
		nand_bank_reset(virtualBank, 100);
		ret = nand_read(virtualBank, page, buffer, spare, TRUE, TRUE);
		if(!empty_ok && ret == ERROR_EMPTYBLOCK) {
			return ERROR_NAND;
		}

		if(ret == ERROR_ARG || ret == ERROR_NAND)
			return ret;
	}

	if(ret == ERROR_EMPTYBLOCK) {
		if(spare) {
			memset(spare, 0xFF, sizeof(SpareData));
		}
	}

	return 0;
}

static int VFL_Open() {
	int bank = 0;
	for(bank = 0; bank < Data->banksTotal; bank++) {
		if(!findDeviceInfoBBT(bank, pstBBTArea)) {
			bufferPrintf("ftl: findDeviceInfoBBT failed\r\n");
			return -1;
		}

		if(bank >= Data->banksTotal) {
			return -1;
		}


		VFLCxt* curVFLCxt = &pstVFLCxt[bank];
		uint8_t* pageBuffer = malloc(Data->bytesPerPage);
		uint8_t* spareBuffer = malloc(Data->bytesPerSpare);
		if(pageBuffer == NULL || spareBuffer == NULL) {
			bufferPrintf("ftl: cannot allocate page and spare buffer\r\n");
			return -1;
		}

		int i = 1;
		for(i = 1; i < Data2->field_0; i++) {
			// so pstBBTArea is a bit array of some sort
			if(!(pstBBTArea[i / 8] & (1 << (i  & 0x7))))
				continue;

			if(vfl_read_page(bank, i, 0, pageBuffer, spareBuffer) == TRUE) {
				memcpy(curVFLCxt->VFLCxtBlock, ((VFLCxt*)pageBuffer)->VFLCxtBlock, sizeof(curVFLCxt->VFLCxtBlock));
				break;
			}
		}

		if(i == Data2->field_0) {
			bufferPrintf("ftl: cannot find readable VFLCxtBlock\r\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		int maxSpareData = 0xFFFFFFFF;
		int maxSpareDataIdx = 4;
		for(i = 0; i < 4; i++) {
			uint16_t block = curVFLCxt->VFLCxtBlock[i];
			if(block == 0xFFFF)
				continue;

			if(vfl_read_page(bank, block, 0, pageBuffer, spareBuffer) != TRUE)
				continue;

			if(*((uint32_t*)spareBuffer) > 0 && *((uint32_t*)spareBuffer) <= maxSpareData) {
				maxSpareData = *((uint32_t*)spareBuffer);
				maxSpareDataIdx = i;
			}
		}

		if(maxSpareDataIdx == 4) {
			bufferPrintf("ftl: cannot find readable VFLCxtBlock index in spares\r\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		int page = 8;
		int last = 0;
		for(page = 8; page < Data->pagesPerBlock; page += 8) {
			if(vfl_read_page(bank, curVFLCxt->VFLCxtBlock[maxSpareDataIdx], page, pageBuffer, spareBuffer) == FALSE) {
				break;
			}
			
			last = page;
		}

		if(vfl_read_page(bank, curVFLCxt->VFLCxtBlock[maxSpareDataIdx], last, pageBuffer, spareBuffer) == FALSE) {
			bufferPrintf("ftl: cannot find readable VFLCxt\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		// Aha, so the upshot is that this finds the VFLCxt and copies it into pstVFLCxt
		memcpy(&pstVFLCxt[bank], pageBuffer, sizeof(VFLCxt));
		if(curVFLCxt->field_0 >= VFLData4) {
			VFLData4 = curVFLCxt->field_0;
		}

		free(pageBuffer);
		free(spareBuffer);

		if(vfl_check_checksum(bank) == FALSE) {
			bufferPrintf("ftl: VFLCxt has bad checksum\n");
			return -1;
		}
	} 

	int max = 0;
	void* maxThing = NULL;
	uint8_t buffer[6];
	for(bank = 0; bank < Data->banksTotal; bank++) {
		int cur = pstVFLCxt[bank].field_0;
		if(max <= cur) {
			max = cur;
			maxThing = pstVFLCxt[bank].field_4;
		}
	}

	memcpy(buffer, maxThing, 6);

	for(bank = 0; bank < Data->banksTotal; bank++) {
		memcpy(pstVFLCxt[bank].field_4, buffer, 6);
		vfl_gen_checksum(bank);
	}

	return 0;
}

static int FTL_Open() {
	return -1;
}

int ftl_setup() {
	Data = nand_get_geometry();
	Data2 = nand_get_data();

	if(VFL_Init() != 0) {
		bufferPrintf("ftl: VFL_Init failed\r\n");
		return -1;
	}

	if(FTL_Init() != 0) {
		bufferPrintf("ftl: FTL_Init failed\r\n");
		return -1;
	}

	int i;
	int foundSignature = FALSE;

	uint8_t* buffer = malloc(Data->bytesPerPage);
	for(i = 0; i < Data->pagesPerBlock; i++) {
		if(nand_read_alternate_ecc(0, i, buffer) == 0 && *((uint32_t*) buffer) == FTL_ID) {
			foundSignature = TRUE;
			break;
		}
	}
	free(buffer);

	if(!foundSignature || !hasDeviceInfoBBT()) {
		bufferPrintf("ftl: no signature or production format.\r\n");
		return -1;
	}

	if(VFL_Open() != 0) {
		bufferPrintf("ftl: VFL_Open failed\r\n");
		return -1;
	}

	if(FTL_Open() != 0) {
		bufferPrintf("ftl: FTL_Open failed\r\n");
		return -1;
	}

	return 0;
}

