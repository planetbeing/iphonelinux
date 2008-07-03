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

#define GPIO_SPI0_CS0 0x400
#define GPIO_SPI1_CS0 0x1800

int spi_setup();
int spi_tx(int port, uint8_t* buffer, int len, int block, int unknown);
int spi_rx(int port, uint8_t* buffer, int len, int block, int unknown);
void spi_set_baud(int port, int baud, SPIOption13 option13, int option3, int option2, int option1);

#endif
