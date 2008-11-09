#ifndef NAND_H
#define NAND_H

#include "openiboot.h"

#define ERROR_ARG 0x80010000
#define ERROR_NAND 0x80020000
#define ERROR_TIMEOUT 0x1F
#define ERROR_ECC 0x17
#define ERROR_EMPTYBLOCK 0x1

typedef struct NANDDeviceType {
	uint32_t id;
	uint16_t blocksPerBank;
	uint16_t pagesPerBlock;
	uint16_t sectorsPerPage;
	uint16_t bytesPerSpare;
	uint8_t NANDSetting2;
	uint8_t NANDSetting1;
	uint8_t NANDSetting3;
	uint8_t NANDSetting4;
	uint32_t userSubBlksTotal;
	uint32_t ecc1;
	uint32_t ecc2;
} NANDDeviceType;

typedef struct UnknownNANDType {
	uint16_t field_0;
	uint16_t field_2;
	uint16_t field_4;		// reservoir blocks?
	uint16_t field_6;
	uint16_t field_8;
} UnknownNANDType;

typedef struct SpareData {
	uint8_t field_0;
	uint8_t field_1;
	uint8_t field_2;
	uint8_t field_3;
	uint8_t field_4;
	uint8_t field_5;
	uint8_t field_6;
	uint8_t field_7;
	uint8_t field_8;
	uint8_t field_9;
	uint8_t field_A;
	uint8_t field_B;
} __attribute__ ((packed)) SpareData;

typedef struct NANDData {
	uint32_t field_0;
	uint16_t field_4;
	uint16_t sectorsPerPage;
	uint16_t pagesPerBlock;
	uint16_t pagesPerSubBlk;
	uint32_t pagesPerBank;
	uint32_t pagesTotal;
	uint16_t subBlksTotal;
	uint16_t userSubBlksTotal;
	uint32_t userPagesTotal;
	uint16_t blocksPerBank;
	uint16_t bytesPerPage;
	uint16_t bytesPerSpare;
	uint16_t field_22;
	uint32_t field_24;
	uint32_t DeviceID;
	uint16_t banksTotal;
	uint8_t field_2E;
	uint8_t field_2F;
} NANDData;

int nand_setup();
int nand_bank_reset(int bank, int timeout);
int nand_read(int bank, int page, uint8_t* buffer, uint8_t* spare, int doECC, int checkBadBlocks);
int nand_read_alternate_ecc(int bank, int page, uint8_t* buffer);
NANDData* nand_get_geometry();
UnknownNANDType* nand_get_data();

#endif
