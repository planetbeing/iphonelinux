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

static uint8_t VFLData1[0x48];
static void* pstVFLCxt = NULL;
static void* pstBBTArea = NULL;
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
		pstBBTArea = malloc((Data->blocksPerBank + 7) / 8);
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

static int VFL_Open() {
	return -1;
}

static int FTL_Open() {
	return -1;
}

int ftl_setup() {
	Data = nand_get_geometry();

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

