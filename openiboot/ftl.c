#include "openiboot.h"
#include "ftl.h"
#include "nand.h"
#include "util.h"

#define FTL_ID_V1 0x43303033
#define FTL_ID_V2 0x43303034
#define FTL_ID_V3 0x43303035

int globalFtlHasBeenRestored; /* global variable to tell wether a ftl_restore has been done*/

int HasFTLInit = FALSE;
int CleanFreeVb = FALSE;

static NANDData* Geometry;
static NANDFTLData* FTLData;

static int vfl_commit_cxt(int bank);
static int ftl_set_free_vb(uint16_t block);
static int ftl_get_free_vb(uint16_t* block);
static int ftl_merge(FTLCxtLog* pLog);
static int ftl_commit_cxt();
static int ftl_open_read_counter_tables();

static int findDeviceInfoBBT(int bank, void* deviceInfoBBT) {
	uint8_t* buffer = malloc(Geometry->bytesPerPage);
	int lowestBlock = Geometry->blocksPerBank - (Geometry->blocksPerBank / 10);
	int block;
	for(block = Geometry->blocksPerBank - 1; block >= lowestBlock; block--) {
		int page;
		int badBlockCount = 0;
		for(page = 0; page < Geometry->pagesPerBlock; page++) {
			if(badBlockCount > 2) {
				DebugPrintf("ftl: findDeviceInfoBBT - too many bad pages, skipping block %d\r\n", block);
				break;
			}

			int ret = nand_read_alternate_ecc(bank, (block * Geometry->pagesPerBlock) + page, buffer);
			if(ret != 0) {
				if(ret == 1) {
					DebugPrintf("ftl: findDeviceInfoBBT - found 'badBlock' on bank %d, page %d\r\n", (block * Geometry->pagesPerBlock) + page);
					badBlockCount++;
				}

				DebugPrintf("ftl: findDeviceInfoBBT - skipping bank %d, page %d\r\n", (block * Geometry->pagesPerBlock) + page);
				continue;
			}

			if(memcmp(buffer, "DEVICEINFOBBT\0\0\0", 16) == 0) {
				if(deviceInfoBBT) {
					memcpy(deviceInfoBBT, buffer + 0x38, *((uint32_t*)(buffer + 0x34)));
				}

				free(buffer);
				return TRUE;
			} else {
				DebugPrintf("ftl: did not find signature on bank %d, page %d\r\n", (block * Geometry->pagesPerBlock) + page);
			}
		}
	}

	free(buffer);
	return FALSE;
}

