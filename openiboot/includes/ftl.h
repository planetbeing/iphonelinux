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

typedef struct FTLCxtElement {
	void* field_0;					// 0x0
	void* field_4;					// 0x4
	void* field_8;					// 0x8
	uint16_t field_C;				// 0xC
	uint16_t field_E;				// 0xE
	uint32_t field_10;				// 0x10
} FTLCxtElement;

typedef struct FTLCxt {
	uint8_t unk1[0x198];				// 0x0
	void* field_198;				// 0x198
	void* field_19C;				// 0x19C
	void* field_1A0;				// 0x1A0
	FTLCxtElement elements[18];			// 0x1A4
	uint8_t unk2[0x10];				// 0x30C
	void* field_31C;				// 0x31C
	uint8_t unk3[0x90];				// 0x320
	void* field_3B0;				// 0x3B0
	uint8_t unk4[0x24];				// 0x3B4
	uint8_t field_3D8[0x420];			// 0x3D8
	uint8_t field_7F8;				// 0x7F8
	uint8_t field_7FC;				// 0x7FC
} FTLCxt;

typedef struct VFLData1Type {
	uint64_t field_8;
	uint64_t field_20;
} VFLData1Type;

int ftl_setup();
int VFL_Read(uint32_t virtualPageNumber, uint8_t* buffer, uint8_t* spare, int empty_ok, int* did_error);
int VFL_ReadScatteredPagesInVb(uint32_t* virtualPageNumber, int count, uint8_t* main, uint8_t* spare, int* refresh_page);

#endif
