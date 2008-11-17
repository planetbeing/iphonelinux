#include "openiboot.h"
#include "ftl.h"
#include "nand.h"
#include "util.h"

#define FTL_ID_V1 0x43303033
#define FTL_ID_V2 0x43303034

int HasFTLInit = FALSE;

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
			if(badBlockCount > 2) {
				DebugPrintf("ftl: findDeviceInfoBBT - too many bad pages, skipping block %d\r\n", block);
				break;
			}

			int ret = nand_read_alternate_ecc(bank, (block * Data->pagesPerBlock) + page, buffer);
			if(ret != 0) {
				if(ret == 1) {
					DebugPrintf("ftl: findDeviceInfoBBT - found 'badBlock' on bank %d, page %d\r\n", (block * Data->pagesPerBlock) + page);
					badBlockCount++;
				}

				DebugPrintf("ftl: findDeviceInfoBBT - skipping bank %d, page %d\r\n", (block * Data->pagesPerBlock) + page);
				continue;
			}

			if(memcmp(buffer, "DEVICEINFOBBT\0\0\0", 16) == 0) {
				if(deviceInfoBBT) {
					memcpy(deviceInfoBBT, buffer + 0x38, *((uint32_t*)(buffer + 0x34)));
				}

				free(buffer);
				return TRUE;
			} else {
				DebugPrintf("ftl: did not find signature on bank %d, page %d\r\n", (block * Data->pagesPerBlock) + page);
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
static uint32_t* ScatteredPageNumberBuffer = NULL;
static uint16_t* ScatteredBankNumberBuffer = NULL;
static int VFLData4 = 0;
static uint8_t VFLData5[0xF8];

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

	if(ScatteredPageNumberBuffer == NULL && ScatteredBankNumberBuffer == NULL) {
		ScatteredPageNumberBuffer = (uint32_t*) malloc(Data->pagesPerSubBlk * 4);
		ScatteredBankNumberBuffer = (uint16_t*) malloc(Data->pagesPerSubBlk * 4);
		if(ScatteredPageNumberBuffer == NULL || ScatteredBankNumberBuffer == NULL)
			return -1;
	}

	VFLData4 = 0;

	return 0;
}

static FTLData1Type FTLData1;
static FTLCxt* pstFTLCxt;
static FTLCxt* FTLCxtBuffer;
static SpareData* FTLSpareBuffer;
static uint32_t* ScatteredVirtualPageNumberBuffer;
static uint8_t* StoreCxt;