static int hasDeviceInfoBBT() {
	int bank;
	int good = TRUE;
	for(bank = 0; bank < Geometry->banksTotal; bank++) {
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
static int curVFLusnInc = 0;
static uint8_t VFLData5[0xF8];

static int VFL_Init() {
	memset(&VFLData1, 0, sizeof(VFLData1));
	if(pstVFLCxt == NULL) {
		pstVFLCxt = malloc(Geometry->banksTotal * sizeof(VFLCxt));
		if(pstVFLCxt == NULL)
			return -1;
	}

	if(pstBBTArea == NULL) {
		pstBBTArea = (uint8_t*) malloc((Geometry->blocksPerBank + 7) / 8);
		if(pstBBTArea == NULL)
			return -1;
	}

	if(ScatteredPageNumberBuffer == NULL && ScatteredBankNumberBuffer == NULL) {
		ScatteredPageNumberBuffer = (uint32_t*) malloc(Geometry->pagesPerSuBlk * 4);
		ScatteredBankNumberBuffer = (uint16_t*) malloc(Geometry->pagesPerSuBlk * 4);
		if(ScatteredPageNumberBuffer == NULL || ScatteredBankNumberBuffer == NULL)
			return -1;
	}

	curVFLusnInc = 0;

	return 0;
}

static FTLCountsTableType FTLCountsTable;
static FTLCxt* pstFTLCxt;
static FTLCxt* FTLCxtBuffer;
static SpareData* FTLSpareBuffer;
static uint32_t* ScatteredVirtualPageNumberBuffer;
static uint8_t* StoreCxt;
static int NumPagesToWriteInStoreCxt;

static int FTL_Init() {
	NumPagesToWriteInStoreCxt = 0;

	int pagesPerCounterTable = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if((((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0) {
		pagesPerCounterTable++;
	}

	NumPagesToWriteInStoreCxt = pagesPerCounterTable * 2;

	int pagesForMapTable = (Geometry->userSuBlksTotal * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if(((Geometry->userSuBlksTotal * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0) {
		pagesForMapTable++;
	}

	NumPagesToWriteInStoreCxt += pagesForMapTable;

	int pagesForLogPageOffsets = (Geometry->pagesPerSuBlk * 34) / Geometry->bytesPerPage;
	if(((Geometry->pagesPerSuBlk * 34) % Geometry->bytesPerPage) != 0) {
		pagesForLogPageOffsets++;
	}

	NumPagesToWriteInStoreCxt += pagesForLogPageOffsets + 2;

	if(NumPagesToWriteInStoreCxt >= Geometry->pagesPerSuBlk) {
		bufferPrintf("nand: error - FTL_NUM_PAGES_TO_WRITE_IN_STORECXT >= PAGES_PER_SUBLK\r\n");
		return -1;
	}

	int pagesPerSimpleMergeBuffer = Geometry->pagesPerSuBlk / 8;
	if((pagesPerSimpleMergeBuffer * 2) >= Geometry->pagesPerSuBlk) {
		bufferPrintf("nand: error - (PAGES_PER_SIMPLE_MERGE_BUFFER * 2) >=  PAGES_PER_SUBLK\r\n");
		return -1;
	}

	memset(&FTLCountsTable, 0, 0x58);

	if(pstFTLCxt == NULL) {
		pstFTLCxt = FTLCxtBuffer = (FTLCxt*) malloc(sizeof(FTLCxt));
		if(pstFTLCxt == NULL)
			return -1;
		memset(pstFTLCxt->field_3D8, 0, sizeof(pstFTLCxt->field_3D8)); 
	}

	pstFTLCxt->eraseCounterPagesDirty = 0;

	pstFTLCxt->pawMapTable = (uint16_t*) malloc(Geometry->userSuBlksTotal * sizeof(uint16_t));
	pstFTLCxt->wPageOffsets = (uint16_t*) malloc((Geometry->pagesPerSuBlk * 18) * sizeof(uint16_t));
	pstFTLCxt->pawEraseCounterTable = (uint16_t*) malloc((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t));
	pstFTLCxt->pawReadCounterTable = (uint16_t*) malloc((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t));

	FTLSpareBuffer = (SpareData*) malloc(Geometry->pagesPerSuBlk * sizeof(SpareData));

	if((Geometry->pagesPerSuBlk / 8) >= NumPagesToWriteInStoreCxt) {
		NumPagesToWriteInStoreCxt = Geometry->pagesPerSuBlk / 8;
	}

	StoreCxt = malloc(Geometry->bytesPerPage * NumPagesToWriteInStoreCxt);
	ScatteredVirtualPageNumberBuffer = (uint32_t*) malloc(Geometry->pagesPerSuBlk * sizeof(uint32_t*));

	if(!pstFTLCxt->pawMapTable || !pstFTLCxt->wPageOffsets || !pstFTLCxt->pawEraseCounterTable || !FTLCxtBuffer->pawReadCounterTable || ! FTLSpareBuffer || !StoreCxt || !ScatteredVirtualPageNumberBuffer)
		return -1;

	int i;
	for(i = 0; i < 18; i++) {
		pstFTLCxt->pLog[i].wPageOffsets = pstFTLCxt->wPageOffsets + (i * Geometry->pagesPerSuBlk);
		memset(pstFTLCxt->pLog[i].wPageOffsets, 0xFF, Geometry->pagesPerSuBlk * 2);
		pstFTLCxt->pLog[i].isSequential = 1;
		pstFTLCxt->pLog[i].pagesUsed = 0;
		pstFTLCxt->pLog[i].pagesCurrent = 0;
	}

	return 0;
}

// pageBuffer and spareBuffer are represented by single BUF struct within Whimory
static int nand_read_vfl_cxt_page(int bank, int block, int page, uint8_t* pageBuffer, uint8_t* spareBuffer) {
	int i;
	for(i = 0; i < 8; i++) {
		if(nand_read(bank, (block * Geometry->pagesPerBlock) + page + i, pageBuffer, spareBuffer, TRUE, TRUE) == 0) {
			SpareData* spareData = (SpareData*) spareBuffer;
			if(spareData->type2 == 0 && spareData->type1 == 0x80)
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


static int vfl_store_cxt(int bank)
{
	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);

	--pstVFLCxt[bank].usnDec;
	pstVFLCxt[bank].usnInc = ++curVFLusnInc;
	pstVFLCxt[bank].nextcxtpage += 8;
	vfl_gen_checksum(bank);

	memset(spareData, 0xFF, Geometry->bytesPerSpare);
	spareData->meta.usnDec = pstVFLCxt[bank].usnDec;
	spareData->type2 = 0;
	spareData->type1 = 0x80;

	int i;
	for(i = 0; i < 8; ++i)
	{
		uint32_t index = pstVFLCxt[bank].activecxtblock;
		uint32_t block = pstVFLCxt[bank].VFLCxtBlock[index];
		uint32_t page = block * Geometry->pagesPerBlock;
		page += pstVFLCxt[bank].nextcxtpage - 8 + i;
		nand_write(bank, page, (uint8_t*) &pstVFLCxt[bank], (uint8_t*) spareData, TRUE);
	}

	int good = 0;
	for(i = 0; i < 8; ++i)
	{
		uint32_t index = pstVFLCxt[bank].activecxtblock;
		uint32_t block = pstVFLCxt[bank].VFLCxtBlock[index];
		uint32_t page = block * Geometry->pagesPerBlock;
		page += pstVFLCxt[bank].nextcxtpage - 8 + i;
		if(nand_read(bank, page, pageBuffer, (uint8_t*) spareData, TRUE, TRUE) != 0)
			continue;

		if(memcmp(pageBuffer, &pstVFLCxt[bank], sizeof(VFLCxt)) != 0)
			continue;

		if(spareData->type2 == 0 && spareData->type1 == 0x80)
			++good;

	}

	free(pageBuffer);
	free(spareData);

	return (good > 3) ? 0 : -1;
}

static int vfl_commit_cxt(int bank)
{
	if((pstVFLCxt[bank].nextcxtpage + 8) <= Geometry->pagesPerBlock)
		if(vfl_store_cxt(bank) == 0)
			return 0;

	uint32_t current = pstVFLCxt[bank].activecxtblock;
	uint32_t block = current;

	while(TRUE)
	{
		block = (block + 1) % 4;
		if(block == current)
			break;

		// try to erase 4 times
		int i;
		for(i = 0; i < 4; ++i)
		{
			if(nand_erase(bank, pstVFLCxt[bank].VFLCxtBlock[block]) == 0)
				break;
		}

		if(i == 4)
			continue;

		pstVFLCxt[bank].activecxtblock = block;
		pstVFLCxt[bank].nextcxtpage = 0;
		if(vfl_store_cxt(bank) == 0)
			return 0;
	}

	bufferPrintf("ftl: failed to commit VFL context!\r\n");
	return -1;
}

static int vfl_store_FTLCtrlBlock()
{
	int bank;
	for(bank = 0; bank < Geometry->banksTotal; bank++)
		memcpy(pstVFLCxt[bank].FTLCtrlBlock, pstFTLCxt->FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));

	// pick a semi-random bank to commit
	return vfl_commit_cxt(curVFLusnInc % Geometry->banksTotal);
}

static void virtual_page_number_to_virtual_address(uint32_t dwVpn, uint16_t* virtualBank, uint16_t* virtualBlock, uint16_t* virtualPage) {
	*virtualBank = dwVpn % Geometry->banksTotal;
	*virtualBlock = dwVpn / Geometry->pagesPerSuBlk;
	*virtualPage = (dwVpn / Geometry->banksTotal) % Geometry->pagesPerBlock;
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
			if(pwDesPbn >= Geometry->blocksPerBank) {
				bufferPrintf("ftl: Destination physical block for remapping is greater than number of blocks per bank!");
			}
			return pstVFLCxt[virtualBank].reservedBlockPoolStart + pwDesPbn;
		}
	}

	return virtualBlock;
}

static int vfl_check_remap_scheduled(int bank, uint16_t block)
{
	int i;
	for(i = 0x333; i > 0 && i > pstVFLCxt[bank].remappingScheduledStart; --i)
	{
		if(pstVFLCxt[bank].reservedBlockPoolMap[i] == block)
			return TRUE;
	}

	return FALSE;
}

static int vfl_schedule_block_for_remap(int bank, uint16_t block)
{
	if(vfl_check_remap_scheduled(bank, block))
		return TRUE;

	bufferPrintf("ftl: attempting to schedule bank %d, block %d for remap!\r\n", bank, block);

	// don't do anything for right now to avoid consequences for false positives
	return FALSE;

	if(pstVFLCxt[bank].remappingScheduledStart == (pstVFLCxt[bank].numReservedBlocks + 10))
	{
		// oh crap, we only have 10 free spares left. back out now.
		return FALSE;
	}

	// stick this into the list
	--pstVFLCxt[bank].remappingScheduledStart;
	pstVFLCxt[bank].reservedBlockPoolMap[pstVFLCxt[bank].remappingScheduledStart] = block;
	vfl_gen_checksum(bank);

	return vfl_commit_cxt(bank);
}

void vfl_set_good_block(int bank, uint16_t block, int isGood)
{
	int index = block / 8;
	uint8_t bit = 1 << (7 - (index % 8));
	if(isGood)
		pstVFLCxt[bank].badBlockTable[index / 8] |= bit;
	else
		pstVFLCxt[bank].badBlockTable[index / 8] &= ~bit;
}

uint16_t vfl_remap_block(int bank, uint16_t block)
{
	if(bank >= Geometry->banksTotal || block >= Geometry->blocksPerBank)
		return 0;

	bufferPrintf("ftl: attempting to remap bank %d, block %d\r\n", bank, block);
	return 0;

	uint16_t newBlock = 0;
	int newBlockIdx;

	// find a reserved block that is not being used
	int i;
	for(i = 0; i < pstVFLCxt[bank].totalReservedBlocks; ++i)
	{
		if(pstVFLCxt[bank].reservedBlockPoolMap[i] == 0)
		{
			newBlock = pstVFLCxt[bank].reservedBlockPoolStart + i;
			newBlockIdx = i;
			break;
		}
	}

	// none found
	if(newBlock == 0)
		return 0;
	
	// try to erase newly allocated reserved block nine times
	for(i = 0; i < 9; ++i)
	{
		if(nand_erase(bank, newBlock) == 0)
			break;
	}

	for(i = 0; i < newBlockIdx; ++i)
	{
		// mark any reserved block previously remapped for this block as bad
		if(pstVFLCxt[bank].reservedBlockPoolMap[i] == block)
			pstVFLCxt[bank].reservedBlockPoolMap[i] = 0xFFFF;
	}

	pstVFLCxt[bank].reservedBlockPoolMap[newBlockIdx] = block;
	++pstVFLCxt[bank].numReservedBlocks;
	vfl_set_good_block(bank, block, FALSE);

	return newBlock;
}

static void vfl_mark_remap_done(int bank, uint16_t block)
{
	bufferPrintf("ftl: attempt to mark remap as done for bank %d, block %d\r\n");
	return;

	uint16_t start = pstVFLCxt[bank].remappingScheduledStart;
	uint16_t lastscheduled = pstVFLCxt[bank].reservedBlockPoolMap[start];
	int i;
	for (i = 0x333; i > 0 && i > start; i--)
	{
		if (pstVFLCxt[bank].reservedBlockPoolMap[i] == block)
		{
			// replace the done entry with the last one
			if(i != start && i != 0x333)
				pstVFLCxt[bank].reservedBlockPoolMap[i] = lastscheduled;

			++pstVFLCxt[bank].remappingScheduledStart;
			return;
		}
	}
}

int VFL_Erase(uint16_t block) {
	uint16_t physicalBlock;
	int ret;
	int bank;
	int i;

	block = block + FTLData->field_4;

	for(bank = 0; bank < Geometry->banksTotal; ++bank) {
		if(vfl_check_remap_scheduled(bank, block))
		{
			vfl_remap_block(bank, block);
			vfl_mark_remap_done(bank, block);
			vfl_commit_cxt(bank);
		}

		physicalBlock = virtual_block_to_physical_block(bank, block);

		for(i = 0; i < 3; ++i)
		{
			ret = nand_erase(bank, physicalBlock);
			if(ret == 0)
				break;
		}

		if(ret) {
			bufferPrintf("ftl: block erase failed for bank %d, block %d\r\n", bank, block);
			// FIXME: properly handle this
			return ret;
		}
	}

	return 0;
}

int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* refresh_page) {
	if(refresh_page) {
		*refresh_page = FALSE;
	}

	VFLData1.field_8++;
	VFLData1.field_20++;

	uint32_t dwVpn = virtualPageNumber + (Geometry->pagesPerSuBlk * FTLData->field_4);
	if(dwVpn >= Geometry->pagesTotal) {
		bufferPrintf("ftl: dwVpn overflow: %d\r\n", dwVpn);
		return ERROR_ARG;
	}

	if(dwVpn < Geometry->pagesPerSuBlk) {
		bufferPrintf("ftl: dwVpn underflow: %d\r\n", dwVpn);
	}

	uint16_t virtualBank;
	uint16_t virtualBlock;
	uint16_t virtualPage;
	uint16_t physicalBlock;

	virtual_page_number_to_virtual_address(dwVpn, &virtualBank, &virtualBlock, &virtualPage);
	physicalBlock = virtual_block_to_physical_block(virtualBank, virtualBlock);

	int page = physicalBlock * Geometry->pagesPerBlock + virtualPage;

	int ret = nand_read(virtualBank, page, buffer, spare, TRUE, TRUE);

	if(!empty_ok && ret == ERROR_EMPTYBLOCK) {
		ret = ERROR_NAND;
	}

	if(refresh_page) {
		if((Geometry->field_2F <= 0 && ret == 0) || ret == ERROR_NAND) {
			bufferPrintf("ftl: setting refresh_page to TRUE due to the following factors: Geometry->field_2F = %x, ret = %d\r\n", Geometry->field_2F, ret);
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

int VFL_Write(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare)
{
	uint32_t dwVpn = virtualPageNumber + (Geometry->pagesPerSuBlk * FTLData->field_4);
	if(dwVpn >= Geometry->pagesTotal) {
		bufferPrintf("ftl: dwVpn overflow: %d\r\n", dwVpn);
		return ERROR_ARG;
	}

	if(dwVpn < Geometry->pagesPerSuBlk) {
		bufferPrintf("ftl: dwVpn underflow: %d\r\n", dwVpn);
	}

	uint16_t virtualBank;
	uint16_t virtualBlock;
	uint16_t virtualPage;
	uint16_t physicalBlock;

	virtual_page_number_to_virtual_address(dwVpn, &virtualBank, &virtualBlock, &virtualPage);
	physicalBlock = virtual_block_to_physical_block(virtualBank, virtualBlock);

	int page = physicalBlock * Geometry->pagesPerBlock + virtualPage;

	int ret = nand_write(virtualBank, page, buffer, spare, TRUE);
	if(ret == 0)
		return 0;

	++pstVFLCxt[virtualBank].field_16;
	vfl_gen_checksum(virtualBank);
	vfl_schedule_block_for_remap(virtualBank, virtualBlock);

	return -1;
}

static int VFL_ReadMultiplePagesInVb(int logicalBlock, int logicalPage, int count, uint8_t* main, SpareData* spare, int* refresh_page) {
	int i;
	int currentPage = logicalPage; 
	for(i = 0; i < count; i++) {
		int ret = VFL_Read((logicalBlock * Geometry->pagesPerSuBlk) + currentPage, main + (Geometry->bytesPerPage * i), (uint8_t*) &spare[i], TRUE, refresh_page);
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
		uint32_t dwVpn = virtualPageNumber[i] + (Geometry->pagesPerSuBlk * FTLData->field_4);

		uint16_t virtualBlock;
		uint16_t virtualPage;
		uint16_t physicalBlock;

		virtual_page_number_to_virtual_address(dwVpn, &ScatteredBankNumberBuffer[i], &virtualBlock, &virtualPage);
		physicalBlock = virtual_block_to_physical_block(ScatteredBankNumberBuffer[i], virtualBlock);
		ScatteredPageNumberBuffer[i] = physicalBlock * Geometry->pagesPerBlock + virtualPage;
	}

	int ret = nand_read_multiple(ScatteredBankNumberBuffer, ScatteredPageNumberBuffer, main, spare, count);
	if(Geometry->field_2F <= 0 && refresh_page != NULL) {
		bufferPrintf("ftl: VFL_ReadScatteredPagesInVb mark page for refresh\r\n");
		*refresh_page = TRUE;
	}

	if(ret != 0)
		return FALSE;
	else
		return TRUE;
}

// sub_18015A9C
static uint16_t* VFL_get_FTLCtrlBlock() {
	int bank = 0;
	int max = 0;
	uint16_t* FTLCtrlBlock = NULL;
	for(bank = 0; bank < Geometry->banksTotal; bank++) {
		int cur = pstVFLCxt[bank].usnInc;
		if(max <= cur) {
			max = cur;
			FTLCtrlBlock = pstVFLCxt[bank].FTLCtrlBlock;
		}
	}

	return FTLCtrlBlock;
}

static int VFL_Open() {
	int bank = 0;
	for(bank = 0; bank < Geometry->banksTotal; bank++) {
		if(!findDeviceInfoBBT(bank, pstBBTArea)) {
			bufferPrintf("ftl: findDeviceInfoBBT failed\r\n");
			return -1;
		}

		if(bank >= Geometry->banksTotal) {
			return -1;
		}


		VFLCxt* curVFLCxt = &pstVFLCxt[bank];
		uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
		uint8_t* spareBuffer = malloc(Geometry->bytesPerSpare);
		if(pageBuffer == NULL || spareBuffer == NULL) {
			bufferPrintf("ftl: cannot allocate page and spare buffer\r\n");
			return -1;
		}

		// Any VFLCxt page will contain an up-to-date list of all blocks used to store VFLCxt pages. Find any such
		// page in the system area.

		int i = 1;
		for(i = 1; i < FTLData->sysSuBlks; i++) {
			// so pstBBTArea is a bit array of some sort
			if(!(pstBBTArea[i / 8] & (1 << (i  & 0x7))))
				continue;

			if(nand_read_vfl_cxt_page(bank, i, 0, pageBuffer, spareBuffer) == TRUE) {
				memcpy(curVFLCxt->VFLCxtBlock, ((VFLCxt*)pageBuffer)->VFLCxtBlock, sizeof(curVFLCxt->VFLCxtBlock));
				break;
			}
		}

		if(i == FTLData->sysSuBlks) {
			bufferPrintf("ftl: cannot find readable VFLCxtBlock\r\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		// Since VFLCxtBlock is a ringbuffer, if blockA.page0.spare.usnDec < blockB.page0.usnDec, then for any page a
	        // in blockA and any page b in blockB, a.spare.usNDec < b.spare.usnDec. Therefore, to begin finding the
		// page/VFLCxt with the lowest usnDec, we should just look at the first page of each block in the ring.
		int minUsn = 0xFFFFFFFF;
		int VFLCxtIdx = 4;
		for(i = 0; i < 4; i++) {
			uint16_t block = curVFLCxt->VFLCxtBlock[i];
			if(block == 0xFFFF)
				continue;

			if(nand_read_vfl_cxt_page(bank, block, 0, pageBuffer, spareBuffer) != TRUE)
				continue;

			SpareData* spareData = (SpareData*) spareBuffer;
			if(spareData->meta.usnDec > 0 && spareData->meta.usnDec <= minUsn) {
				minUsn = spareData->meta.usnDec;
				VFLCxtIdx = i;
			}
		}

		if(VFLCxtIdx == 4) {
			bufferPrintf("ftl: cannot find readable VFLCxtBlock index in spares\r\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		// VFLCxts are stored in the block such that they are duplicated 8 times. Therefore, we only need to
		// read every 8th page, and nand_read_vfl_cxt_page will try the 7 subsequent pages if the first was
		// no good. The last non-blank page will have the lowest spare.usnDec and highest usnInc for VFLCxt
		// in all the land (and is the newest).
		int page = 8;
		int last = 0;
		for(page = 8; page < Geometry->pagesPerBlock; page += 8) {
			if(nand_read_vfl_cxt_page(bank, curVFLCxt->VFLCxtBlock[VFLCxtIdx], page, pageBuffer, spareBuffer) == FALSE) {
				break;
			}
			
			last = page;
		}

		if(nand_read_vfl_cxt_page(bank, curVFLCxt->VFLCxtBlock[VFLCxtIdx], last, pageBuffer, spareBuffer) == FALSE) {
			bufferPrintf("ftl: cannot find readable VFLCxt\n");
			free(pageBuffer);
			free(spareBuffer);
			return -1;
		}

		// Aha, so the upshot is that this finds the VFLCxt and copies it into pstVFLCxt
		memcpy(&pstVFLCxt[bank], pageBuffer, sizeof(VFLCxt));

		// This is the newest VFLCxt across all banks
		if(curVFLCxt->usnInc >= curVFLusnInc) {
			curVFLusnInc = curVFLCxt->usnInc;
		}

		free(pageBuffer);
		free(spareBuffer);

		// Verify the checksum
		if(vfl_check_checksum(bank) == FALSE) {
			bufferPrintf("ftl: VFLCxt has bad checksum\n");
			return -1;
		}
	} 

	// retrieve the FTL control blocks from the latest VFL across all banks.
	void* FTLCtrlBlock = VFL_get_FTLCtrlBlock();
	uint16_t buffer[3];

	// Need a buffer because eventually we'll copy over the source
	memcpy(buffer, FTLCtrlBlock, sizeof(buffer));

	// Then we update the VFLCxts on every bank with that information.
	for(bank = 0; bank < Geometry->banksTotal; bank++) {
		memcpy(pstVFLCxt[bank].FTLCtrlBlock, buffer, sizeof(buffer));
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

static int ftl_set_free_vb(uint16_t block)
{
	// get to the end of the ring buffer
	int nextFreeVb = (pstFTLCxt->nextFreeIdx + pstFTLCxt->wNumOfFreeVb) % 20;
	++pstFTLCxt->wNumOfFreeVb;

	++pstFTLCxt->pawEraseCounterTable[block];
	pstFTLCxt->pawReadCounterTable[block] = 0;

	if(VFL_Erase(block) != 0)
	{
		bufferPrintf("ftl: failed to release a virtual block from the pool\r\n");
		return FALSE;
	}

	pstFTLCxt->awFreeVb[nextFreeVb] = block;

	return TRUE;
}

static int ftl_get_free_vb(uint16_t* block)
{
	int i;

	int chosenVbIdx = 20;
	int curFreeIdx = pstFTLCxt->nextFreeIdx;
	uint16_t smallestEC = 0xFFFF;
	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; ++i)
	{
		if(pstFTLCxt->awFreeVb[curFreeIdx] != 0xFFFF)
		{
			if(pstFTLCxt->pawEraseCounterTable[pstFTLCxt->awFreeVb[curFreeIdx]] < smallestEC)
			{
				smallestEC = pstFTLCxt->pawEraseCounterTable[pstFTLCxt->awFreeVb[curFreeIdx]];
				chosenVbIdx = curFreeIdx;
			}
		}
		curFreeIdx = (curFreeIdx + 1) % 20;
	}

	if(chosenVbIdx > 19)
	{
		bufferPrintf("ftl: could not find a free vb!\r\n");
		return FALSE;
	}

	uint16_t chosenVb = pstFTLCxt->awFreeVb[chosenVbIdx];

	if(chosenVbIdx != pstFTLCxt->nextFreeIdx)
	{
		// swap
		pstFTLCxt->awFreeVb[chosenVbIdx] = pstFTLCxt->awFreeVb[pstFTLCxt->nextFreeIdx];
		pstFTLCxt->awFreeVb[pstFTLCxt->nextFreeIdx] = chosenVb;
	}

	if(chosenVb > (Geometry->userSuBlksTotal + 23))
	{
		bufferPrintf("ftl: invalid free vb\r\n");
		return FALSE;
	}
	
	--pstFTLCxt->wNumOfFreeVb;
	if(pstFTLCxt->wNumOfFreeVb > 19)
	{
		bufferPrintf("ftl: invalid freeVbn\r\n");
		return FALSE;
	}

	if(pstFTLCxt->nextFreeIdx > 19)
	{
		bufferPrintf("ftl: invalid vbListTail\r\n");
		return FALSE;
	}

	// increment cursor
	pstFTLCxt->nextFreeIdx = (pstFTLCxt->nextFreeIdx + 1) % 20;

	*block = chosenVb;
	return TRUE;
}

static int ftl_next_ctrl_page()
{
	++pstFTLCxt->FTLCtrlPage;
	if((pstFTLCxt->FTLCtrlPage % Geometry->pagesPerSuBlk) != 0)
	{
		--pstFTLCxt->usnDec;
		return TRUE;
	}

	// find old block to swap out

	int i;
	for(i = 0; i < 3; ++i)
	{
		if(((pstFTLCxt->FTLCtrlBlock[i] + 1) * Geometry->pagesPerSuBlk) == pstFTLCxt->FTLCtrlPage)
			break;
	}

	int blockIdx = (i + 1) % 3;

	if((pstFTLCxt->eraseCounterPagesDirty % 30) > 2)
	{
		bufferPrintf("ftl: reusing ctrl block at: %d\r\n", pstFTLCxt->FTLCtrlBlock[blockIdx]);

		++pstFTLCxt->eraseCounterPagesDirty;
		++pstFTLCxt->pawEraseCounterTable[pstFTLCxt->FTLCtrlBlock[blockIdx]];
		++pstFTLCxt->pawReadCounterTable[pstFTLCxt->FTLCtrlBlock[blockIdx]];
		if(VFL_Erase(pstFTLCxt->FTLCtrlBlock[blockIdx]) != 0)
		{
			bufferPrintf("ftl: next_ctrl_page failed to erase and markEC(0x%X)\r\n", pstFTLCxt->FTLCtrlBlock[blockIdx]);
			return FALSE;
		}

		pstFTLCxt->FTLCtrlPage = pstFTLCxt->FTLCtrlBlock[blockIdx] * Geometry->pagesPerSuBlk;
		--pstFTLCxt->usnDec;

		return TRUE;
	} else
	{
		++pstFTLCxt->eraseCounterPagesDirty;

		uint16_t newBlock;
		if(!ftl_get_free_vb(&newBlock))
		{
			bufferPrintf("ftl: next_ctrl_page failed to get free VB\r\n");
			return FALSE;
		}

		bufferPrintf("ftl: allocated new ctrl block at: %d\r\n", newBlock);

		uint16_t oldBlock = pstFTLCxt->FTLCtrlBlock[blockIdx];

		pstFTLCxt->FTLCtrlBlock[blockIdx] = newBlock;
		pstFTLCxt->FTLCtrlPage = newBlock * Geometry->pagesPerSuBlk;

		if(!ftl_set_free_vb(oldBlock))
		{
			bufferPrintf("ftl: next_ctrl_page failed to set free VB\r\n");
			return FALSE;
		}


		if(vfl_store_FTLCtrlBlock() == 0)
		{
			--pstFTLCxt->usnDec;
			return TRUE;
		} else
		{
			bufferPrintf("ftl: next_ctrl_page failed to store FTLCtrlBlock info in VFL\r\n");
			return FALSE;
		}
	}
}

// Return whether the block is sequential and also the highest USN of pages
// found in that block. Assumption: for any pages p_i, p_j in a block where
// i < j, usn(p_i) <= usn(p_j)
static int determine_block_type(uint16_t block, uint32_t* highest_usn)
{
	uint8_t* pageBuffer = (uint8_t*) malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);
	int isSequential = TRUE;

	uint32_t max = 0;	
	int page;
	for(page = Geometry->pagesPerSuBlk - 1; page >= 0; --page)
	{
		int ret = VFL_Read(block * Geometry->pagesPerSuBlk + page, pageBuffer, (uint8_t*) spareData, TRUE, NULL);
		if(ret != 0)
			continue;
		
		if(spareData->user.usn > max)
			max = spareData->user.usn;

		if((spareData->user.logicalPageNumber % Geometry->pagesPerSuBlk) != page)
			isSequential = FALSE;
	}

	free(spareData);
	free(pageBuffer);

	*highest_usn = max;

	return isSequential;
}

// Assumptions: same conditions that FTL_Open would have been called in.
static int FTL_Restore() {
	uint16_t* blockMap = (uint16_t*) malloc((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t));
	uint8_t* isEmpty = (uint8_t*) malloc((Geometry->userSuBlksTotal + 23) * sizeof(uint8_t));
	uint8_t* nonSequential = (uint8_t*) malloc((Geometry->userSuBlksTotal + 23) * sizeof(uint8_t));
	uint32_t* usnA = (uint32_t*) malloc(sizeof(uint32_t) * Geometry->pagesPerSuBlk);
	uint32_t* usnB = (uint32_t*) malloc(sizeof(uint32_t) * Geometry->pagesPerSuBlk);
	uint32_t* lpnA = (uint32_t*) malloc(sizeof(uint32_t) * Geometry->pagesPerSuBlk);
	uint32_t* lpnB = (uint32_t*) malloc(sizeof(uint32_t) * Geometry->pagesPerSuBlk);
	uint16_t* awFreeVb = &pstFTLCxt->awFreeVb[0];
	uint16_t* pawMapTable = pstFTLCxt->pawMapTable;
	uint16_t* pawEraseCounterTable = pstFTLCxt->pawEraseCounterTable;
	void* pawReadCounterTable = pstFTLCxt->pawReadCounterTable;
	uint16_t* wPageOffsets = pstFTLCxt->wPageOffsets;
	FTLCxtLog* pLog = &pstFTLCxt->pLog[0];

	uint8_t* pageBuffer = (uint8_t*) malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);

	int i;
	int block;

	bufferPrintf("ftl: restore searching for latest FTL context...\r\n");

	// Step 0, find and load the last readable FTLCxt, so we can have a base erase counter data and
	// goodies. It is not really mandatory though; we could just make a new one.

	void* FTLCtrlBlock;
	if((FTLCtrlBlock = VFL_get_FTLCtrlBlock()) == NULL)
	{
		bufferPrintf("ftl: restore could not get FTL control blocks from VFL!\r\n");
		goto error_release;
	}
	
	memcpy(pstFTLCxt->FTLCtrlBlock, FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));

	uint32_t ftlCtrlBlock = 0xffff;
	uint32_t minUsnDec = 0xffffffff;
	int ftlCxtFound = FALSE;
	for(i = 0; i < sizeof(pstFTLCxt->FTLCtrlBlock)/sizeof(uint16_t); ++i)
	{
		// read the first page of the block
		int ret = VFL_Read(Geometry->pagesPerSuBlk * pstFTLCxt->FTLCtrlBlock[i], pageBuffer, (uint8_t*) spareData, TRUE, NULL);
		if(ret != 0)
			continue;	// this block errored out!

		// 0x43 is the lowest type of FTL control data. Apparently 0x4F would be the highest type.
		if((spareData->type1 - 0x43) > 0xC)
			continue;	// this block doesn't have FTL data in it! Try the next one

		if(ftlCtrlBlock != 0xffff && spareData->meta.usnDec >= minUsnDec)
			continue;	// we've seen a newer FTLCxtBlock before

		// this is the latest so far
		ftlCtrlBlock = pstFTLCxt->FTLCtrlBlock[i];

		uint32_t blockUSNDec = spareData->meta.usnDec;

		int page;
		for(page = Geometry->pagesPerSuBlk - 1; page > 0; page--)
		{
			ret = VFL_Read(Geometry->pagesPerSuBlk * ftlCtrlBlock + page, pageBuffer, (uint8_t*) spareData, TRUE, NULL);
			if(ret == 1) {
				continue;
			} else if(ret == 0 && spareData->type1 == 0x43) { // 43 is FTLCxtBlock
				minUsnDec = blockUSNDec;
				memcpy(pstFTLCxt, pageBuffer, sizeof(FTLCxt));

				// we just overwrote our good FTLCtrlBlock info, fill it in again.
				memcpy(pstFTLCxt->FTLCtrlBlock, FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));
				ftlCxtFound = TRUE;
				break;
			}
		}
	}

	if(!ftlCxtFound)
	{
		bufferPrintf("ftl: restore could not find ANY FTL contexts!\r\n");
		goto error_release;
	}

	bufferPrintf("ftl: restore found useable FTL context with usnDec = 0x%x\r\n", pstFTLCxt->usnDec);

	// Reset the pointers
	pstFTLCxt->pawMapTable = pawMapTable;
	pstFTLCxt->pawEraseCounterTable = pawEraseCounterTable;
	pstFTLCxt->pawReadCounterTable = pawReadCounterTable;
	pstFTLCxt->wPageOffsets = wPageOffsets;

	// Clean out now almost certainly invalid values

	memset(&FTLCountsTable, 0, 0x58);

	pstFTLCxt->clean = 0;

	for(i = 0; i < 18; ++i)
	{
		pLog[i].wPageOffsets = pstFTLCxt->wPageOffsets + (i * Geometry->pagesPerSuBlk);
		memset(pLog[i].wPageOffsets, 0xFF, Geometry->pagesPerSuBlk * sizeof(uint16_t));
		pLog[i].isSequential = 1;
		pLog[i].pagesUsed = 0;
		pLog[i].pagesCurrent = 0;
		pLog[i].wVbn = 0xFFFF;
	}

	for(i = 0; i < 20; ++i)
		awFreeVb[i] = 0xFFFF;

	pstFTLCxt->nextFreeIdx = 0;
	pstFTLCxt->wNumOfFreeVb = 0;

	// Read back the counter tables... those are still useful.
	if(!ftl_open_read_counter_tables())
	{
		bufferPrintf("ftl: restore could not read back the counter tables from the FTL context!\r\n");
		goto error_release;
	}

	++FTLCountsTable.ftlRestoresCount;

	// next time we commit, do it on a fresh block
	block = pstFTLCxt->FTLCtrlPage / Geometry->pagesPerSuBlk;
	pstFTLCxt->FTLCtrlPage = (block * Geometry->pagesPerSuBlk) + Geometry->pagesPerSuBlk - 1;

	int numLogs = 0;

	// Step one, create an overview of which virtual blocks have pages belonging to which logical blocks.
	// Mark any blocks discovered to be empty. Also to save time in the next step, if a block is proven
	// to be non-sequential, mark it as such.
	
	for(block = 0; block < (Geometry->userSuBlksTotal + 23); ++block)
	{
		if((block % 1000) == 0)
		{
			bufferPrintf("ftl: restore scanning virtual blocks %d - %d\r\n", block,
					block + ((((Geometry->userSuBlksTotal + 23) - block) > 1000) ? 999 : ((Geometry->userSuBlksTotal + 23) - block - 1)));
		}

		blockMap[block] = 0xFFFF;
		isEmpty[block] = 1;
		nonSequential[block] = 0;

		int page;
		for(page = 0; page < Geometry->pagesPerSuBlk; ++page)
		{
			int ret = VFL_Read(block * Geometry->pagesPerSuBlk + page, pageBuffer, (uint8_t*) spareData, TRUE, NULL);

			if(ret == ERROR_EMPTYBLOCK)
				continue;
			
			isEmpty[block] = 0;
			
			if(ret != 0)
				continue;
			
			if(spareData->type1 >= 0x43 && spareData->type1 <= 0x4F)
				break;

			// wtf is this? well, we'll just count it as empty
			if(spareData->type1 != 0x40 && spareData->type1 != 0x41)
				continue;

			if((spareData->user.logicalPageNumber % Geometry->pagesPerSuBlk) != page)
				nonSequential[block] = 1;

			blockMap[block] = spareData->user.logicalPageNumber / Geometry->pagesPerSuBlk;
			break;	
		}
	}

	// Step two, make sure each logical block has a mapping to virtual block. If more than one virtual
	// block contain pages to a logical block, pick which one is a mapping block and which one is a
	// log block based on whether the entries are sequential and which ones have the highest USN. If
	// no virtual block for a logical block is found, then it must have been empty so we select an
	// empty block to use as its virtual block.

	bufferPrintf("ftl: restore creating mapping table...\r\n");

	for(block = 0; block < Geometry->userSuBlksTotal; ++block)
	{
		uint16_t mapCandidate = 0xFFFF;
		uint32_t mapCandidateUSN = 0xFFFFFFFF;
		uint16_t logCandidate = 0xFFFF;
		uint32_t logCandidateUSN = 0xFFFFFFFF;
		uint16_t candidate;

		if((block % 1000) == 0)
		{
			bufferPrintf("ftl: restore scanning logical blocks %d - %d\r\n", block,
					block + (((Geometry->userSuBlksTotal - block) > 1000) ? 999 : (Geometry->userSuBlksTotal - block - 1)));
		}

		for(candidate = 0; candidate < (Geometry->userSuBlksTotal + 23); ++candidate)
		{
			uint32_t candidateUSN;

			if(blockMap[candidate] != block)
				continue;

			if(nonSequential[candidate])
			{
				if(logCandidate == 0xFFFF)
				{
					logCandidate = candidate;
					continue;
				}
				if(logCandidateUSN == 0xFFFFFFFF)
					determine_block_type(logCandidate, &logCandidateUSN);

				determine_block_type(candidate, &candidateUSN);

				if(logCandidateUSN < candidateUSN)
				{
					logCandidate = candidate;
					logCandidateUSN = candidateUSN;
				}
			} else if(mapCandidate == 0xFFFF)
			{
				mapCandidate = candidate;
				continue;
			}

			int origMCSeq = TRUE;
			if(mapCandidateUSN == 0xFFFFFFFF)
				origMCSeq = determine_block_type(mapCandidate, &mapCandidateUSN);

			int newMCSeq = determine_block_type(candidate, &candidateUSN);

			uint16_t newLCandidate;
			uint32_t newLCandidateUSN;

			if(origMCSeq && newMCSeq)
			{
				if(mapCandidateUSN > candidateUSN)
				{
					newLCandidate = candidate;
					newLCandidateUSN = candidateUSN;
				} else
				{
					newLCandidate = mapCandidate;
					newLCandidateUSN = mapCandidateUSN;
					mapCandidate = candidate;
					mapCandidateUSN = candidateUSN;
				}
			} else if(origMCSeq && !newMCSeq)
			{	
				newLCandidate = candidate;
				newLCandidateUSN = candidateUSN;
			} else if(!origMCSeq && newMCSeq)
			{
				newLCandidate = mapCandidate;
				newLCandidateUSN = mapCandidateUSN;
				mapCandidate = candidate;
				mapCandidateUSN = candidateUSN;
			} else if(!origMCSeq && !newMCSeq)
			{	
				if(mapCandidateUSN > candidateUSN)
				{
					newLCandidate = mapCandidate;
					newLCandidateUSN = mapCandidateUSN;
				} else
				{
					newLCandidate = candidate;
					newLCandidateUSN = candidateUSN;
				}
				mapCandidate = 0xFFFF;
				mapCandidateUSN = 0xFFFFFFFF;
			}

			if(logCandidate == 0xFFFF)
			{
				logCandidate = newLCandidate;
				logCandidateUSN = newLCandidateUSN;
				continue;
			}

			if(logCandidateUSN == 0xFFFFFFFF)
				determine_block_type(logCandidate, &logCandidateUSN);

			if(logCandidateUSN < newLCandidateUSN)
			{
				logCandidate = newLCandidate;
				logCandidateUSN = newLCandidateUSN;
			}
		}

		if(mapCandidate == 0xFFFF)
		{
			for(candidate = 0; candidate < (Geometry->userSuBlksTotal + 23); ++candidate)
			{
				if(isEmpty[candidate])
				{
					mapCandidate = candidate;
					isEmpty[candidate] = 0;
					break;
				}
			}

			if(mapCandidate == 0xFFFF)
			{
				bufferPrintf("ftl: restore failed, didn't find enough empty blocks to pair with orphan logical blocks.\r\n");
				goto error_release;
			}
		}

		pawMapTable[block] = mapCandidate;
		blockMap[mapCandidate] = 0;

		if(logCandidate != 0xFFFF)
		{
			pLog[numLogs].wLbn = block;
			pLog[numLogs].wVbn = logCandidate;
			++numLogs;

			blockMap[logCandidate] = 0;
		}
	}	

	// Step three
	// At this point, pawMapTable ought to be correct. We can also assert that all blocks that were originally
	// either in pawMapTable or pLog, we have put in pawMapTable or pLog. The rest are either the three control
	// blocks, or the free vbs. Therefore, we look for blocks that were not marked as pawMapTable or pLog and
	// also not a control block and mark them as free vbs.

	bufferPrintf("ftl: restore determing free vbs...\r\n");

	for(block = 0; block < (Geometry->userSuBlksTotal + 23); ++block)
	{
		if(blockMap[block] == 0)
			continue;

		for(i = 0; i < 3; ++i)
		{
			if(block == pstFTLCxt->FTLCtrlBlock[i])
				break;
		}

		if(i < 3)
			continue;

		awFreeVb[pstFTLCxt->wNumOfFreeVb++] = block;
	}
	
	bufferPrintf("ftl: restore wNumOfFreeVb = %d, number of log vbs = %d\r\n",
			pstFTLCxt->wNumOfFreeVb, numLogs);

	// Now we do a consistency check. The total number of virtual blocks accessible by
	// our FTL is the number of user superblocks + 3 control blocks + 20 blocks divided
	// between free blocks and log blocks. It can get down to 3 free blocks and 17 log
	// blocks (defined by the size of the log block array, and explicitly guarded by
	// ftl_prepare_log), or up to 20 free blocks and 0 log blocks (there is space for
	// 20 free vbs). That counts up to the + 23 number you see everywhere. Therefore,
	// the number of free vbs and the number of log blocks used must equal 20.
	if((pstFTLCxt->wNumOfFreeVb + numLogs) != 20)
	{
		bufferPrintf("ftl: restore failed, we are missing pool blocks!\r\n");
		goto error_release;
	}

	// Step four. Now for those blocks that have log entries, we must decide how to
	// populate the page offsets table, i.e. divide the logical block between these
	// two blocks. Assumption: if block L is the log block for block B, pick any
	// page u from L and v from B -- usn(u) > usn(v). This is because after the
	// map block is written, any further write to it will hit the log block for it
	// and increment the USN.
	
	uint32_t highest_usn = 0;

	for(i = 0; i < numLogs; ++i)
	{
		// Since we always store the highest USN block as the map in step two
		// to ensure we always end up with the two highest USN blocks, we
		// could have the ordering swapped. Figure out the correct one.

		uint16_t blockA = pawMapTable[pLog[i].wLbn];
		uint16_t blockB = pLog[i].wVbn;
		int aSequential = 1;
		int bSequential = 1;
		uint32_t aHighestUSN = 0;
		uint32_t bHighestUSN = 0;
		uint32_t* mapBlockUSN;
		uint32_t* mapBlockLPN;
		uint32_t* logBlockUSN;
		uint32_t* logBlockLPN;
		int page;

		// Populate information about these two blocks

		for(page = Geometry->pagesPerSuBlk - 1; page >= 0; --page)
		{
			int ret = VFL_Read((blockA * Geometry->pagesPerSuBlk) + page, pageBuffer, (uint8_t*) spareData, TRUE, NULL);
			if(ret == ERROR_EMPTYBLOCK)
				usnA[page] = 0;
			else
			{
				usnA[page] = spareData->user.usn;
				lpnA[page] = spareData->user.logicalPageNumber;

				if(usnA[page] > aHighestUSN)
					aHighestUSN = usnA[page];

				if((spareData->user.logicalPageNumber % Geometry->pagesPerSuBlk) != page)
					aSequential = 0;
			}
		}

		for(page = Geometry->pagesPerSuBlk - 1; page >= 0; --page)
		{
			int ret = VFL_Read((blockB * Geometry->pagesPerSuBlk) + page, pageBuffer, (uint8_t*) spareData, TRUE, NULL);
			if(ret == ERROR_EMPTYBLOCK)
				usnB[page] = 0;
			else
			{
				usnB[page] = spareData->user.usn;
				lpnB[page] = spareData->user.logicalPageNumber;

				if(usnB[page] > bHighestUSN)
					bHighestUSN = usnB[page];

				if((spareData->user.logicalPageNumber % Geometry->pagesPerSuBlk) != page)
					bSequential = 0;
			}
		}

		// Determine which is which
		if((aSequential && bSequential && bHighestUSN > aHighestUSN) || aSequential)
		{
			pLog[i].wVbn = blockB;
			pawMapTable[pLog[i].wLbn] = blockA;
			mapBlockUSN = usnA;
			logBlockUSN = usnB;
			mapBlockLPN = lpnA;
			logBlockLPN = lpnB;
			if(bHighestUSN > highest_usn)
				highest_usn = bHighestUSN;
		} else if((aSequential && bSequential && bHighestUSN <= aHighestUSN) || bSequential)
		{
			pLog[i].wVbn = blockA;
			pawMapTable[pLog[i].wLbn] = blockB;
			mapBlockUSN = usnB;
			logBlockUSN = usnA;
			mapBlockLPN = lpnB;
			logBlockLPN = lpnA;
			if(aHighestUSN > highest_usn)
				highest_usn = aHighestUSN;
		} else
		{
			bufferPrintf("ftl: restore failed, we have two non-sequential blocks!\n");
			goto error_release;
		}

		// okay, now we will populate the log block with the correct information.

		for(page = Geometry->pagesPerSuBlk - 1; page >= 0; --page)
		{
			int logOffset;

			// check if empty
			if(logBlockUSN[page] == 0)
				continue;

			// we set it to after the first non-empty page
			if(pLog[i].pagesUsed == 0)
				pLog[i].pagesUsed = page + 1;

			logOffset = logBlockLPN[page] % Geometry->pagesPerSuBlk;

			// is there a newer copy of this page already in this log block?
			if(pLog[i].wPageOffsets[logOffset] != 0xFFFF)
				continue;

			// if not, we'll use it since it's the most recent.
			pLog[i].wPageOffsets[logOffset] = page;
			++pLog[i].pagesCurrent;
			
			if(logOffset != page)
				pLog[i].isSequential = 0;
		}

		if(pLog[i].pagesUsed != pLog[i].pagesCurrent)
			pLog[i].isSequential = 0;

		bufferPrintf("ftl: restore -- log %d, wLbn = %d, wVbn = %d, pagesUsed = %d, pagesCurrent = %d, isSequential = %d\r\n",
				i, pLog[i].wLbn, pLog[i].wVbn, pLog[i].pagesUsed, pLog[i].pagesCurrent, pLog[i].isSequential);
	}

	pstFTLCxt->nextblockusn = highest_usn + 1;

	for(i = 0; i < numLogs; ++i)
	{
		pLog[i].usn = pstFTLCxt->nextblockusn - 1;
	}

	bufferPrintf("ftl: restore successful!\r\n");
	globalFtlHasBeenRestored = 1;
	free(pageBuffer);
	free(spareData);
	free(blockMap);
	free(nonSequential);
	free(isEmpty);

	return TRUE;

error_release:
	free(usnA);
	free(usnB);
	free(lpnA);
	free(lpnB);
	free(pageBuffer);
	free(spareData);
	free(blockMap);
	free(nonSequential);
	free(isEmpty);

	return FALSE;
}

static int FTL_GetStruct(FTLStruct type, void** data, int* size) {
	switch(type) {
		case FTLCountsTableSID:
			*data = &FTLCountsTable;
			*size = sizeof(FTLCountsTable);
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
	FTL_GetStruct(FTLCountsTableSID, &data, &size);
	FTL_64bit_sum((uint64_t*)pageBuffer, (uint64_t*)data, size);
	VFL_GetStruct(VFLData1SID, &data, &size);
	FTL_64bit_sum((uint64_t*)(pageBuffer + 0x200), (uint64_t*)data, size);
	VFL_GetStruct(VFLData5SID, &data, &size);
	FTL_64bit_sum((uint64_t*)(pageBuffer + 0x400), (uint64_t*)data, size);
	return TRUE;
}

static int ftl_open_read_counter_tables()
{
	int i;
	int pagesToRead;

	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	uint8_t* spareBuffer = malloc(Geometry->bytesPerSpare);

	pagesToRead = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if((((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0)
		pagesToRead++;

	int success = FALSE;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_pawEraseCounterTable[i], pageBuffer, spareBuffer, TRUE, NULL) != 0)
		{
			success = FALSE;
			goto release;
		}

		int toRead = Geometry->bytesPerPage;
		if(toRead > (((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
			toRead = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->pawEraseCounterTable) + (i * Geometry->bytesPerPage), pageBuffer, toRead);	
	}

	bufferPrintf("ftl: Detected version %x %x\r\n", FTLCxtBuffer->versionLower, FTLCxtBuffer->versionUpper);
	if(FTLCxtBuffer->versionLower == 0x46560001 && FTLCxtBuffer->versionUpper == 0xB9A9FFFE) {
		pagesToRead = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) / Geometry->bytesPerPage;
		if((((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0)
			pagesToRead++;

		success = TRUE;
		for(i = 0; i < pagesToRead; i++) {
			if(VFL_Read(pstFTLCxt->pages_for_pawReadCounterTable[i], pageBuffer, spareBuffer, TRUE, NULL) != 0) {
				success = FALSE;
				break;
			}

			int toRead = Geometry->bytesPerPage;
			if(toRead > (((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
				toRead = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
			}

			memcpy(((uint8_t*)pstFTLCxt->pawReadCounterTable) + (i * Geometry->bytesPerPage), pageBuffer, toRead);	
		}

		if((pstFTLCxt->hasFTLCountsTable + 1) == 0) {
			int x = pstFTLCxt->page_for_FTLCountsTable / Geometry->pagesPerSuBlk;
			if(x == 0 || x <= Geometry->userSuBlksTotal) {
				if(VFL_Read(pstFTLCxt->page_for_FTLCountsTable, pageBuffer, spareBuffer, TRUE, NULL) != 0)
				{
					success = FALSE;
					goto release;
				}

				sum_data(pageBuffer);
			}
		}
	} else {
		bufferPrintf("ftl: updating the FTL from seemingly compatible version\r\n");
		for(i = 0; i < (Geometry->userSuBlksTotal + 23); i++) {
			pstFTLCxt->pawReadCounterTable[i] = 0x1388;
		}

		for(i = 0; i < 5; i++) {
			pstFTLCxt->elements2[i].field_0 = -1;
			pstFTLCxt->elements2[i].field_2 = -1;
		}

		pstFTLCxt->field_3C8 = 0;
		pstFTLCxt->clean = 0;
		FTLCxtBuffer->versionLower = 0x46560000;
		FTLCxtBuffer->versionUpper = 0xB9A9FFFF;

		success = TRUE;
	}

release:
	free(pageBuffer);
	free(spareBuffer);
	return success;
}

void check_for_dirty_free_vb()
{
	int i;
	int curFreeIdx = pstFTLCxt->nextFreeIdx;

	if(CleanFreeVb)
		return;

	uint8_t* pageBuffer = (uint8_t*) malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);

	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; ++i)
	{
		if(pstFTLCxt->awFreeVb[curFreeIdx] != 0xFFFF)
		{
			int page;
			int block = pstFTLCxt->awFreeVb[curFreeIdx];
			for(page = 0; page < Geometry->pagesPerSuBlk; ++page)
			{
				int ret = VFL_Read(block * Geometry->pagesPerSuBlk + page, pageBuffer, (uint8_t*) spareData, TRUE, NULL);

				if(ret == ERROR_EMPTYBLOCK)
					continue;

				bufferPrintf("ftl: free block %d has non-empty pages.\r\n", block);
				VFL_Erase(block);
				break;
			}
		}
		curFreeIdx = (curFreeIdx + 1) % 20;
	}

	CleanFreeVb = TRUE;
	free(pageBuffer);
	free(spareData);
}

static int FTL_Open(int* pagesAvailable, int* bytesPerPage) {
	int refreshPage;
	int ret;
	int i;

	uint16_t* pawMapTable = pstFTLCxt->pawMapTable;
	uint16_t* pawEraseCounterTable = pstFTLCxt->pawEraseCounterTable;
	void* pawReadCounterTable = pstFTLCxt->pawReadCounterTable;
	uint16_t* wPageOffsets = pstFTLCxt->wPageOffsets;

	void* FTLCtrlBlock;
	if((FTLCtrlBlock = VFL_get_FTLCtrlBlock()) == NULL)
		goto FTL_Open_Error;
	
	memcpy(pstFTLCxt->FTLCtrlBlock, FTLCtrlBlock, sizeof(pstFTLCxt->FTLCtrlBlock));

	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	uint8_t* spareBuffer = malloc(Geometry->bytesPerSpare);
	if(!pageBuffer || !spareBuffer) {
		bufferPrintf("ftl: FTL_Open ran out of memory!\r\n");
		return ERROR_ARG;
	}

	// First thing is to get the latest FTLCtrlBlock from the FTLCtrlBlock. It will have the lowest spare.usnDec
	// Again, since a ring buffer is used, the lowest usnDec FTLCxt will be in the FTLCtrlBlock whose first page
	// has the lowest usnDec
	uint32_t ftlCtrlBlock = 0xffff;
	uint32_t minUsnDec = 0xffffffff;
	for(i = 0; i < sizeof(pstFTLCxt->FTLCtrlBlock)/sizeof(uint16_t); i++) {
		// read the first page of the block
		ret = VFL_Read(Geometry->pagesPerSuBlk * pstFTLCxt->FTLCtrlBlock[i], pageBuffer, spareBuffer, TRUE, &refreshPage);
		if(ret == ERROR_ARG) {
			free(pageBuffer);
			free(spareBuffer);
			goto FTL_Open_Error;
		}

		// 0x43 is the lowest type of FTL control data. Apparently 0x4F would be the highest type.
		SpareData* spareData = (SpareData*) spareBuffer;
		if((spareData->type1 - 0x43) > 0xC)
			continue;	// this block doesn't have FTL data in it! Try the next one

		if(ret != 0)
			continue;	// this block errored out!

		if(ftlCtrlBlock != 0xffff && spareData->meta.usnDec >= minUsnDec)
			continue;	// we've seen a newer FTLCxtBlock before

		// this is the latest so far
		minUsnDec = spareData->meta.usnDec;
		ftlCtrlBlock = pstFTLCxt->FTLCtrlBlock[i];
	}

	if(ftlCtrlBlock == 0xffff) {
		bufferPrintf("ftl: Cannot find context!\r\n");
		goto FTL_Open_Error_Release;
	}

	bufferPrintf("ftl: Successfully found FTL context block: %d\r\n", ftlCtrlBlock);

	// The last readable page in this block ought to be a FTLCxt block! If it's any other ftl control page
	// then the shut down was unclean. FTLCxt ought never be the very first page.
	int ftlCxtFound = FALSE;
	for(i = Geometry->pagesPerSuBlk - 1; i > 0; i--) {
		ret = VFL_Read(Geometry->pagesPerSuBlk * ftlCtrlBlock + i, pageBuffer, spareBuffer, TRUE, &refreshPage);
		if(ret == 1) {
			continue;
		} else if(ret == 0 && ((SpareData*)spareBuffer)->type1 == 0x43) { // 43 is FTLCxtBlock
			memcpy(FTLCxtBuffer, pageBuffer, sizeof(FTLCxt));
			ftlCxtFound = TRUE;
			break;
		} else {
			if(ret == 0)
				bufferPrintf("ftl: Possible unclean shutdown, last FTL metadata type written was 0x%x\r\n", ((SpareData*)spareBuffer)->type1);
			else
				bufferPrintf("ftl: Error reading FTL context block.\r\n");

			ftlCxtFound = FALSE;
			break;
		}
	}

	if(!ftlCxtFound)
		goto FTL_Open_Error_Release;

	bufferPrintf("ftl: Successfully read FTL context block. usnDec = 0x%x\r\n", pstFTLCxt->usnDec);

	// Restore now possibly overwritten (by data from NAND) pointers from backed up copies
	pstFTLCxt->pawMapTable = pawMapTable;
	pstFTLCxt->pawEraseCounterTable = pawEraseCounterTable;
	pstFTLCxt->pawReadCounterTable = pawReadCounterTable;
	pstFTLCxt->wPageOffsets = wPageOffsets;

	for(i = 0; i < 18; i++) {
		pstFTLCxt->pLog[i].wPageOffsets = pstFTLCxt->wPageOffsets + (i * Geometry->pagesPerSuBlk);
	}

	int pagesToRead;

	pagesToRead = (Geometry->userSuBlksTotal * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if(((Geometry->userSuBlksTotal * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_pawMapTable[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
			goto FTL_Open_Error_Release;

		int toRead = Geometry->bytesPerPage;
		if(toRead > ((Geometry->userSuBlksTotal * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
			toRead = (Geometry->userSuBlksTotal * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->pawMapTable) + (i * Geometry->bytesPerPage), pageBuffer, toRead);	
	}

	pagesToRead = (Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) / Geometry->bytesPerPage;
	if(((Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) % Geometry->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_wPageOffsets[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
			goto FTL_Open_Error_Release;

		int toRead = Geometry->bytesPerPage;
		if(toRead > ((Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) - (i * Geometry->bytesPerPage))) {
			toRead = (Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) - (i * Geometry->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->wPageOffsets) + (i * Geometry->bytesPerPage), pageBuffer, toRead);	
	}

	pagesToRead = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if((((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0)
		pagesToRead++;

	for(i = 0; i < pagesToRead; i++) {
		if(VFL_Read(pstFTLCxt->pages_for_pawEraseCounterTable[i], pageBuffer, spareBuffer, TRUE, &refreshPage) != 0)
			goto FTL_Open_Error_Release;

		int toRead = Geometry->bytesPerPage;
		if(toRead > (((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
			toRead = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
		}

		memcpy(((uint8_t*)pstFTLCxt->pawEraseCounterTable) + (i * Geometry->bytesPerPage), pageBuffer, toRead);	
	}

	int success = ftl_open_read_counter_tables();

	if(success) {
		CleanFreeVb = TRUE;
		bufferPrintf("ftl: FTL successfully opened!\r\n");
		free(pageBuffer);
		free(spareBuffer);
		*pagesAvailable = Geometry->userPagesTotal;
		*bytesPerPage = Geometry->bytesPerPage;
		return 0;
	}

FTL_Open_Error_Release:
	free(pageBuffer);
	free(spareBuffer);

FTL_Open_Error:
	bufferPrintf("ftl: FTL_Open cannot load FTLCxt!\r\n");
	CleanFreeVb = FALSE;
	if(FTL_Restore() != FALSE) {
		*pagesAvailable = Geometry->userPagesTotal;
		*bytesPerPage = Geometry->bytesPerPage;
		return 0;
	} else {
		return ERROR_ARG;
	}
}

uint32_t FTL_map_page(FTLCxtLog* pLog, int lbn, int offset) {
	if(pLog && pLog->wPageOffsets[offset] != 0xFFFF) {
		if(((pLog->wVbn * Geometry->pagesPerSuBlk) + pLog->wPageOffsets[offset] + 1) != 0)
			return (pLog->wVbn * Geometry->pagesPerSuBlk) + pLog->wPageOffsets[offset];
	}

	return (pstFTLCxt->pawMapTable[lbn] * Geometry->pagesPerSuBlk) + offset;
}

static inline FTLCxtLog* ftl_get_log(uint16_t lbn)
{
	int i;
	for(i = 0; i < 17; i++) {
		if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
			continue;

		if(pstFTLCxt->pLog[i].wLbn == lbn) {
			return &pstFTLCxt->pLog[i];
		}
	}
	return NULL;
}

int FTL_Read(int logicalPageNumber, int totalPagesToRead, uint8_t* pBuf) {
	int i;
	int hasError = FALSE;

	FTLCountsTable.totalPagesRead += totalPagesToRead;
	++FTLCountsTable.totalReads;
	pstFTLCxt->totalReadCount++;

	if(!pBuf) {
		return ERROR_ARG;
	}

	if(totalPagesToRead == 0 || (logicalPageNumber + totalPagesToRead) >= Geometry->userPagesTotal) {
		bufferPrintf("ftl: invalid input parameters\r\n");
		return ERROR_INPUT;
	}

	int lbn = logicalPageNumber / Geometry->pagesPerSuBlk;
	int offset = logicalPageNumber - (lbn * Geometry->pagesPerSuBlk);

	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	uint8_t* spareBuffer = malloc(Geometry->bytesPerSpare);
	if(!pageBuffer || !spareBuffer) {
		bufferPrintf("ftl: FTL_Read ran out of memory!\r\n");
		return ERROR_ARG;
	}

	FTLCxtLog* pLog = ftl_get_log(lbn);

	int ret = 0;
	int pagesRead = 0;
	int pagesToRead;
	int refreshPage;
	int currentLogicalPageNumber = logicalPageNumber;

	while(TRUE) {
		// Read as much as we can from the first logical block
		pagesToRead = Geometry->pagesPerSuBlk - offset;
		if(pagesToRead >= (totalPagesToRead - pagesRead))
			pagesToRead = totalPagesToRead - pagesRead;

		int readSuccessful;
		if(pLog != NULL) {
			// we have a scatter entry for this logical block, so we use it
			for(i = 0; i < pagesToRead; i++) {
				ScatteredVirtualPageNumberBuffer[i] = FTL_map_page(pLog, lbn, offset + i);
				if((ScatteredVirtualPageNumberBuffer[i] / Geometry->pagesPerSuBlk) == pLog->wVbn) {
					// This particular page is mapped within one of the log blocks, so we increment for the log block
					pstFTLCxt->pawReadCounterTable[ScatteredVirtualPageNumberBuffer[i] / Geometry->pagesPerSuBlk]++;
				} else {
					// This particular page is mapped to the main block itself, so we increment for that block
					pstFTLCxt->pawReadCounterTable[pstFTLCxt->pawMapTable[lbn]]++;
				}
			}

			readSuccessful = VFL_ReadScatteredPagesInVb(ScatteredVirtualPageNumberBuffer, pagesToRead, pBuf + (pagesRead * Geometry->bytesPerPage), FTLSpareBuffer, &refreshPage);
			if(refreshPage) {
				bufferPrintf("ftl: _AddLbnToRefreshList (0x%x, 0x%x, 0x%x)\r\n", lbn, pstFTLCxt->pawMapTable[lbn], pLog->wVbn);
			}
		} else {
			// VFL_ReadMultiplePagesInVb has a different calling convention and implementation than the equivalent iBoot function.
			// Ours is a bit less optimized, and just calls VFL_Read for each page.
			pstFTLCxt->pawReadCounterTable[pstFTLCxt->pawMapTable[lbn]] += pagesToRead;
			readSuccessful = VFL_ReadMultiplePagesInVb(pstFTLCxt->pawMapTable[lbn], offset, pagesToRead, pBuf + (pagesRead * Geometry->bytesPerPage), FTLSpareBuffer, &refreshPage);
			if(refreshPage) {
				bufferPrintf("ftl: _AddLbnToRefreshList (0x%x, 0x%x)\r\n", lbn, pstFTLCxt->pawMapTable[lbn]);
			}
		}

		int loop = 0;
		if(readSuccessful) {
			// check ECC mark for all pages
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
			if(pagesRead != totalPagesToRead && Geometry->pagesPerSuBlk != offset) {
				// there's some remaining pages we have not read before. handle them individually

				int virtualPage = FTL_map_page(pLog, lbn, offset);
				ret = VFL_Read(virtualPage, pBuf + (Geometry->bytesPerPage * pagesRead), spareBuffer, TRUE, &refreshPage);
				if(refreshPage) {
					bufferPrintf("ftl: _AddLbnToRefreshList (0x%x, 0x%x)\r\n", lbn, virtualPage / Geometry->pagesPerSuBlk);
				}

				if(ret == ERROR_ARG)
					goto FTL_Read_Error_Release;

				if(ret == ERROR_NAND || ((SpareData*) spareBuffer)->eccMark != 0xFF) {
					// ecc error
					bufferPrintf("ftl: ECC error, ECC mark is: %x\r\n", ((SpareData*) spareBuffer)->eccMark);
					hasError = TRUE;
					if(pLog) {
						virtualPage = FTL_map_page(pLog, lbn, offset);
						bufferPrintf("ftl: lbn 0x%x pLog->wVbn 0x%x pawMapTable 0x%x offset 0x%x vpn 0x%x\r\n", lbn, pLog->wVbn, pstFTLCxt->pawMapTable[lbn], offset, virtualPage);
					} else {
						virtualPage = FTL_map_page(NULL, lbn, offset);
						bufferPrintf("ftl: lbn 0x%x pawMapTable 0x%x offset 0x%x vpn 0x%x\r\n", lbn, pstFTLCxt->pawMapTable[lbn], offset, virtualPage);
					}
				}

				if(ret == 0) {
					if(((SpareData*) spareBuffer)->user.logicalPageNumber != offset) {
						// that's not the page we expected there
						bufferPrintf("ftl: error, dwWrittenLpn(0x%x) != dwLpn(0x%x)\r\n", ((SpareData*) spareBuffer)->user.logicalPageNumber, offset);
					}
				}

				pagesRead++;
				currentLogicalPageNumber++;
				offset++;
				if(pagesRead == totalPagesToRead) {
					goto FTL_Read_Done;
				}
			}

			if(offset == Geometry->pagesPerSuBlk) {
				// go to the next block

				lbn++;
				if(lbn >= Geometry->userSuBlksTotal)
					goto FTL_Read_Error_Release;

				pLog = ftl_get_log(lbn);

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

static int ftl_commit_cxt()
{

	// TODO: We should use the StoreCxt for this. Not only would we be able to more easily do
	// multiplanar writes, but if any of this fails, we can back out without changes to our
	// working context

	int i;

	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);
	if(!pageBuffer || !spareData) {
		bufferPrintf("ftl: ftl_commit_cxt ran out of memory!\r\n");
		return ERROR_ARG;
	}

	// We need to precalculate how many pages we'd need to write to determine if we should start a new block.

	int eraseCounterPages;
	int readCounterPages;
	int mapPages;
	int offsetsPages;

	eraseCounterPages = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if((((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0)
		eraseCounterPages++;

	readCounterPages = eraseCounterPages;
	
	mapPages = (Geometry->userSuBlksTotal * sizeof(uint16_t)) / Geometry->bytesPerPage;
	if(((Geometry->userSuBlksTotal * sizeof(uint16_t)) % Geometry->bytesPerPage) != 0)
		mapPages++;

	offsetsPages = (Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) / Geometry->bytesPerPage;
	if(((Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) % Geometry->bytesPerPage) != 0)
		offsetsPages++;

	int totalPages = eraseCounterPages + readCounterPages + mapPages + offsetsPages + 1 /* for the SID */ + 1 /* for FTLCxt */;

	uint16_t curBlock = pstFTLCxt->FTLCtrlPage / Geometry->pagesPerSuBlk;
	if((pstFTLCxt->FTLCtrlPage + totalPages) >= ((curBlock * Geometry->pagesPerSuBlk) + Geometry->pagesPerSuBlk))
	{
		// looks like we would be overflowing into the next block, force the next ctrl page to be on a fresh
		// block in that case
		pstFTLCxt->FTLCtrlPage = (curBlock * Geometry->pagesPerSuBlk) + Geometry->pagesPerSuBlk - 1;
	}

	int pagesToWrite;

	pagesToWrite = eraseCounterPages;

	for(i = 0; i < pagesToWrite; i++) {
		if(!ftl_next_ctrl_page())
		{
			bufferPrintf("ftl: cannot allocate next FTL ctrl page\r\n");
			goto ftl_commit_cxt_error_release;
		}

		pstFTLCxt->pages_for_pawEraseCounterTable[i] = pstFTLCxt->FTLCtrlPage;

		int toWrite = Geometry->bytesPerPage;
		if(toWrite > (((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
			toWrite = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
		}

		memcpy(pageBuffer, ((uint8_t*)pstFTLCxt->pawEraseCounterTable) + (i * Geometry->bytesPerPage), toWrite);
		memset(pageBuffer + toWrite, 0, Geometry->bytesPerPage - toWrite);

		memset(spareData, 0xFF, sizeof(SpareData));
		spareData->meta.usnDec = pstFTLCxt->usnDec;
		spareData->type1 = 0x46;
		spareData->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_pawEraseCounterTable[i], pageBuffer, (uint8_t*) spareData) != 0)
			goto ftl_commit_cxt_error_release;
	}

	pagesToWrite = readCounterPages;

	for(i = 0; i < pagesToWrite; i++) {
		if(!ftl_next_ctrl_page())
		{
			bufferPrintf("ftl: cannot allocate next FTL ctrl page\r\n");
			goto ftl_commit_cxt_error_release;
		}

		pstFTLCxt->pages_for_pawReadCounterTable[i] = pstFTLCxt->FTLCtrlPage;

		int toWrite = Geometry->bytesPerPage;
		if(toWrite > (((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
			toWrite = ((Geometry->userSuBlksTotal + 23) * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
		}

		memcpy(pageBuffer, ((uint8_t*)pstFTLCxt->pawReadCounterTable) + (i * Geometry->bytesPerPage), toWrite);
		memset(pageBuffer + toWrite, 0, Geometry->bytesPerPage - toWrite);

		memset(spareData, 0xFF, sizeof(SpareData));
		spareData->meta.usnDec = pstFTLCxt->usnDec;
		spareData->type1 = 0x49;
		spareData->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_pawReadCounterTable[i], pageBuffer, (uint8_t*) spareData) != 0)
			goto ftl_commit_cxt_error_release;
	}

	pagesToWrite = mapPages;

	for(i = 0; i < pagesToWrite; i++) {
		if(!ftl_next_ctrl_page())
		{
			bufferPrintf("ftl: cannot allocate next FTL ctrl page\r\n");
			goto ftl_commit_cxt_error_release;
		}

		pstFTLCxt->pages_for_pawMapTable[i] = pstFTLCxt->FTLCtrlPage;

		int toWrite = Geometry->bytesPerPage;
		if(toWrite > ((Geometry->userSuBlksTotal * sizeof(uint16_t)) - (i * Geometry->bytesPerPage))) {
			toWrite = (Geometry->userSuBlksTotal * sizeof(uint16_t)) - (i * Geometry->bytesPerPage);
		}

		memcpy(pageBuffer, ((uint8_t*)pstFTLCxt->pawMapTable) + (i * Geometry->bytesPerPage), toWrite);
		memset(pageBuffer + toWrite, 0, Geometry->bytesPerPage - toWrite);

		memset(spareData, 0xFF, sizeof(SpareData));
		spareData->meta.usnDec = pstFTLCxt->usnDec;
		spareData->type1 = 0x44;
		spareData->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_pawMapTable[i], pageBuffer, (uint8_t*) spareData) != 0)
			goto ftl_commit_cxt_error_release;
	}

	pagesToWrite = offsetsPages;

	for(i = 0; i < pagesToWrite; i++) {
		if(!ftl_next_ctrl_page())
		{
			bufferPrintf("ftl: cannot allocate next FTL ctrl page\r\n");
			goto ftl_commit_cxt_error_release;
		}

		pstFTLCxt->pages_for_wPageOffsets[i] = pstFTLCxt->FTLCtrlPage;

		int toWrite = Geometry->bytesPerPage;
		if(toWrite > ((Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) - (i * Geometry->bytesPerPage))) {
			toWrite = (Geometry->pagesPerSuBlk * (17 * sizeof(uint16_t))) - (i * Geometry->bytesPerPage);
		}

		memcpy(pageBuffer, ((uint8_t*)pstFTLCxt->wPageOffsets) + (i * Geometry->bytesPerPage), toWrite);
		memset(pageBuffer + toWrite, 0, Geometry->bytesPerPage - toWrite);

		memset(spareData, 0xFF, sizeof(SpareData));
		spareData->meta.usnDec = pstFTLCxt->usnDec;
		spareData->type1 = 0x45;
		spareData->meta.idx = i;

		if(VFL_Write(pstFTLCxt->pages_for_wPageOffsets[i], pageBuffer, (uint8_t*) spareData) != 0)
			goto ftl_commit_cxt_error_release;
	}

	{
		if(!ftl_next_ctrl_page())
		{
			bufferPrintf("ftl: cannot allocate next FTL ctrl page\r\n");
			goto ftl_commit_cxt_error_release;
		}

		pstFTLCxt->page_for_FTLCountsTable = pstFTLCxt->FTLCtrlPage;
		pstFTLCxt->hasFTLCountsTable = 0xFFFFFFFF;

		void* data;
		int size;

		memset(pageBuffer, 0, Geometry->bytesPerPage);

		FTL_GetStruct(FTLCountsTableSID, &data, &size);
		memcpy(pageBuffer, data, size);
		VFL_GetStruct(VFLData1SID, &data, &size);
		memcpy(pageBuffer + 0x200, data, size);
		VFL_GetStruct(VFLData5SID, &data, &size);
		memcpy(pageBuffer + 0x400, data, size);

		uint32_t unkSID = 0x10001;
		memcpy(pageBuffer + Geometry->bytesPerPage - sizeof(unkSID), &unkSID, sizeof(unkSID));

		memset(spareData, 0xFF, sizeof(SpareData));
		spareData->meta.usnDec = pstFTLCxt->usnDec;
		spareData->type1 = 0x47;
		spareData->meta.idx = 0;

		if(VFL_Write(pstFTLCxt->page_for_FTLCountsTable, pageBuffer, (uint8_t*) spareData) != 0)
			goto ftl_commit_cxt_error_release;
	}

	if(!ftl_next_ctrl_page())
	{
		bufferPrintf("ftl: cannot allocate next FTL ctrl page\r\n");
		goto ftl_commit_cxt_error_release;
	}

	pstFTLCxt->clean = 1;

	memset(spareData, 0xFF, sizeof(SpareData));
	spareData->meta.usnDec = pstFTLCxt->usnDec;
	spareData->type1 = 0x43; 
	if(VFL_Write(pstFTLCxt->FTLCtrlPage, (uint8_t*) pstFTLCxt, (uint8_t*) spareData) != 0)
		goto ftl_commit_cxt_error_release;

	free(pageBuffer);
	free(spareData);

	return TRUE;

ftl_commit_cxt_error_release:
	bufferPrintf("ftl: error committing FTLCxt!\r\n");

	free(pageBuffer);
	free(spareData);

	return FALSE;
}

static int ftl_mark_unclean()
{
	uint8_t* pageBuffer = (uint8_t*) malloc(Geometry->bytesPerPage);
	uint8_t* spareBuffer = (uint8_t*) malloc(Geometry->bytesPerSpare);
	if(!pageBuffer || !spareBuffer) {
		bufferPrintf("ftl: ftl_mark_unclean: out of memory\r\n");
		return FALSE;
	}

	if(!pstFTLCxt->clean)
		return TRUE;

	check_for_dirty_free_vb();

	int i;
	for(i = 0; i < 3; ++i)
	{
		if(!ftl_next_ctrl_page())
		{
			bufferPrintf("ftl: ftl_mark_unclean: could not get a ctrl page\r\n");
			goto error_release;
		}

		memset(pageBuffer, 0xFF, Geometry->bytesPerPage);
		((SpareData*)spareBuffer)->type1 = 0x4F;

		if(VFL_Write(pstFTLCxt->FTLCtrlPage, pageBuffer, spareBuffer) == 0)
		{
			pstFTLCxt->clean = 0;
			free(pageBuffer);
			free(spareBuffer);
			return TRUE;
		}

		// have an error, try again on the next block.
		uint16_t block = pstFTLCxt->FTLCtrlPage / Geometry->pagesPerSuBlk;
		pstFTLCxt->FTLCtrlPage = (block * Geometry->pagesPerSuBlk) + Geometry->pagesPerSuBlk - 1;	
	}

	bufferPrintf("ftl: ftl_mark_unclean failed!\r\n");

error_release:
	free(pageBuffer);
	free(spareBuffer);
	return FALSE;
}

static FTLCxtLog* ftl_prepare_log(uint16_t lbn)
{
	FTLCxtLog* pLog = ftl_get_log(lbn);

	if(pLog == NULL)
	{
		int i;
		for(i = 0; i < 17; ++i)
		{
			if((pstFTLCxt->pLog[i].wVbn != 0xFFFF) && (pstFTLCxt->pLog[i].pagesUsed == 0))
			{
				pLog = &pstFTLCxt->pLog[i];
				break;
			}
		}

		if(pLog == NULL)
		{
			if(pstFTLCxt->wNumOfFreeVb < 3)
			{
				bufferPrintf("ftl: prepare_log WARNING: Pool block leak detected!\r\n");
				return NULL;
			} else if(pstFTLCxt->wNumOfFreeVb == 3)
			{
				if(!ftl_merge(NULL))
				{
					bufferPrintf("ftl: block merged failed!\r\n");
					return NULL;
				}
			}

			for(i = 0; i < 17; ++i)
			{
				if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
				       break;
			}

			pLog = &pstFTLCxt->pLog[i];

			if(!ftl_get_free_vb(&pLog->wVbn))
			{
				bufferPrintf("ftl: prepare_log could not get free vb\r\n");
				return NULL;
			}
		}

		memset(pLog->wPageOffsets, 0xFF, Geometry->pagesPerSuBlk * sizeof(uint16_t));
		pLog->wLbn = lbn;
		pLog->pagesUsed = 0;
		pLog->pagesCurrent = 0;
		pLog->isSequential = 1;
	}

	pLog->usn = pstFTLCxt->nextblockusn - 1;

	if(pstFTLCxt->nextblockusn == 1)
		memset(pstFTLCxt->pLog, 0, sizeof(FTLCxtLog) * 17);

	return pLog;
}

static inline void ftl_check_still_sequential(FTLCxtLog* pLog, uint32_t page)
{
	if((pLog->pagesUsed != pLog->pagesCurrent) || (pLog->wPageOffsets[page] != page))
		pLog->isSequential = 0;
}

static int ftl_copy_page(uint32_t src, uint32_t dest, uint32_t lpn, uint32_t isSequential)
{
	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);

	int ret = VFL_Read(src, pageBuffer, (uint8_t*) spareData, TRUE, NULL);

	memset(spareData, 0xFF, Geometry->bytesPerSpare);
	
	if(ret == ERROR_EMPTYBLOCK)
		memset(pageBuffer, 0, Geometry->bytesPerPage);
	else if(ret != 0)
		spareData->eccMark = 0x55;

	// FIXME: In CPICH, not all uses of this seem to increment this value. Find out whose fail it is.
	// Incrementing shouldn't hurt, though.
	spareData->user.usn = ++pstFTLCxt->nextblockusn;
	spareData->user.logicalPageNumber = lpn;

	// This isn't always done either
	if(isSequential == 1 && ((dest % Geometry->pagesPerSuBlk) == (Geometry->pagesPerSuBlk - 1)))
		spareData->type1 = 0x41;
	else
		spareData->type1 = 0x40;

	ret = VFL_Write(dest, pageBuffer, (uint8_t*) spareData);
	if(ret != 0)
		goto error_release;

	free(pageBuffer);
	free(spareData);
	return TRUE;

error_release:
	free(pageBuffer);
	free(spareData);

	return FALSE;
}

static int ftl_copy_block(uint16_t lSrc, uint16_t vDest)
{
	int error = FALSE;
	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);

	++pstFTLCxt->nextblockusn;

	int i;
	for(i = 0; i < Geometry->pagesPerSuBlk; ++i)
	{
		int ret = FTL_Read(lSrc * Geometry->pagesPerSuBlk + i, 1, pageBuffer);
		memset(spareData, 0xFF, Geometry->bytesPerSpare);
		if(ret)
			spareData->eccMark = 0x55;

		spareData->user.logicalPageNumber = lSrc * Geometry->pagesPerSuBlk + i;
		spareData->user.usn = pstFTLCxt->nextblockusn;
		if(i == (Geometry->pagesPerSuBlk - 1))
			spareData->type1 = 0x41;
		else
			spareData->type1 = 0x40;

		if(VFL_Write(vDest * Geometry->pagesPerSuBlk + i, pageBuffer, (uint8_t*) spareData) != 0)
		{
			error = TRUE;
			break;
		}
	}

	if(error)
	{
		++pstFTLCxt->pawEraseCounterTable[vDest];
		pstFTLCxt->pawReadCounterTable[vDest] = 0;

		if(VFL_Erase(vDest) != 0)
		{
			bufferPrintf("ftl: ftl_copy_block failed to erase after failure!\r\n");
			goto error_release;
		}

		bufferPrintf("ftl: ftl_copy_block failed!\r\n");
		goto error_release;
	}

	free(pageBuffer);
	free(spareData);
	return TRUE;

error_release:
	free(pageBuffer);
	free(spareData);

	return FALSE;
}

static int ftl_compact_scattered(FTLCxtLog* pLog)
{
	++FTLCountsTable.compactScatteredCount;

	if(pLog->pagesCurrent == 0)
	{
		// nothing useful in here, just release it.
		if(!ftl_set_free_vb(pLog->wVbn))
		{
			bufferPrintf("ftl: ftl_compact_scattered cannot release vb!\r\n");
		}

		pLog->wVbn = 0xFFFF;
		++pstFTLCxt->swapCounter;
		return TRUE;
	}

	// make a backup
	pstFTLCxt->pLog[17].usn  = pLog->usn;
	pstFTLCxt->pLog[17].wVbn  = pLog->wVbn;
	pstFTLCxt->pLog[17].wLbn  = pLog->wLbn;
	pstFTLCxt->pLog[17].pagesUsed  = pLog->pagesUsed;
	pstFTLCxt->pLog[17].pagesCurrent  = pLog->pagesCurrent;
	pstFTLCxt->pLog[17].isSequential  = pLog->isSequential;

	memcpy(pstFTLCxt->pLog[17].wPageOffsets, pLog->wPageOffsets, Geometry->pagesPerSuBlk); 

	int error = FALSE;
	
	int i;
	for(i = 0; i < 4; ++i)
	{
		uint16_t newBlock;
		if(!ftl_get_free_vb(&newBlock))
		{
			bufferPrintf("ftl: ftl_compact_scattered ran out of free vb!\r\n");
			goto error_release;
		}

		pLog->pagesUsed = 0;
		pLog->pagesCurrent = 0;
		pLog->isSequential = 1;
		pLog->wVbn = newBlock;

		int page;
		for(page = 0; page < Geometry->pagesPerSuBlk; ++page)
		{
			if(pLog->wPageOffsets[page] != 0xFFFF)
			{
				uint32_t lpn = pLog->wLbn * Geometry->pagesPerSuBlk + page;
				uint32_t newPage = newBlock * Geometry->pagesPerSuBlk + pLog->pagesUsed;
				uint32_t oldPage = pstFTLCxt->pLog[17].wVbn * Geometry->pagesPerSuBlk + pLog->wPageOffsets[page];
				if(!ftl_copy_page(oldPage, newPage, lpn, pLog->isSequential))
				{
					error = TRUE;
					break;
				}
				pLog->wPageOffsets[page] = pLog->pagesUsed++;
				++pLog->pagesCurrent;
				ftl_check_still_sequential(pLog, page);
			}
		}

		if(pstFTLCxt->pLog[17].pagesCurrent != pLog->pagesCurrent)
			error = TRUE;

		if(!error)
		{
			if(!ftl_set_free_vb(pstFTLCxt->pLog[17].wVbn))
			{
				bufferPrintf("ftl: ftl_compact_scattered could not set old Vb free!\r\n");
				goto error_release;
			}

			return TRUE;
		}

		if(!ftl_set_free_vb(pLog->wVbn))
		{
			bufferPrintf("ftl: ftl_compact_scattered could not set failed Vb free!\r\n");
			// better just to continue anyway
		}

		// restore the backup
		pLog->usn = pstFTLCxt->pLog[17].usn;
		pLog->wVbn = pstFTLCxt->pLog[17].wVbn;
		pLog->wLbn = pstFTLCxt->pLog[17].wLbn;
		pLog->pagesUsed = pstFTLCxt->pLog[17].pagesUsed;
		pLog->pagesCurrent = pstFTLCxt->pLog[17].pagesCurrent;
		pLog->isSequential = pstFTLCxt->pLog[17].isSequential;

		memcpy(pLog->wPageOffsets, pstFTLCxt->pLog[17].wPageOffsets, Geometry->pagesPerSuBlk); 
	}

error_release:
	return FALSE;
}

static int ftl_simple_merge(FTLCxtLog* pLog)
{
	int i;
	int error = FALSE;
	uint16_t block;

	++FTLCountsTable.simpleMergeCount;

	for(i = 0; i < 4; ++i)
	{
		if(!ftl_get_free_vb(&block))
		{
			bufferPrintf("ftl: ftl_simple_merge can't get free vb!\r\n");
			return FALSE;
		}

		if(ftl_copy_block(pLog->wLbn, block))
		{
			error = FALSE;
			break;
		}

		error = TRUE;
		if(!ftl_set_free_vb(block))
		{
			bufferPrintf("ftl: ftl_simple_merge can't set free vb after failure!\r\n");
			return FALSE;
		}
	}

	if(error)
	{
		bufferPrintf("ftl: ftl_simple_merge failed!\r\n");
		return FALSE;
	}

	if(!ftl_set_free_vb(pLog->wVbn))
	{
			bufferPrintf("ftl: ftl_simple_merge can't set free scatter vb!\r\n");
			return FALSE;
	}

	pLog->wVbn = 0xFFFF;

	if(!ftl_set_free_vb(pstFTLCxt->pawMapTable[pLog->wLbn]))
	{
			bufferPrintf("ftl: ftl_simple_merge can't set free map vb!\r\n");
			return FALSE;
	}

	pstFTLCxt->pawMapTable[pLog->wLbn] = block;

	return TRUE;
}

static int ftl_copy_merge(FTLCxtLog* pLog)
{
	if((pLog->isSequential != 1) || (pLog->pagesCurrent != pLog->pagesUsed))
	{
		bufferPrintf("ftl: attempted ftl_copy_merge on non-sequential scatter block!\r\n");
		return FALSE;
	}

	if(pLog->pagesUsed >= Geometry->pagesPerSuBlk)
		++FTLCountsTable.copyMergeWhileFullCount;
	else
		++FTLCountsTable.copyMergeWhileNotFullCount;

	for(; pLog->pagesUsed < Geometry->pagesPerSuBlk; ++pLog->pagesUsed)
	{
		uint32_t lpn = pLog->wLbn * Geometry->pagesPerSuBlk  + pLog->pagesUsed;
		uint32_t newPage = pLog->wVbn * Geometry->pagesPerSuBlk + pLog->pagesUsed;
		uint32_t oldPage = pstFTLCxt->pawMapTable[pLog->wLbn] * Geometry->pagesPerSuBlk + pLog->pagesUsed;

		// wtf, this isn't really all sequential!
		if(pLog->wPageOffsets[pLog->pagesUsed] != 0xFFFF)
			return ftl_simple_merge(pLog);

		// try the simple merge if this fails
		if(!ftl_copy_page(oldPage, newPage, lpn, 1))
			return ftl_simple_merge(pLog);
	}

	if(!ftl_set_free_vb(pstFTLCxt->pawMapTable[pLog->wLbn]))
	{
			bufferPrintf("ftl: ftl_copy_merge can't set free map vb!\r\n");
			return FALSE;
	}

	// replace it with our now completed block
	pstFTLCxt->pawMapTable[pLog->wLbn] = pLog->wVbn;

	// set this log block as free
	pLog->wVbn = 0xFFFF;

	return TRUE;
}

static int ftl_merge(FTLCxtLog* pLog)
{
	if(!ftl_mark_unclean())
	{
		bufferPrintf("ftl: merge failed - cannot open new mark context\r\n");
		return FALSE;
	}

	uint32_t oldest = 0xFFFFFFFF;
	uint32_t mostCurrent = 0;
	if(pLog == NULL)
	{
		int i;

		// find one to swap out
		for(i = 0; i < 17; ++i)
		{
			if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
				continue;
			
			if(pstFTLCxt->pLog[i].pagesUsed == 0 || pstFTLCxt->pLog[i].pagesCurrent == 0)
			{
				bufferPrintf("ft: merge error - we still have logs that can be used instead!\r\n");
				return FALSE;
			}

			if(pstFTLCxt->pLog[i].usn < oldest || (pstFTLCxt->pLog[i].usn == oldest && pstFTLCxt->pLog[i].pagesCurrent > mostCurrent))
			{
				pLog = &pstFTLCxt->pLog[i];
				oldest = pstFTLCxt->pLog[i].usn;
				mostCurrent = pstFTLCxt->pLog[i].pagesCurrent;
			}
		}

		if(pLog == NULL)
			return FALSE;
	} else if(pLog->pagesCurrent < (Geometry->pagesPerSuBlk / 2))
	{
		// less than half the pages in this log seems to be current, let's get rid of the crap and just reuse this one.

		++pstFTLCxt->swapCounter;
		return ftl_compact_scattered(pLog);
	}

	if(pLog->isSequential == 1)
	{
		if(!ftl_copy_merge(pLog))
		{
			bufferPrintf("ftl: simple merge failed\r\n");
			return FALSE;
		}
		++pstFTLCxt->swapCounter;
		return TRUE;
	}
	else
	{
		if(!ftl_simple_merge(pLog))
		{
			bufferPrintf("ftl: simple merge failed\r\n");
			return FALSE;
		}
		++pstFTLCxt->swapCounter;
		return TRUE;
	}
}

int ftl_auto_wearlevel()
{
	int i;
	uint16_t smallestEraseCount = 0xFFFF;
	uint16_t leastErasedBlock = 0;
	uint16_t leastErasedBlockLbn = 0;
	uint16_t largestEraseCount = 0;
	uint16_t mostErasedFreeBlock = 0;
	uint16_t mostErasedFreeBlockIdx = 20;

	if(!ftl_mark_unclean())
	{
		bufferPrintf("ftl: auto wearlevel failed due to mark_unclean failure\r\n");
		return FALSE;
	}

	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; ++i)
	{
		int idx = (pstFTLCxt->nextFreeIdx + i) % 20;

		if(pstFTLCxt->awFreeVb[idx] == 0xFFFF)
			continue;

		if((mostErasedFreeBlockIdx == 20) || pstFTLCxt->pawEraseCounterTable[pstFTLCxt->awFreeVb[idx]] > largestEraseCount)
		{
			mostErasedFreeBlockIdx = idx;
			mostErasedFreeBlock = pstFTLCxt->awFreeVb[idx];
			largestEraseCount = pstFTLCxt->pawEraseCounterTable[mostErasedFreeBlock];
		}
	}

	for(i = 0; i < Geometry->userSuBlksTotal; ++i)
	{
		if(pstFTLCxt->pawEraseCounterTable[pstFTLCxt->pawMapTable[i]] > largestEraseCount)
			largestEraseCount = pstFTLCxt->pawEraseCounterTable[pstFTLCxt->pawMapTable[i]];

		// don't swap stuff with log blocks attached
		if(ftl_get_log(i) != NULL)
			continue;

		if(pstFTLCxt->pawEraseCounterTable[pstFTLCxt->pawMapTable[i]] < smallestEraseCount)
		{
			leastErasedBlockLbn = i;
			leastErasedBlock = pstFTLCxt->pawMapTable[i];
			smallestEraseCount = pstFTLCxt->pawEraseCounterTable[leastErasedBlock];
		}
	}

	if(largestEraseCount == 0)
		return TRUE;

	if((largestEraseCount - smallestEraseCount) < 5)
		return TRUE;

	++pstFTLCxt->pawEraseCounterTable[mostErasedFreeBlock];
	pstFTLCxt->pawReadCounterTable[mostErasedFreeBlock] = 0;

	if(VFL_Erase(mostErasedFreeBlock) != 0)
	{
		bufferPrintf("ftl: auto wear-level cannot erase most erased free block\r\n");
		return FALSE;
	}

	++FTLCountsTable.blockSwapCount;

	if(!ftl_copy_block(leastErasedBlockLbn, mostErasedFreeBlock))
	{
		bufferPrintf("ftl: auto wear-level cannot copy least erased to most erased\r\n");
		return FALSE;
	}

	pstFTLCxt->pawMapTable[leastErasedBlockLbn] = mostErasedFreeBlock;

	++pstFTLCxt->pawEraseCounterTable[leastErasedBlock];
	pstFTLCxt->pawReadCounterTable[leastErasedBlock] = 0;

	if(VFL_Erase(leastErasedBlock) != 0)
	{
		bufferPrintf("ftl: auto wear-level cannot erase previously least erased block\r\n");
		return FALSE;
	}

	pstFTLCxt->awFreeVb[mostErasedFreeBlockIdx] = leastErasedBlock;

	return TRUE;
}

int FTL_Write(int logicalPageNumber, int totalPagesToWrite, uint8_t* pBuf) 
{
	int i;

	FTLCountsTable.totalPagesWritten += totalPagesToWrite;
	++FTLCountsTable.totalWrites;

	if(!pBuf) {
		return ERROR_ARG;
	}

	if(totalPagesToWrite == 0 || (logicalPageNumber + totalPagesToWrite) >= Geometry->userPagesTotal) {
		bufferPrintf("ftl: write has invalid input parameters\r\n");
		return ERROR_INPUT;
	}

	if(!ftl_mark_unclean())
	{
		bufferPrintf("ftl: write could not mark FTL as unclean!\r\n");
		return ERROR_ARG;
	}

	uint8_t* pageBuffer = malloc(Geometry->bytesPerPage);
	SpareData* spareData = (SpareData*) malloc(Geometry->bytesPerSpare);
	if(!pageBuffer || !spareData) {
		bufferPrintf("ftl: FTL_Write ran out of memory!\r\n");
		return ERROR_ARG;
	}

	for(i = 0; i < totalPagesToWrite; )
	{
		int lbn = (logicalPageNumber + i) / Geometry->pagesPerSuBlk;
		int offset = (logicalPageNumber + i) - (lbn * Geometry->pagesPerSuBlk);

		FTLCxtLog* pLog = ftl_prepare_log(lbn);

		if(pLog == NULL)
		{
			bufferPrintf("ftl: write failed to prepare log!\r\n");
			goto error_release;
		}

		if(offset == 0 && (totalPagesToWrite - i) >= Geometry->pagesPerSuBlk)
		{
			// we are replacing an entire block

			uint16_t vblock;

			if(pLog->pagesUsed != 0)
			{
				// we can't use this log block since it's not empty, get rid of it.
				if(!ftl_set_free_vb(pLog->wVbn))
				{
					bufferPrintf("ftl: write failed to set a free Vb when replacing an entire block!\r\n");
					goto error_release;
				}

				if(!ftl_get_free_vb(&vblock))
				{
					bufferPrintf("ftl: write failed to get a free Vb for replacing an entire block!\r\n");
					goto error_release;
				}
			} else
			{
				// we can just use this empty log block
				vblock = pLog->wVbn;
			}

			// don't need this log block anymore, we're now a full map block
			pLog->wVbn = 0xFFFF;

			++pstFTLCxt->nextblockusn;

			int j;
			for(j = 0; j < Geometry->pagesPerSuBlk; ++j)
			{
				memset(spareData, 0xFF, Geometry->bytesPerSpare);
				spareData->user.logicalPageNumber = logicalPageNumber + i + j;
				spareData->user.usn = pstFTLCxt->nextblockusn;
				if(j == (Geometry->pagesPerSuBlk - 1))
					spareData->type1 = 0x41;
				else
					spareData->type1 = 0x40;

				int tries = 0;
				for(tries = 0; tries < 4; ++tries)
				{
					if(VFL_Write((vblock * Geometry->pagesPerSuBlk) + j, 
							pBuf + ((i + j) * (Geometry->bytesPerPage)), (uint8_t*) spareData) == 0)
						break;
				}

				if(tries == 4)
				{
					bufferPrintf("ftl: write error during writing replacement block!\r\n");
					// FIXME: no real error handling here!
				}
			}

			if(!ftl_set_free_vb(pstFTLCxt->pawMapTable[lbn]))
			{
				bufferPrintf("ftl: write failed to set a free Vb after writing replacement block!\r\n");
				goto error_release;
			}

			pstFTLCxt->pawMapTable[lbn] = vblock;
			i += Geometry->pagesPerSuBlk;
		} else
		{
			// we'll have to use the log since we're not replacing the whole block

			if(pLog->pagesUsed == Geometry->pagesPerSuBlk)
			{
				// oh no, this log is full. we have to commit it
				if(!ftl_merge(pLog))
				{
					bufferPrintf("ftl: write failed to merge in the log!\r\n");
					goto error_release;
				}

				pLog = ftl_prepare_log(lbn);

				if(pLog == NULL)
				{
					bufferPrintf("ftl: write failed to prepare log after merging!\r\n");
					goto error_release;
				}
			}

			int pagesCanWrite = totalPagesToWrite - i;

			// don't overflow the log
			if(pagesCanWrite > (Geometry->pagesPerSuBlk - pLog->pagesUsed))
				pagesCanWrite = Geometry->pagesPerSuBlk - pLog->pagesUsed;

			// don't write pages belonging to some other block here
			if(pagesCanWrite > (Geometry->pagesPerSuBlk - offset))
				pagesCanWrite = Geometry->pagesPerSuBlk - offset;

			int origPagesUsed = pLog->pagesUsed;

			int j;
			for(j = 0; j < pagesCanWrite; ++j)
			{
				memset(spareData, 0xFF, Geometry->bytesPerSpare);
				spareData->user.logicalPageNumber = logicalPageNumber + i + j;
				spareData->user.usn = ++pstFTLCxt->nextblockusn;
				if(((origPagesUsed + j) == (Geometry->pagesPerSuBlk - 1)) && pLog->isSequential)
					spareData->type1 = 0x41;
				else
					spareData->type1 = 0x40;

				int abspage = pLog->wVbn * Geometry->pagesPerSuBlk + origPagesUsed + j;

				int tries = 0;
				for(tries = 0; tries < 4; ++tries)
				{
					if(VFL_Write(abspage, 
							pBuf + ((i + j) * (Geometry->bytesPerPage)), (uint8_t*) spareData) == 0)
						break;
				}

				if(tries == 4)
				{
					bufferPrintf("ftl: write error during writing to log!\r\n");
					// FIXME: no real error handling here
				}

				if(pLog->wPageOffsets[offset + j] == 0xFFFF)
				{
					// This page wasn't current before but now it is
					++pLog->pagesCurrent;
				}

				pLog->wPageOffsets[offset + j] = origPagesUsed + j;
				++pLog->pagesUsed;

				if(pLog->isSequential == 1)
					ftl_check_still_sequential(pLog, offset + j);
			}

			i += pagesCanWrite;
		}

	}

	if(pstFTLCxt->swapCounter >= 300)
	{
		int tries;
		for(tries = 0; tries < 4; ++tries)
		{
			if(ftl_auto_wearlevel())
			{
				pstFTLCxt->swapCounter -= 20;
				break;
			}
		}
	}

	return 0;

error_release:
	free(pageBuffer);
	free(spareData);

	return ERROR_ARG;
}

int ftl_sync()
{
	int tries;

	if(pstFTLCxt->clean)
		return TRUE;

	check_for_dirty_free_vb();

	if(pstFTLCxt->swapCounter >= 20)
	{
		for(tries = 0; tries < 4; ++ tries)
		{
			if(ftl_auto_wearlevel())
			{
				pstFTLCxt->swapCounter -= 20;
				break;
			}
		}
	}

	for(tries = 0; tries < 4; ++ tries)
	{
		if(ftl_commit_cxt())
		{
			return TRUE;
		} else
		{
			// have some kind of error, try again on a new block
			uint16_t block = pstFTLCxt->FTLCtrlPage / Geometry->pagesPerSuBlk;
			pstFTLCxt->FTLCtrlPage = (block * Geometry->pagesPerSuBlk) + Geometry->pagesPerSuBlk - 1;	
		}
	}

	return FALSE;
}

int ftl_setup() {
	if(HasFTLInit)
		return 0;

	nand_setup();

	Geometry = nand_get_geometry();
	FTLData = nand_get_ftl_data();

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

	DebugPrintf("ftl: Attempting to read %d pages from first block of first bank.\r\n", Geometry->pagesPerBlock);
	uint8_t* buffer = malloc(Geometry->bytesPerPage);
	for(i = 0; i < Geometry->pagesPerBlock; i++) {
		int ret;
		if((ret = nand_read_alternate_ecc(0, i, buffer)) == 0) {
			uint32_t id = *((uint32_t*) buffer);
			if(id == FTL_ID_V1 || id == FTL_ID_V2 || id == FTL_ID_V3) {
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
	int curPage = offset / Geometry->bytesPerPage;
	int toRead = size;
	int pageOffset = offset - (curPage * Geometry->bytesPerPage);
	uint8_t* tBuffer = (uint8_t*) malloc(Geometry->bytesPerPage);
	while(toRead > 0) {
		if(FTL_Read(curPage, 1, tBuffer) != 0) {
			free(tBuffer);
			return FALSE;
		}

		int read = (((Geometry->bytesPerPage-pageOffset) > toRead) ? toRead : Geometry->bytesPerPage-pageOffset);
		memcpy(curLoc, tBuffer + pageOffset, read);
		curLoc += read;
		toRead -= read;
		pageOffset = 0;
		curPage++;
	}

	free(tBuffer);
	return TRUE;
}

int ftl_write(void* buffer, uint64_t offset, int size) {
	uint8_t* curLoc = (uint8_t*) buffer;
	int curPage = offset / Geometry->bytesPerPage;
	int toWrite = size;
	int pageOffset = offset - (curPage * Geometry->bytesPerPage);
	uint8_t* tBuffer = (uint8_t*) malloc(Geometry->bytesPerPage);
	while(toWrite > 0) {
		if(FTL_Read(curPage, 1, tBuffer) != 0) {
			free(tBuffer);
			return FALSE;
		}

		int write = (((Geometry->bytesPerPage - pageOffset) > toWrite) ? toWrite : Geometry->bytesPerPage - pageOffset);
		memcpy(tBuffer + pageOffset, curLoc, write);

		if(FTL_Write(curPage, 1, tBuffer) != 0) {
			free(tBuffer);
			return FALSE;
		}

		curLoc += write;
		toWrite -= write;
		pageOffset = 0;
		curPage++;
	}

	free(tBuffer);
	return TRUE;
}

void ftl_printdata() {
	int i, j;

	bufferPrintf("usnDec: %u\r\n", pstFTLCxt->usnDec);
	bufferPrintf("nextblockusn: %u\r\n", pstFTLCxt->nextblockusn);
	bufferPrintf("nextFreeIdx: %u\r\n", pstFTLCxt->nextFreeIdx);
	bufferPrintf("swapCounter: %u\r\n", pstFTLCxt->swapCounter);
	bufferPrintf("eraseCounterPagesDirty: %u\r\n", pstFTLCxt->eraseCounterPagesDirty);
	bufferPrintf("unk3: %u\r\n", pstFTLCxt->unk3);
	bufferPrintf("FTLCtrlPage: %u\r\n", pstFTLCxt->FTLCtrlPage);
	bufferPrintf("clean: %u\r\n", pstFTLCxt->clean);
	bufferPrintf("field_3C8: %u\r\n", pstFTLCxt->field_3C8);
	bufferPrintf("page_for_FTLCountsTable: %u\r\n", pstFTLCxt->page_for_FTLCountsTable);
	bufferPrintf("hasFTLCountsTable: %u\r\n", pstFTLCxt->hasFTLCountsTable);
	bufferPrintf("Total read count: %u\r\n", pstFTLCxt->totalReadCount);

	bufferPrintf("Free virtual blocks: %d\r\n", pstFTLCxt->wNumOfFreeVb);
	for(i = 0; i < pstFTLCxt->wNumOfFreeVb; i++)
	{
		bufferPrintf("\t%u: %u\r\n", i, pstFTLCxt->awFreeVb[i]);
	}

	bufferPrintf("Pages for pawMapTable:\r\n");
	for(i = 0; i < 18; i++)
	{
		bufferPrintf("\t%u: %u\r\n", i, pstFTLCxt->pages_for_pawMapTable[i]);
	}

	bufferPrintf("Pages for pawEraseCounterTable:\r\n");
	for(i = 0; i < 36; i++)
	{
		bufferPrintf("\t%u: %u\r\n", i, pstFTLCxt->pages_for_pawEraseCounterTable[i]);
	}

	bufferPrintf("Pages for wPageOffsets:\r\n");
	for(i = 0; i < 34; i++)
	{
		bufferPrintf("\t%u: %u\r\n", i, pstFTLCxt->pages_for_wPageOffsets[i]);
	}

	bufferPrintf("Pages for pawReadCounterTable:\r\n");
	for(i = 0; i < 36; i++)
	{
		bufferPrintf("\t%u: %u\r\n", i, pstFTLCxt->pages_for_pawReadCounterTable[i]);
	}

	bufferPrintf("Log blocks (page-by-page remapping):\r\n");
	for(i = 0; i < 17; i++) {
		if(pstFTLCxt->pLog[i].wVbn == 0xFFFF)
			continue;

		bufferPrintf("\tpLog %d: logical block %d => virtual block %d\r\n", i, pstFTLCxt->pLog[i].wLbn, pstFTLCxt->pLog[i].wVbn);
		for(j = 0; j < Geometry->pagesPerSuBlk; j++)
		{
			if(pstFTLCxt->pLog[i].wPageOffsets[j] != 0xFFFF) {
				bufferPrintf("\t\tpage %d => page %d\r\n", j, pstFTLCxt->pLog[i].wPageOffsets[j]);
			}
		}
	}

	bufferPrintf("Block map:\r\n");
	for(i = 0; i < Geometry->userSuBlksTotal; i++) {
		bufferPrintf("\tlogical block %d => virtual block %d\r\n", i, pstFTLCxt->pawMapTable[i]);
	}

	bufferPrintf("Read counts:\r\n");
	for(i = 0; i < Geometry->userSuBlksTotal; i++) {
		bufferPrintf("\tvirtual block %d: %d\r\n", i, pstFTLCxt->pawReadCounterTable[i]);
	}

	bufferPrintf("Erase counts:\r\n");
	for(i = 0; i < Geometry->userSuBlksTotal; i++) {
		bufferPrintf("\tvirtual block %d: %d\r\n", i, pstFTLCxt->pawEraseCounterTable[i]);
	}

}
