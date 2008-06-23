#include "openiboot.h"
#include "nor.h"
#include "hardware/nor.h"
#include "util.h"
#include "timer.h"
#include "mmu.h"

static NorInfo* probeNOR() {
	SET_REG16(NOR, DATA_MODE);
	GET_REG16(NOR);
	GET_REG16(NOR);
	SET_REG16(NOR, DATA_MODE);
	GET_REG16(NOR);
	GET_REG16(NOR);
	SET_REG16(NOR, DATA_MODE);
	GET_REG16(NOR);
	uint16_t temp = GET_REG16(NOR);

	bufferPrintf("NOR data=%x\r\n", GET_REG16(0x1C000000));
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
//	while(TRUE);
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

	bufferPrintf("successfully wrote to NOR!\r\n");
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

	bufferPrintf("successfully erased NOR sector!\r\n");
}

int nor_setup() {
	probeNOR();

	return 0;
}


