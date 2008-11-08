#include "openiboot.h"
#include "ftl.h"
#include "nand.h"
#include "util.h"

#define FTL_ID 0x43303033

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

static uint8_t VFLData1[0x48];
static void* pstVFLCxt = NULL;
static uint8_t* pstBBTArea = NULL;
static void* VFLData2 = NULL;
static void* VFLData3 = NULL;
static int VFLData4 = 0;

static int VFL_Init() {
	memset(VFLData1, 0, 0x48);
	if(pstVFLCxt == NULL) {
		pstVFLCxt = malloc(Data->banksTotal * 2048);
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
static int sub_18016120(int bank, int page, int option, uint8_t* pageBuffer, uint8_t* spareBuffer) {
	return FALSE;
}

static int VFLAuxFunction1(int bank) {
	return FALSE;
}

static int VFLAuxFunction2(int bank) {
	return FALSE;
}

static int VFL_Open() {
	int bank = 0;
	for(bank = 0; bank < Data->banksTotal; bank++) {
		if(findDeviceInfoBBT(bank, pstBBTArea) != 0) {
			bufferPrintf("ftl: findDeviceInfoBBT failed\r\n");
			return -1;
		}
		if(bank >= Data->banksTotal) {
			return -1;
		}


		uint8_t* r10 = ((uint8_t*)pstVFLCxt) + bank * 2048;
		uint8_t* pageBuffer = malloc(Data->bytesPerPage);
		uint8_t* spareBuffer = malloc(Data->bytesPerSpare);
		if(pageBuffer == NULL)
			return -1;

		int i = 1;
		for(i = 1; i < Data2->field_0; i++) {
			// so pstBBTArea is a bit array of some sort
			if(!(pstBBTArea[i / 8] & (1 << (i  & 0x7))))
				continue;

			if(sub_18016120(bank, i, 0, pageBuffer, spareBuffer) == TRUE) {
				memcpy(r10 + 0x7A2, pageBuffer + 0x7A2, 8);
				break;
			}
		}

		if(i == Data->field_0) {
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		int maxSpareData = 0xFFFFFFFF;
		int maxSpareDataIdx = 4;
		for(i = 0; i < 4; i++) {
			uint16_t r1 = *((uint16_t*)(r10 + 0x7A2 + 2 * i));
			if(r1 == 0xFFFF)
				continue;

			if(sub_18016120(bank, r1, 0, pageBuffer, spareBuffer) != TRUE)
				continue;

			if(*((uint32_t*)spareBuffer) > 0 && *((uint32_t*)spareBuffer) <= maxSpareData) {
				maxSpareData = *((uint32_t*)spareBuffer);
				maxSpareDataIdx = i;
			}
		}

		if(maxSpareDataIdx == 4) {
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		int page = 8;
		int last = 0;
		for(page = 8; page < Data->pagesPerBlock; page += 8) {
			if(sub_18016120(bank, *((uint16_t*)((r10 + (maxSpareDataIdx * 2) + 1952) + 2)), page, pageBuffer, spareBuffer) == FALSE) {
				break;
			}
			
			last = page;
		}

		if(sub_18016120(bank, *((uint16_t*)((r10 + (maxSpareDataIdx * 2) + 1952) + 2)), last, pageBuffer, spareBuffer) == FALSE) {
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		// Aha, so the upshot is that this finds the VFLCxt and copies it into pstVFLCxt
		memcpy((void**)(((uint8_t*)pstVFLCxt) + bank * 2048), pageBuffer, 2048);
		if(*((uint32_t*)r10) >= VFLData4) {
			VFLData4 = *((uint32_t*)r10);
		}

		free(pageBuffer);
		free(spareBuffer);

		if(VFLAuxFunction2(bank) == FALSE)
			return -1;
	} 

	int max = 0;
	void* maxThing = NULL;
	uint8_t buffer[6];
	for(bank = 0; bank < Data->banksTotal; bank++) {
		int cur = *((int*)(((uint8_t*)pstVFLCxt) + bank * 2048));
		if(max <= cur) {
			max = cur;
			maxThing = (void*)((((uint8_t*)pstVFLCxt) + bank * 2048) + 4);
		}
	}

	memcpy(buffer, maxThing, 6);

	for(bank = 0; bank < Data->banksTotal; bank++) {
		memcpy((void**)(((uint8_t*)pstVFLCxt) + bank * 2048), buffer, 6);
		VFLAuxFunction1(bank);
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

