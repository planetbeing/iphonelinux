#ifndef FTL_H
#define FTL_H

#include "openiboot.h"

#define ERROR_INPUT 0x80030000

typedef struct VFLCxt {
	uint32_t usnInc;				// 0x000
	uint16_t FTLCtrlBlock[3];			// 0x004
	uint8_t unk1[2];				// 0x00A
	uint32_t usnDec;				// 0x00C
	uint16_t activecxtblock;			// 0x010
	uint16_t nextcxtpage;				// 0x012
	uint8_t unk2[2];				// 0x014
	uint16_t field_16;				// 0x016
	uint16_t field_18;				// 0x018
	uint16_t numReservedBlocks;			// 0x01A
	uint16_t reservedBlockPoolStart;		// 0x01C
	uint16_t totalReservedBlocks;			// 0x01E
	uint16_t reservedBlockPoolMap[0x334];		// 0x020
	uint8_t badBlockTable[0x11a];			// 0x688
	uint16_t VFLCxtBlock[4];			// 0x7A2
	uint16_t remappingScheduledStart;		// 0x7AA
	uint8_t unk3[0x4C];				// 0x7AC
	uint32_t checksum1;				// 0x7F8
	uint32_t checksum2;				// 0x7FC
} VFLCxt;

typedef struct FTLCxtLog {
	uint32_t usn;					// 0x0
	uint16_t wVbn;					// 0x4
	uint16_t wLbn;					// 0x6
	uint16_t* wPageOffsets;				// 0x8
	uint16_t pagesUsed;				// 0xC
	uint16_t pagesCurrent;				// 0xE
	uint32_t isSequential;				// 0x10
} FTLCxtLog;

typedef struct FTLCxtElement2 {
	uint16_t field_0;				// 0x0
	uint16_t field_2;				// 0x2
} FTLCxtElement2;

typedef struct FTLCxt {
	uint32_t usnDec;				// 0x0
	uint32_t nextblockusn;				// 0x4
	uint16_t wNumOfFreeVb;				// 0x8
	uint16_t nextFreeIdx;				// 0xA
	uint16_t swapCounter;				// 0xC
	uint16_t awFreeVb[20];				// 0xE
	uint16_t field_36;				// 0x36
	uint32_t pages_for_pawMapTable[18];		// 0x38
	uint32_t pages_for_pawEraseCounterTable[36];	// 0x80
	uint32_t pages_for_wPageOffsets[34];		// 0x110
	uint16_t* pawMapTable;				// 0x198
	uint16_t* pawEraseCounterTable;			// 0x19C
	uint16_t* wPageOffsets;				// 0x1A0
	FTLCxtLog pLog[18];				// 0x1A4
	uint32_t eraseCounterPagesDirty;		// 0x30C
	uint16_t unk3;					// 0x310
	uint16_t FTLCtrlBlock[3];			// 0x312
	uint32_t FTLCtrlPage;				// 0x318
	uint32_t clean;					// 0x31C
	uint32_t pages_for_pawReadCounterTable[36];	// 0x320
	uint16_t* pawReadCounterTable;			// 0x3B0
	FTLCxtElement2 elements2[5];			// 0x3B4
	uint32_t field_3C8;				// 0x3C8
	uint32_t totalReadCount;			// 0x3CC
	uint32_t page_for_FTLCountsTable;		// 0x3D0
	uint32_t hasFTLCountsTable;			// 0x3D4, set to -1 if yes
	uint8_t field_3D8[0x420];			// 0x3D8
	uint32_t versionLower;				// 0x7F8
	uint32_t versionUpper;				// 0x7FC
} FTLCxt;

typedef struct VFLData1Type {
	uint64_t field_0;
	uint64_t field_8;
	uint64_t field_10;
	uint64_t field_18;
	uint64_t field_20;
	uint64_t field_28;
	uint64_t field_30;
	uint64_t field_38;
	uint64_t field_40;
} VFLData1Type;

typedef struct FTLCountsTableType {
	uint64_t totalPagesWritten;		// 0x0
	uint64_t totalPagesRead;		// 0x8
	uint64_t totalWrites;			// 0x10
	uint64_t totalReads;			// 0x18
	uint64_t compactScatteredCount;		// 0x20
	uint64_t copyMergeWhileFullCount;	// 0x28
	uint64_t copyMergeWhileNotFullCount;	// 0x30
	uint64_t simpleMergeCount;		// 0x38
	uint64_t blockSwapCount;		// 0x40
	uint64_t ftlRestoresCount;		// 0x48
	uint64_t field_50;
} FTLCountsTableType;

typedef enum FTLStruct {
	FTLCountsTableSID = 0x1000200
} FTLStruct;

typedef enum VFLStruct {
	VFLData1SID = 0x2000200,
	VFLData5SID = 0x2000500
} VFLStruct;

extern int HasFTLInit;

int ftl_setup();
int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* did_error);
int VFL_Erase(uint16_t block);
int FTL_Read(int logicalPageNumber, int totalPagesToRead, uint8_t* pBuf);
int FTL_Write(int logicalPageNumber, int totalPagesToRead, uint8_t* pBuf);
int ftl_read(void* buffer, uint64_t offset, int size);
int ftl_write(void* buffer, uint64_t offset, int size);
void ftl_printdata();
int ftl_sync();

#endif
