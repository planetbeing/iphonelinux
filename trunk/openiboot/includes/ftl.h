#ifndef FTL_H
#define FTL_H

#include "openiboot.h"

typedef struct VFLCxt {
	uint32_t field_0;
	uint8_t field_4[6];
	uint8_t unk1[0x798];
	uint16_t VFLCxtBlock[4];	// 0x7A2
	uint8_t unk2[0x4E];
	uint32_t checksum1;		// 0x7F8
	uint32_t checksum2;		// 0x7FC
} VFLCxt;

int ftl_setup();

#endif
