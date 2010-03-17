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
	uint8_t WPPulseTime;
	uint8_t WEHighHoldTime;
	uint8_t NANDSetting3;
	uint8_t NANDSetting4;
	uint32_t userSuBlksTotal;
	uint32_t ecc1;
	uint32_t ecc2;
} NANDDeviceType;

typedef struct NANDFTLData {
	uint16_t sysSuBlks;
	uint16_t field_2;
	uint16_t field_4;		// reservoir blocks?
	uint16_t field_6;
	uint16_t field_8;
} NANDFTLData;

typedef struct SpareData {
	union {
		struct {
			uint32_t logicalPageNumber;
			uint32_t usn;
		} __attribute__ ((packed)) user;

		struct {
			uint32_t usnDec;
			uint16_t idx;
			uint8_t field_6;
			uint8_t field_7;
		} __attribute__ ((packed)) meta;
	};

	uint8_t type2;
	uint8_t type1;
	uint8_t eccMark;
	uint8_t field_B;

} __attribute__ ((packed)) SpareData;

typedef struct NANDData {
	uint32_t field_0;
	uint16_t field_4;
	uint16_t sectorsPerPage;
	uint16_t pagesPerBlock;
	uint16_t pagesPerSuBlk;
	uint32_t pagesPerBank;
	uint32_t pagesTotal;
	uint16_t suBlksTotal;
	uint16_t userSuBlksTotal;
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
	int* banksTable;
} NANDData;

extern int HasNANDInit;

int nand_setup();
int nand_bank_reset(int bank, int timeout);
int nand_read(int bank, int page, uint8_t* buffer, uint8_t* spare, int doECC, int checkBadBlocks);
int nand_read_multiple(uint16_t* bank, uint32_t* pages, uint8_t* main, SpareData* spare, int pagesCount);
int nand_read_alternate_ecc(int bank, int page, uint8_t* buffer);
int nand_erase(int bank, int block);
int nand_write(int bank, int page, uint8_t* buffer, uint8_t* spare, int doECC);
int nand_read_status();
int nand_calculate_ecc(uint8_t* data, uint8_t* ecc);
NANDData* nand_get_geometry();
NANDFTLData* nand_get_ftl_data();

#endif
