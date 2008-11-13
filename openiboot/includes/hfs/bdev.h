#ifndef BDEV_H
#define BDEV_H

#include "openiboot.h"
#include "hfs/common.h"

typedef struct MBRPartitionRecord {
	uint8_t status;
	uint8_t beginHead;
	uint8_t beginSectorCyl;
	uint8_t beginCyl;
	uint8_t type;
	uint8_t endHead;
	uint8_t endSectorCyl;
	uint8_t endCyl;
	uint32_t beginLBA;
	uint32_t numSectors;
} MBRPartitionRecord;

typedef struct MBR {
	uint8_t code[0x1B8];
	uint32_t signature;
	uint16_t padding;
	MBRPartitionRecord partitions[4];
	uint16_t magic;
} __attribute__ ((packed)) MBR;

extern int HasBDevInit;

int bdev_setup();
io_func* bdev_open(int partition);

#endif
