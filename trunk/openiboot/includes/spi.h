#ifndef SPI_H
#define SPI_H

#include "openiboot.h"

typedef struct SPIRegister {
	uint32_t control;
	uint32_t setup;
	uint32_t status;
	uint32_t unkReg1;
	uint32_t txData;
	uint32_t rxData;
	uint32_t clkDivider;
	uint32_t unkReg2;
	uint32_t unkReg3;
} SPIRegister;

typedef enum SPIClockSource {
	NCLK = 0,
	PCLK = 1
} SPIClockSource;

typedef enum SPIOption13 {
	SPIOption13Setting0 = 8,
	SPIOption13Setting1 = 16,
	SPIOption13Setting2 = 32
} SPIOption13;

typedef struct SPIInfo {
	int option13;
	int option2;
	int option1;
	SPIClockSource clockSource;
	int baud;
	int option3;
	int option5;
	uint8_t* txBuffer;
	int txCurrentLen;
	int txTotalLen;
	uint8_t* rxBuffer;
	int rxCurrentLen;
	int rxTotalLen;
	int counter;
	int txDone;
	int rxDone;
} SPIInfo;

int spi_setup();

#endif
