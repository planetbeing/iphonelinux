#ifndef FTL_H
#define FTL_H

#include "openiboot.h"

typedef struct VFLCxt {
	uint32_t field_0;				// 0x000
	uint8_t field_4[6];				// 0x004
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

typedef struct VFLData1Type {
	uint64_t field_8;
	uint64_t field_20;
} VFLData1Type;

int ftl_setup();
int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* did_error);

#endif
