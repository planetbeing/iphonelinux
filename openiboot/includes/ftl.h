#ifndef FTL_H
#define FTL_H

#include "openiboot.h"

#define ERROR_INPUT 0x80030000

typedef struct VFLCxt {
	uint32_t field_0;				// 0x000
	uint16_t field_4[3];				// 0x004
	uint8_t unk1[0x10];				// 0x00A
	uint16_t numReservedBlocks;			// 0x01A
	uint16_t reservedBlockPoolStart;		// 0x01C
	uint16_t field_1E;				// 0x01E
	uint16_t reservedBlockPoolMap[0x334];		// 0x020
	uint8_t badBlockTable[0x11a];			// 0x688
	uint16_t VFLCxtBlock[4];			// 0x7A2
	uint8_t unk3[0x4E];				// 0x7AA
	uint32_t checksum1;				// 0x7F8
	uint32_t checksum2;				// 0x7FC
} VFLCxt;

typedef struct FTLCxtLog {
	void* field_0;					// 0x0
	uint16_t wVbn;					// 0x4
	uint16_t field_6;				// 0x6
	uint16_t* field_8;				// 0x8
	uint16_t field_C;				// 0xC
	uint16_t field_E;				// 0xE
	uint32_t field_10;				// 0x10
} FTLCxtLog;

typedef struct FTLCxtElement2 {
	uint16_t field_0;				// 0x0
	uint16_t field_2;				// 0x2
} FTLCxtElement2;

typedef struct FTLCxt {
	uint8_t unk1[0x38];				// 0x0
	uint32_t pages_for_dataVbn[18];			// 0x38
	uint32_t pages_for_19C[36];			// 0x80
	uint32_t pages_for_1A0[34];			// 0x110
	uint16_t* dataVbn;				// 0x198
	uint16_t* field_19C;				// 0x19C
	uint16_t* field_1A0;				// 0x1A0
	FTLCxtLog pLog[18];				// 0x1A4
	uint8_t unk2[6];				// 0x30C
	uint16_t thing[3];				// 0x312
	uint32_t field_318;				// 0x318
	uint32_t field_31C;				// 0x31C
	uint32_t pages_for_3B0[36];			// 0x320
	uint16_t* field_3B0;				// 0x3B0
	FTLCxtElement2 elements2[5];			// 0x3B4
	uint32_t field_3C8;				// 0x3C8
	uint32_t field_3CC;				// 0x3CC
	uint32_t field_3D0;				// 0x3D0
	uint32_t field_3D4;				// 0x3D4
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

typedef struct FTLData1Type {
	uint64_t field_0;
	uint64_t field_8;
	uint64_t field_10;
	uint64_t field_18;
	uint64_t field_20;
	uint64_t field_28;
	uint64_t field_30;
	uint64_t field_38;
	uint64_t field_40;
	uint64_t field_48;
	uint64_t field_50;
} FTLData1Type;

typedef enum FTLStruct {
	FTLData1SID = 0x1000200
} FTLStruct;

typedef enum VFLStruct {
	VFLData1SID = 0x2000200,
	VFLData5SID = 0x2000500
} VFLStruct;

extern int HasFTLInit;

int ftl_setup();
int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* did_error);
int FTL_Read(int logicalPageNumber, int totalPagesToRead, uint8_t* pBuf);
int ftl_read(void* buffer, uint64_t offset, int size);

#endif
