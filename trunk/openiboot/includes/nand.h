#ifndef NAND_H
#define NAND_H

#include "openiboot.h"

#define ERROR_ARG 0x80010000
#define ERROR_TIMEOUT 0x1F

typedef struct NANDDeviceType {
	uint32_t id;
	uint16_t blocksPerBank;
	uint16_t unk1;
	uint16_t sectorsPerPage;
	uint16_t bytesPerSpare;
	uint32_t unk2;
	uint32_t userSubBlksTotal;
	uint32_t unk3;
	uint32_t unk4;
} NANDDeviceType;

int nand_setup();

#endif
