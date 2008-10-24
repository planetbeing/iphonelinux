#include "openiboot.h"
#include "nor.h"
#include "hardware/nor.h"
#include "util.h"
#include "timer.h"

static int NORSectorSize = 4096;

static NorInfo* probeNOR() {
	SET_REG16(NOR + COMMAND, COMMAND_UNLOCK);
	SET_REG16(NOR + LOCK, LOCK_UNLOCK);

	SET_REG16(NOR + COMMAND, COMMAND_IDENTIFY);
	GET_REG16(NOR);
	GET_REG16(NOR);
	uint16_t vendor = GET_REG16(NOR + VENDOR);
	uint16_t device = GET_REG16(NOR + DEVICE);

	SET_REG16(NOR + COMMAND, COMMAND_LOCK);
	SET_REG16(NOR, DATA_MODE);
	GET_REG16(NOR);
	GET_REG16(NOR);

	bufferPrintf("NOR vendor=%x, device=%x\r\n", (uint32_t) vendor, (uint32_t) device);

	return NULL;
}

void nor_write_word(uint32_t offset, uint16_t data) {

	SET_REG16(NOR + COMMAND, COMMAND_UNLOCK);
	SET_REG16(NOR + LOCK, LOCK_UNLOCK);

	SET_REG16(NOR + COMMAND, COMMAND_WRITE);
	SET_REG16(NOR + offset, data);

	while(TRUE) {
		udelay(40);
		if(GET_REG16(NOR + offset) != data) {
			bufferPrintf("failed to write to NOR\r\n");
		} else {
			break;
		}
	}
}

uint16_t nor_read_word(uint32_t offset) {
	return GET_REG16(NOR + offset);
}

void nor_erase_sector(uint32_t offset) {
	SET_REG16(NOR + COMMAND, COMMAND_UNLOCK);
	SET_REG16(NOR + LOCK, LOCK_UNLOCK);

	SET_REG16(NOR + COMMAND, COMMAND_ERASE);

	SET_REG16(NOR + COMMAND, COMMAND_UNLOCK);
	SET_REG16(NOR + LOCK, LOCK_UNLOCK);

	SET_REG16(NOR + offset, ERASE_DATA);

	while(TRUE) {
		udelay(50000);
		if(GET_REG16(NOR + offset) != 0xFFFF) {
			bufferPrintf("failed to erase NOR sector: %x %x\r\n", (unsigned int) GET_REG16(NOR + offset), GET_REG(0x20000000));
		} else {
			break;
		}
	}
}

void nor_read(void* buffer, int offset, int len) {
	uint16_t* alignedBuffer = (uint16_t*) buffer;
	for(; len >= 2; len -= 2) {
		*alignedBuffer = nor_read_word(offset);
		offset += 2;
		alignedBuffer++;
	}

	if(len > 0) {
		uint16_t lastWord = nor_read_word(offset);
		uint8_t* unalignedBuffer = (uint8_t*) alignedBuffer;
		*unalignedBuffer = *((uint8_t*)(&lastWord));
	}
}

void nor_write(void* buffer, int offset, int len) {
	int startSector = offset / NORSectorSize;
	int endSector = (offset + len) / NORSectorSize;

	int numSectors = endSector - startSector + 1;
	uint8_t* sectorsToChange = (uint8_t*) malloc(NORSectorSize * numSectors);
	nor_read(sectorsToChange, startSector * NORSectorSize, NORSectorSize * numSectors);

	int offsetFromStart = offset - (startSector * NORSectorSize);

	memcpy(sectorsToChange + offsetFromStart, buffer, len);

	int i;
	for(i = 0; i < numSectors; i++) {
		nor_erase_sector((i + startSector) * NORSectorSize);
		int j;
		uint16_t* curSector = (uint16_t*)(sectorsToChange + (i * NORSectorSize));
		for(j = 0; j < (NORSectorSize / 2); j++) {
			nor_write_word(((i + startSector) * NORSectorSize) + (j * 2), curSector[j]);
		}
	}

	free(sectorsToChange);
}

int getNORSectorSize() {
	return NORSectorSize;
}

int nor_setup() {
	probeNOR();

	return 0;
}

