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
	PCLK = 0,
	NCLK = 1
} SPIClockSource;

typedef enum SPIOption13 {
	SPIOption13Setting0 = 8,
	SPIOption13Setting1 = 16,
	SPIOption13Setting2 = 32
} SPIOption13;

typedef struct SPIInfo {
	int option13;
	int isActiveLow;
	int lastClockEdgeMissing;
	SPIClockSource clockSource;
	int baud;
	int isMaster;
	int option5;
	const volatile uint8_t* txBuffer;
	volatile int txCurrentLen;
	volatile int txTotalLen;
	volatile uint8_t* rxBuffer;
	volatile int rxCurrentLen;
	volatile int rxTotalLen;
	volatile int counter;
	volatile int txDone;
	volatile int rxDone;
} SPIInfo;

int spi_setup();
int spi_tx(int port, const uint8_t* buffer, int len, int block, int unknown);
int spi_rx(int port, uint8_t* buffer, int len, int block, int noTransmitJunk);
int spi_txrx(int port, const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen, int block);
void spi_set_baud(int port, int baud, SPIOption13 option13, int isMaster, int isActiveLow, int lastClockEdgeMissing);

#endif