static int FTL_Init() {
	int numPagesToWriteInStoreCxt = 0;

	int x = ((Data->userSubBlksTotal + 23) * sizeof(uint16_t)) / Data->bytesPerPage;
	if((((Data->userSubBlksTotal + 23) * sizeof(uint16_t)) % Data->bytesPerPage) != 0) {
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

	memset(&FTLData1, 0, 0x58);

	if(pstFTLCxt == NULL) {
		pstFTLCxt = FTLCxtBuffer = (FTLCxt*) malloc(sizeof(FTLCxt));
		if(pstFTLCxt == NULL)
			return -1;
		memset(pstFTLCxt->field_3D8, 0, sizeof(pstFTLCxt->field_3D8)); 
	}

	pstFTLCxt->field_31C = 0;

	pstFTLCxt->dataVbn = (uint16_t*) malloc(Data->userSubBlksTotal * sizeof(uint16_t));
	pstFTLCxt->field_1A0 = (uint16_t*) malloc((Data->pagesPerSubBlk * 18) * sizeof(uint16_t));
	pstFTLCxt->field_19C = (uint16_t*) malloc((Data->userSubBlksTotal + 23) * sizeof(uint16_t));
	pstFTLCxt->field_3B0 = (uint16_t*) malloc((Data->userSubBlksTotal + 23) * sizeof(uint16_t));

	FTLSpareBuffer = (SpareData*) malloc(Data->pagesPerSubBlk * sizeof(SpareData));

	if((Data->pagesPerSubBlk / 8) >= numPagesToWriteInStoreCxt) {
		numPagesToWriteInStoreCxt = Data->pagesPerSubBlk / 8;
	}

	StoreCxt = malloc(Data->bytesPerPage * numPagesToWriteInStoreCxt);
	ScatteredVirtualPageNumberBuffer = (uint32_t*) malloc(Data->pagesPerSubBlk * sizeof(uint32_t*));

	if(!pstFTLCxt->dataVbn || !pstFTLCxt->field_1A0 || !pstFTLCxt->field_19C || !FTLCxtBuffer->field_3B0 || ! FTLSpareBuffer || !StoreCxt || !ScatteredVirtualPageNumberBuffer)
		return -1;

	int i;
	for(i = 0; i < 18; i++) {
		pstFTLCxt->pLog[i].field_8 = pstFTLCxt->field_1A0 + (i * Data->pagesPerSubBlk);
		memset(pstFTLCxt->pLog[i].field_8, 0xFF, Data->pagesPerSubBlk * 2);
		pstFTLCxt->pLog[i].field_10 = 1;
		pstFTLCxt->pLog[i].field_C = 0;
		pstFTLCxt->pLog[i].field_E = 0;
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

static void virtual_page_number_to_virtual_address(uint32_t dwVpn, uint16_t* virtualBank, uint16_t* virtualBlock, uint16_t* virtualPage) {
	*virtualBank = dwVpn % Data->banksTotal;
	*virtualBlock = dwVpn / Data->pagesPerSubBlk;
	*virtualPage = (dwVpn / Data->banksTotal) % Data->pagesPerBlock;
}

// badBlockTable is a bit array with 8 virtual blocks in one bit entry
static int isGoodBlock(uint8_t* badBlockTable, uint16_t virtualBlock) {
	int index = virtualBlock/8;
	return ((badBlockTable[index / 8] >> (7 - (index % 8))) & 0x1) == 0x1;
}

static uint16_t virtual_block_to_physical_block(uint16_t virtualBank, uint16_t virtualBlock) {
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

int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* refresh_page) {
	if(refresh_page) {
		*refresh_page = FALSE;
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

	uint16_t virtualBank;
	uint16_t virtualBlock;
	uint16_t virtualPage;
	uint16_t physicalBlock;

	virtual_page_number_to_virtual_address(dwVpn, &virtualBank, &virtualBlock, &virtualPage);
	physicalBlock = virtual_block_to_physical_block(virtualBank, virtualBlock);

	int page = physicalBlock * Data->pagesPerBlock + virtualPage;

	int ret = nand_read(virtualBank, page, buffer, spare, TRUE, TRUE);

	if(!empty_ok && ret == ERROR_EMPTYBLOCK) {
		ret = ERROR_NAND;
	}

	if(refresh_page) {
		if((Data->field_2F <= 0 && ret == 0) || ret == ERROR_NAND) {
			bufferPrintf("ftl: setting refresh_page to TRUE due to the following factors: Data->field_2F = %x, ret = %d\r\n", Data->field_2F, ret);
			*refresh_page = TRUE;
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

	return ret;
}

static int VFL_ReadMultiplePagesInVb(int logicalBlock, int logicalPage, int count, uint8_t* main, SpareData* spare, int* refresh_page) {
	int i;
	int currentPage = logicalPage; 
	for(i = 0; i < count; i++) {
		int ret = VFL_Read((logicalBlock * Data->pagesPerSubBlk) + currentPage, main + (Data->bytesPerPage * i), (uint8_t*) &spare[i], TRUE, refresh_page);
		currentPage++;
		if(ret != 0)
			return FALSE;
	}
	return TRUE;
}

static int VFL_ReadScatteredPagesInVb(uint32_t* virtualPageNumber, int count, uint8_t* main, SpareData* spare, int* refresh_page) {
	VFLData1.field_8 += count;
	VFLData1.field_20++;

	if(refresh_page) {
		*refresh_page = FALSE;
	}

	int i = 0;
	for(i = 0; i < count; i++) {
		uint32_t dwVpn = virtualPageNumber[i] + (Data->pagesPerSubBlk * Data2->field_4);

		uint16_t virtualBlock;
		uint16_t virtualPage;
		uint16_t physicalBlock;

		virtual_page_number_to_virtual_address(dwVpn, &ScatteredBankNumberBuffer[i], &virtualBlock, &virtualPage);
		physicalBlock = virtual_block_to_physical_block(ScatteredBankNumberBuffer[i], virtualBlock);
		ScatteredPageNumberBuffer[i] = physicalBlock * Data->pagesPerBlock + virtualPage;
	}

	int ret = nand_read_multiple(ScatteredBankNumberBuffer, ScatteredPageNumberBuffer, main, spare, count);
	if(Data->field_2F <= 0 && refresh_page != NULL) {
		bufferPrintf("ftl: VFL_ReadScatteredPagesInVb mark page for refresh\r\n");
		*refresh_page = TRUE;
	}

	if(ret != 0)
		return FALSE;
	else
		return TRUE;
}

// sub_18015A9C
static uint16_t* VFL_get_maxThing() {
	int bank = 0;
	int max = 0;
	uint16_t* maxThing = NULL;
	for(bank = 0; bank < Data->banksTotal; bank++) {
		int cur = pstVFLCxt[bank].field_0;
		if(max <= cur) {
			max = cur;
			maxThing = pstVFLCxt[bank].field_4;
		}
	}

	return maxThing;
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

		int minLpn = 0xFFFFFFFF;
		int VFLCxtIdx = 4;
		for(i = 0; i < 4; i++) {
			uint16_t block = curVFLCxt->VFLCxtBlock[i];
			if(block == 0xFFFF)
				continue;

			if(vfl_read_page(bank, block, 0, pageBuffer, spareBuffer) != TRUE)
				continue;

			SpareData* spareData = (SpareData*) spareBuffer;
			if(spareData->logicalPageNumber > 0 && spareData->logicalPageNumber <= minLpn) {
				minLpn = spareData->logicalPageNumber;
				VFLCxtIdx = i;
			}
		}

		if(VFLCxtIdx == 4) {
			bufferPrintf("ftl: cannot find readable VFLCxtBlock index in spares\r\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		int page = 8;
		int last = 0;
		for(page = 8; page < Data->pagesPerBlock; page += 8) {
			if(vfl_read_page(bank, curVFLCxt->VFLCxtBlock[VFLCxtIdx], page, pageBuffer, spareBuffer) == FALSE) {
				break;
			}
			
			last = page;
		}

		if(vfl_read_page(bank, curVFLCxt->VFLCxtBlock[VFLCxtIdx], last, pageBuffer, spareBuffer) == FALSE) {
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

	void* maxThing = VFL_get_maxThing();
	uint16_t buffer[3];

	memcpy(buffer, maxThing, 6);

	for(bank = 0; bank < Data->banksTotal; bank++) {
		memcpy(pstVFLCxt[bank].field_4, buffer, sizeof(buffer));
		vfl_gen_checksum(bank);
	}

	return 0;
}

void FTL_64bit_sum(uint64_t* src, uint64_t* dest, int size) {
	int i;
	for(i = 0; i < size / sizeof(uint64_t); i++) {
		dest[i] += src[i];
	}
}

static int FTL_Restore() {
	return FALSE;
}

static int FTL_GetStruct(FTLStruct type, void** data, int* size) {
	switch(type) {
		case FTLData1SID:
			*data = &FTLData1;
			*size = sizeof(FTLData1);
			return TRUE;
		default:
			return FALSE;
	}
}

static int VFL_GetStruct(FTLStruct type, void** data, int* size) {
	switch(type) {
		case VFLData1SID:
			*data = &VFLData1;
			*size = sizeof(VFLData1);
			return TRUE;
		case VFLData5SID:
			*data = VFLData5;
			*size = 0xF8;
			return TRUE;
		default:
			return FALSE;
	}
}

static int sum_data(uint8_t* pageBuffer) {
	void* data;
	int size;
	FTL_GetStruct(FTLData1SID, &data, &size);
	FTL_64bit_sum((uint64_t*)pageBuffer, (uint64_t*)data, size);
	VFL_GetStruct(VFLData1SID, &data, &size);
	FTL_64bit_sum((uint64_t*)(pageBuffer + 0x200), (uint64_t*)data, size);
	VFL_GetStruct(VFLData5SID, &data, &size);
	FTL_64bit_sum((uint64_t*)(pageBuffer + 0x400), (uint64_t*)data, size);
	return TRUE;
}

static int FTL_Open(int* pagesAvailable, int* bytesPerPage) {
	int refreshPage;
	int ret;
	int i;

	uint16_t* dataVbn = pstFTLCxt->dataVbn;
	uint16_t* field_19C = pstFTLCxt->field_19C;
	void* field_3B0 = pstFTLCxt->field_3B0;
	uint16_t* field_1A0 = pstFTLCxt->field_1A0;

	void* thing;
	if((thing = VFL_get_maxThing()) == NULL)
		goto FTL_Open_Error;
	
	memcpy(pstFTLCxt->thing, thing, sizeof(pstFTLCxt->thing));

	uint8_t* pageBuffer = malloc(Data->bytesPerPage);
	uint8_t* spareBuffer = malloc(Data->bytesPerSpare);
	if(!pageBuffer || !spareBuffer) {
		bufferPrintf("ftl: FTL_Open ran out of memory!\r\n");
		return ERROR_ARG;
	}

	uint32_t ftlCxtBlock = 0xffff;
	uint32_t minLpn = 0xffffffff;
	for(i = 0; i < sizeof(pstFTLCxt->thing)/sizeof(uint16_t); i++) {
		ret = VFL_Read(Data->pagesPerSubBlk * pstFTLCxt->thing[i], pageBuffer, spareBuffer, TRUE, &refreshPage);
		if(ret == ERROR_ARG) {
			free(pageBuffer);
			free(spareBuffer);
			goto FTL_Open_Error;
		}

		SpareData* spareData = (SpareData*) spareBuffer;
		if((spareData->field_9 - 0x43) > 0xC)
			continue;

		if(ret != 0)
			continue;

		if(ftlCxtBlock != 0xffff && spareData->logicalPageNumber >= minLpn)
			continue;

		minLpn = spareData->logicalPageNumber;
		ftlCxtBlock = pstFTLCxt->thing[i];
	}


	if(ftlCxtBlock == 0xffff) {
		bufferPrintf("ftl: Cannot find context!\r\n");
		goto FTL_Open_Error_Release;
	}

	bufferPrintf("ftl: Successfully found FTL context block: %d\r\n", ftlCxtBlock);

	int ftlCxtFound = FALSE;
	for(i = Data->pagesPerSubBlk - 1; i > 0; i--) {
		ret = VFL_Read(Data->pagesPerSubBlk * ftlCxtBlock + i, pageBuffer, spareBuffer, TRUE, &refreshPage);
		if(ret == 1) {
			continue;
		} else if(ret == 0 && ((SpareData*)spareBuffer)->field_9 == 0x43) {
			memcpy(FTLCxtBuffer, pageBuffer, sizeof(FTLCxt));
			ftlCxtFound = TRUE;
			break;
		} else {
			ftlCxtFound = FALSE;
			break;
		}
	}

	// Restore now possibly overwritten (by data from NAND) pointers from backed up copies
	pstFTLCxt->dataVbn = dataVbn;
	pstFTLCxt->field_19C = field_19C;
	pstFTLCxt->field_3B0 = field_3B0;
	pstFTLCxt->field_1A0 = field_1A0;

	for(i = 0; i < 18; i++) {
		pstFTLCxt->pLog[i].field_8 = pstFTLCxt->field_1A0 + (i * Data->pagesPerSubBlk);
	}

	if(!ftlCxtFound)
		goto FTL_Open_Error_Release;

	bufferPrintf("ftl: Successfully read FTL context block.\r\n");

	int pagesToRead;

	pagesToRead = (Data->userSubBlksTotal * 2) / Data->bytesPerPage;
	if(((Data->userSubBlksTotal * 2) % Data->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_dataVbn[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
			goto FTL_Open_Error_Release;

		int toRead = Data->bytesPerPage;
		if(toRead > ((Data->userSubBlksTotal * 2) - (i * Data->bytesPerPage))) {
			toRead = (Data->userSubBlksTotal * 2) - (i * Data->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->dataVbn) + (i * Data->bytesPerPage), pageBuffer, toRead);	
	}

	pagesToRead = (Data->pagesPerSubBlk * 34) / Data->bytesPerPage;
	if(((Data->pagesPerSubBlk * 34) % Data->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_1A0[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
			goto FTL_Open_Error_Release;

		int toRead = Data->bytesPerPage;
		if(toRead > ((Data->pagesPerSubBlk * 34) - (i * Data->bytesPerPage))) {
			toRead = (Data->pagesPerSubBlk * 34) - (i * Data->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->field_1A0) + (i * Data->bytesPerPage), pageBuffer, toRead);	
	}

	pagesToRead = ((Data->userSubBlksTotal + 23) * sizeof(uint16_t)) / Data->bytesPerPage;
	if((((Data->userSubBlksTotal + 23) * sizeof(uint16_t)) % Data->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_19C[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
			goto FTL_Open_Error_Release;

		int toRead = Data->bytesPerPage;
		if(toRead > (((Data->pagesPerSubBlk + 23) * sizeof(uint16_t)) - (i * Data->bytesPerPage))) {
			toRead = ((Data->pagesPerSubBlk + 23) * sizeof(uint16_t)) - (i * Data->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->field_19C) + (i * Data->bytesPerPage), pageBuffer, toRead);	
	}

	int success = FALSE;

	bufferPrintf("ftl: Detected version %x %x\r\n", FTLCxtBuffer->versionLower, FTLCxtBuffer->versionUpper);
	if(FTLCxtBuffer->versionLower == 0x46560001 && FTLCxtBuffer->versionUpper == 0xB9A9FFFE) {
		pagesToRead = ((Data->userSubBlksTotal + 23) * sizeof(uint16_t)) / Data->bytesPerPage;
		if((((Data->userSubBlksTotal + 23) * sizeof(uint16_t)) % Data->bytesPerPage) != 0)
			pagesToRead++;

		success = TRUE;
		for(i = 0; i < pagesToRead; i++) {
			if(VFL_Read(pstFTLCxt->pages_for_3B0[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0) {
				success = FALSE;
				break;
			}

			int toRead = Data->bytesPerPage;
			if(toRead > (((Data->pagesPerSubBlk + 23) * sizeof(uint16_t)) - (i * Data->bytesPerPage))) {
				toRead = ((Data->pagesPerSubBlk + 23) * sizeof(uint16_t)) - (i * Data->bytesPerPage);
			}

			memcpy(((uint8_t*)pstFTLCxt->field_3B0) + (i * Data->bytesPerPage), pageBuffer, toRead);	
		}

		if((pstFTLCxt->field_3D4 + 1) == 0) {
			int x = pstFTLCxt->field_3D0 / Data->pagesPerSubBlk;
			if(x == 0 || x <= Data->userSubBlksTotal) {
				if(VFL_Read(pstFTLCxt->field_3D0, pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
					goto FTL_Open_Error_Release;

				sum_data(pageBuffer);
			}
		}
	} else {
		bufferPrintf("ftl: updating the FTL from seemingly compatible version\r\n");
		for(i = 0; i < (Data->userSubBlksTotal + 23); i++) {
			pstFTLCxt->field_3B0[i] = 0x1388;
		}

		for(i = 0; i < 5; i++) {
			pstFTLCxt->elements2[i].field_0 = -1;
			pstFTLCxt->elements2[i].field_2 = -1;
		}

		pstFTLCxt->field_3C8 = 0;
		pstFTLCxt->field_31C = 0;
		FTLCxtBuffer->versionLower = 0x46560000;
		FTLCxtBuffer->versionUpper = 0xB9A9FFFF;

		success = TRUE;
	}

	if(success) {
		bufferPrintf("ftl: FTL successfully opened!\r\n");
		free(pageBuffer);
		free(spareBuffer);
		*pagesAvailable = Data->userPagesTotal;
		*bytesPerPage = Data->bytesPerPage;
		return 0;
	}

FTL_Open_Error_Release:
	free(pageBuffer);
	free(spareBuffer);

FTL_Open_Error:
	bufferPrintf("ftl: FTL_Open cannot load FTLCxt!\r\n");
	if(FTL_Restore() != FALSE) {
		*pagesAvailable = Data->userPagesTotal;
		*bytesPerPage = Data->bytesPerPage;
		return 0;
	} else {
		return ERROR_ARG;
	}
}

uint32_t FTL_map_page(FTLCxtLog* pLog, int lbn, int offset) {
	if(pLog && pLog->field_8[offset] != 0xFFFF) {
		if(((pLog->wVbn * Data->pagesPerSubBlk) + pLog->field_8[offset] + 1) != 0)
			return (pLog->wVbn * Data->pagesPerSubBlk) + pLog->field_8[offset];
	}

	return (pstFTLCxt->dataVbn[lbn] * Data->pagesPerSubBlk) + offset;
}

int FTL_Read(int logicalPageNumber, int totalPagesToRead, uint8_t* pBuf) {
	int i;
	int hasError = FALSE;

	FTLData1.field_8 += totalPagesToRead;
	FTLData1.field_18++;
	pstFTLCxt->field_3CC++;

	if(!pBuf) {
		return ERROR_ARG;
	}

	if(totalPagesToRead == 0 || (logicalPageNumber + totalPagesToRead) >= Data->userPagesTotal) {
		bufferPrintf("ftl: invalid input parameters\r\n");
		return ERROR_INPUT;
	}

	int lbn = logicalPageNumber / Data->pagesPerSubBlk;
	int offset = logicalPageNumber - (lbn * Data->pagesPerSubBlk);
	
	uint8_t* pageBuffer = malloc(Data->bytesPerPage);
	uint8_t* spareBuffer = malloc(Data->bytesPerSpare);
	if(!pageBuffer || !spareBuffer) {
		bufferPrintf("ftl: FTL_Read ran out of memory!\r\n");
		return ERROR_ARG;
	}

	FTLCxtLog* pLog = NULL;
	for(i = 0; i < 17; i++) {
		if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
			continue;

		if(pstFTLCxt->pLog[i].field_6 == lbn) {
			pLog = &pstFTLCxt->pLog[i];
			break;
		}
	}

	int ret = 0;
	int pagesRead = 0;
	int pagesToRead;
	int refreshPage;
	int currentLogicalPageNumber = logicalPageNumber;

	while(TRUE) {
		pagesToRead = Data->pagesPerSubBlk - offset;
		if(pagesToRead >= (totalPagesToRead - pagesRead))
			pagesToRead = totalPagesToRead - pagesRead;

		int readSuccessful;
		if(pLog != NULL) {
			for(i = 0; i < pagesToRead; i++) {
				ScatteredVirtualPageNumberBuffer[i] = FTL_map_page(pLog, lbn, offset + i);
				if((ScatteredVirtualPageNumberBuffer[i] / Data->pagesPerSubBlk) == pLog->wVbn) {
					pstFTLCxt->field_3B0[ScatteredVirtualPageNumberBuffer[i] / Data->pagesPerSubBlk]++;
				} else {
					pstFTLCxt->field_3B0[pstFTLCxt->dataVbn[ScatteredVirtualPageNumberBuffer[i] / Data->pagesPerSubBlk]]++;
				}
			}

			readSuccessful = VFL_ReadScatteredPagesInVb(ScatteredVirtualPageNumberBuffer, pagesToRead, pBuf + (pagesRead * Data->bytesPerPage), FTLSpareBuffer, &refreshPage);
			if(refreshPage) {
				bufferPrintf("ftl: _AddLbnToRefreshList (0x%x, 0x%x, 0x%x)\r\n", lbn, pstFTLCxt->dataVbn[lbn], pLog->wVbn);
			}
		} else {
			// VFL_ReadMultiplePagesInVb has a different calling convention and implementation than the equivalent iBoot function.
			// Ours is a bit less optimized, and just calls VFL_Read for each page.
			readSuccessful = VFL_ReadMultiplePagesInVb(pstFTLCxt->dataVbn[lbn], offset, pagesToRead, pBuf + (pagesRead * Data->bytesPerPage), FTLSpareBuffer, &refreshPage);
			if(refreshPage) {
				bufferPrintf("ftl: _AddLbnToRefreshList (0x%x, 0x%x)\r\n", lbn, pstFTLCxt->dataVbn[lbn]);
			}
		}

		int loop = 0;
		if(readSuccessful) {
			for(i = 0; i < pagesToRead; i++) {
				if(FTLSpareBuffer[i].eccMark == 0xFF)
					continue;

				bufferPrintf("ftl: CHECK_FTL_ECC_MARK (0x%x, 0x%x, 0x%x, 0x%x)\r\n", lbn, offset, i, FTLSpareBuffer[i].eccMark);
				hasError = TRUE;
			}

			pagesRead += pagesToRead;
			currentLogicalPageNumber += pagesToRead;
			offset += pagesToRead;

			if(pagesRead == totalPagesToRead) {
				goto FTL_Read_Done;
			}

			loop = FALSE;
		} else {
			loop = TRUE;
		}

		do {
			if(pagesRead != totalPagesToRead && Data->pagesPerSubBlk != offset) {
				int virtualPage = FTL_map_page(pLog, lbn, offset);
				ret = VFL_Read(virtualPage, pBuf + (Data->bytesPerPage * pagesRead), spareBuffer, TRUE, &refreshPage);
				if(refreshPage) {
					bufferPrintf("ftl: _AddLbnToRefreshList (0x%x, 0x%x)\r\n", lbn, virtualPage / Data->pagesPerSubBlk);
				}

				if(ret == ERROR_ARG)
					goto FTL_Read_Error_Release;

				if(ret == ERROR_NAND || ((SpareData*) spareBuffer)->eccMark != 0xFF) {
					bufferPrintf("ftl: ECC error, ECC mark is: %x\r\n", ((SpareData*) spareBuffer)->eccMark);
					hasError = TRUE;
					if(pLog) {
						virtualPage = FTL_map_page(pLog, lbn, offset);
						bufferPrintf("ftl: lbn 0x%x pLog->wVbn 0x%x dataVbn 0x%x offset 0x%x vpn 0x%x\r\n", lbn, pLog->wVbn, pstFTLCxt->dataVbn[lbn], offset, virtualPage);
					} else {
						virtualPage = FTL_map_page(NULL, lbn, offset);
						bufferPrintf("ftl: lbn 0x%x dataVbn 0x%x offset 0x%x vpn 0x%x\r\n", lbn, pstFTLCxt->dataVbn[lbn], offset, virtualPage);
					}
				}

				if(ret == 0) {
					if(((SpareData*) spareBuffer)->logicalPageNumber != offset) {
						bufferPrintf("ftl: error, dwWrittenLpn(0x%x) != dwLpn(0x%x)\r\n", ((SpareData*) spareBuffer)->logicalPageNumber, offset);
					}
				}

				pagesRead++;
				currentLogicalPageNumber++;
				offset++;
				if(pagesRead == totalPagesToRead) {
					goto FTL_Read_Done;
				}
			}

			if(offset == Data->pagesPerSubBlk) {
				lbn++;
				if(lbn >= Data->userSubBlksTotal)
					goto FTL_Read_Error_Release;

				pLog = NULL;
				for(i = 0; i < 17; i++) {
					if(pstFTLCxt->pLog[i].wVbn != 0xFFFF && pstFTLCxt->pLog[i].field_6 == lbn) {
						pLog = &pstFTLCxt->pLog[i];
						break;
					}
				}

				offset = 0;
				break;
			}
		} while(loop);
	}

FTL_Read_Done:
	free(pageBuffer);
	free(spareBuffer);
	if(hasError) {
		bufferPrintf("ftl: USER_DATA_ERROR, failed with (0x%x, 0x%x, 0x%x)\r\n", logicalPageNumber, totalPagesToRead, pBuf);
		return ERROR_NAND;
	}

	return 0;

FTL_Read_Error_Release:
	free(pageBuffer);
	free(spareBuffer);
	bufferPrintf("ftl: _FTLRead error!\r\n");
	return ret;
}

int ftl_setup() {
	if(HasFTLInit)
		return 0;

	nand_setup();

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

	DebugPrintf("ftl: Attempting to read %d pages from first block of first bank.\r\n", Data->pagesPerBlock);
	uint8_t* buffer = malloc(Data->bytesPerPage);
	for(i = 0; i < Data->pagesPerBlock; i++) {
		int ret;
		if((ret = nand_read_alternate_ecc(0, i, buffer)) == 0) {
			uint32_t id = *((uint32_t*) buffer);
			if(id == FTL_ID_V1 || id == FTL_ID_V2) {
				bufferPrintf("ftl: Found production format: %x\r\n", id);
				foundSignature = TRUE;
				break;
			} else {
				DebugPrintf("ftl: Found non-matching signature: %x\r\n", ((uint32_t*) buffer));
			}
		} else {
			DebugPrintf("ftl: page %d of first bank is unreadable: %x!\r\n", i, ret);
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

	int pagesAvailable;
	int bytesPerPage;
	if(FTL_Open(&pagesAvailable, &bytesPerPage) != 0) {
		bufferPrintf("ftl: FTL_Open failed\r\n");
		return -1;
	}

	HasFTLInit = TRUE;

	return 0;
}

int ftl_read(void* buffer, uint64_t offset, int size) {
	uint8_t* curLoc = (uint8_t*) buffer;
	int curPage = offset / Data->bytesPerPage;
	int toRead = size;
	int pageOffset = offset - (curPage * Data->bytesPerPage);
	uint8_t* tBuffer = (uint8_t*) malloc(Data->bytesPerPage);
	while(toRead > 0) {
		if(FTL_Read(curPage, 1, tBuffer) != 0) {
			free(tBuffer);
			return FALSE;
		}

		int read = ((Data->bytesPerPage > toRead) ? toRead : Data->bytesPerPage);
		memcpy(curLoc, tBuffer + pageOffset, read);
		curLoc += read;
		toRead -= read;
		pageOffset = 0;
		curPage++;
	}

	free(tBuffer);
	return TRUE;
}
