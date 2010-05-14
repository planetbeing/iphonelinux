#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "spi.h"
#include "hardware/spi.h"
#include "util.h"
#include "clock.h"
#include "chipid.h"
#include "timer.h"
#include "interrupt.h"

static const SPIRegister SPIRegs[NUM_SPIPORTS] = {
	{SPI0 + CONTROL, SPI0 + SETUP, SPI0 + STATUS, SPI0 + UNKREG1, SPI0 + TXDATA, SPI0 + RXDATA, SPI0 + CLKDIVIDER, SPI0 + UNKREG2, SPI0 + UNKREG3},
	{SPI1 + CONTROL, SPI1 + SETUP, SPI1 + STATUS, SPI1 + UNKREG1, SPI1 + TXDATA, SPI1 + RXDATA, SPI1 + CLKDIVIDER, SPI1 + UNKREG2, SPI1 + UNKREG3},
	{SPI2 + CONTROL, SPI2 + SETUP, SPI2 + STATUS, SPI2 + UNKREG1, SPI2 + TXDATA, SPI2 + RXDATA, SPI2 + CLKDIVIDER, SPI2 + UNKREG2, SPI2 + UNKREG3}
};

static SPIInfo spi_info[NUM_SPIPORTS];

static void spiIRQHandler(uint32_t port);

int spi_setup() {
	clock_gate_switch(SPI0_CLOCKGATE, ON);
	clock_gate_switch(SPI1_CLOCKGATE, ON);
	clock_gate_switch(SPI2_CLOCKGATE, ON);

	memset(spi_info, 0, sizeof(SPIInfo) * NUM_SPIPORTS);

	int i;
	for(i = 0; i < NUM_SPIPORTS; i++) {
		spi_info[i].clockSource = NCLK;
		SET_REG(SPIRegs[i].control, 0);
	}

	interrupt_install(SPI0_IRQ, spiIRQHandler, 0);
	interrupt_install(SPI1_IRQ, spiIRQHandler, 1);
	interrupt_install(SPI2_IRQ, spiIRQHandler, 2);
	interrupt_enable(SPI0_IRQ);
	interrupt_enable(SPI1_IRQ);
	interrupt_enable(SPI2_IRQ);

	return 0;
}

int spi_tx(int port, const uint8_t* buffer, int len, int block, int unknown) {
	if(port > (NUM_SPIPORTS - 1)) {
		return -1;
	}

	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 2));
	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 3));

	spi_info[port].txBuffer = buffer;

	if(len > MAX_TX_BUFFER)
		spi_info[port].txCurrentLen = MAX_TX_BUFFER;
	else
		spi_info[port].txCurrentLen = len;

	spi_info[port].txTotalLen = len;
	spi_info[port].txDone = FALSE;

	if(unknown == 0) {
		SET_REG(SPIRegs[port].unkReg2, 0);
	}

	int i;
	for(i = 0; i < spi_info[port].txCurrentLen; i++) {
		SET_REG(SPIRegs[port].txData, buffer[i]);
	}

	SET_REG(SPIRegs[port].control, 1);

	if(block) {
		while(!spi_info[port].txDone || GET_BITS(GET_REG(SPIRegs[port].status), 4, 4) != 0) {
			// yield
		}
		return len;
	} else {
		return 0;
	}
}

int spi_rx(int port, uint8_t* buffer, int len, int block, int noTransmitJunk) {
	if(port > (NUM_SPIPORTS - 1)) {
		return -1;
	}

	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 2));
	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 3));

	spi_info[port].rxBuffer = buffer;
	spi_info[port].rxDone = FALSE;
	spi_info[port].rxCurrentLen = 0;
	spi_info[port].rxTotalLen = len;
	spi_info[port].counter = 0;

	if(noTransmitJunk == 0) {
		SET_REG(SPIRegs[port].setup, GET_REG(SPIRegs[port].setup) | 1);
	}

	SET_REG(SPIRegs[port].unkReg2, len);
	SET_REG(SPIRegs[port].control, 1);

	if(block) {
		uint64_t startTime = timer_get_system_microtime();
		while(!spi_info[port].rxDone) {
			// yield
			if(has_elapsed(startTime, 1000)) {
				EnterCriticalSection();
				spi_info[port].rxDone = TRUE;
				spi_info[port].rxBuffer = NULL;
				LeaveCriticalSection();
				if(noTransmitJunk == 0) {
					SET_REG(SPIRegs[port].setup, GET_REG(SPIRegs[port].setup) & ~1);
				}
				return -1;
			}
		}
		if(noTransmitJunk == 0) {
			SET_REG(SPIRegs[port].setup, GET_REG(SPIRegs[port].setup) & ~1);
		}
		return len;
	} else {
		return 0;
	}
}

int spi_txrx(int port, const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen, int block)
{
	if(port > (NUM_SPIPORTS - 1)) {
		return -1;
	}

	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 2));
	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 3));

	spi_info[port].txBuffer = outBuffer;

	if(outLen > MAX_TX_BUFFER)
		spi_info[port].txCurrentLen = MAX_TX_BUFFER;
	else
		spi_info[port].txCurrentLen = outLen;

	spi_info[port].txTotalLen = outLen;
	spi_info[port].txDone = FALSE;

	spi_info[port].rxBuffer = inBuffer;
	spi_info[port].rxDone = FALSE;
	spi_info[port].rxCurrentLen = 0;
	spi_info[port].rxTotalLen = inLen;
	spi_info[port].counter = 0;

	int i;
	for(i = 0; i < spi_info[port].txCurrentLen; i++) {
		SET_REG(SPIRegs[port].txData, outBuffer[i]);
	}

	SET_REG(SPIRegs[port].unkReg2, inLen);
	SET_REG(SPIRegs[port].control, 1);

	if(block) {
		while(!spi_info[port].txDone || !spi_info[port].rxDone || GET_BITS(GET_REG(SPIRegs[port].status), 4, 4) != 0) {
			// yield
		}
		return inLen;
	} else {
		return 0;
	}
}

void spi_set_baud(int port, int baud, SPIOption13 option13, int isMaster, int isActiveLow, int lastClockEdgeMissing) {
	if(port > (NUM_SPIPORTS - 1)) {
		return;
	}

	SET_REG(SPIRegs[port].control, 0);

	switch(option13) {
		case SPIOption13Setting0:
			spi_info[port].option13 = 0;
			break;

		case SPIOption13Setting1:
			spi_info[port].option13 = 1;
			break;

		case SPIOption13Setting2:
			spi_info[port].option13 = 2;
			break;
	}

	spi_info[port].isActiveLow = isActiveLow;
	spi_info[port].lastClockEdgeMissing = lastClockEdgeMissing;

	uint32_t clockFrequency;

	if(spi_info[port].clockSource == PCLK) {
		clockFrequency = PeripheralFrequency;
	} else {
		clockFrequency = FixedFrequency;
	}

	uint32_t divider;

	if(chipid_spi_clocktype() != 0) {
		divider = clockFrequency / baud;
		if(divider < 2)
			divider = 2;
	} else {
		divider = clockFrequency / (baud * 2 - 1);
	}

	if(divider > MAX_DIVIDER) {
		return;
	}
	
	SET_REG(SPIRegs[port].clkDivider, divider);
	spi_info[port].baud = baud;
	spi_info[port].isMaster = isMaster;

	uint32_t options = (lastClockEdgeMissing << 1)
			| (isActiveLow << 2)
			| ((isMaster ? 0x3 : 0) << 3)
			| ((spi_info[port].option5 ? 0x2 : 0x3D) << 5)
			| (spi_info[port].clockSource << CLOCK_SHIFT)
			| spi_info[port].option13 << 13;

	SET_REG(SPIRegs[port].setup, options);
	SET_REG(SPIRegs[port].unkReg1, 0);
	SET_REG(SPIRegs[port].control, 1);

}

static void spiIRQHandler(uint32_t port) {
	if(port > (NUM_SPIPORTS - 1)) {
		return;
	}

	uint32_t status = GET_REG(SPIRegs[port].status);
	if(status & (1 << 3)) {
		spi_info[port].counter++;
	}

	int i;

	if(status & (1 << 1)) {
		while(TRUE) {
			// take care of tx
			if(spi_info[port].txBuffer != NULL) {
				if(spi_info[port].txCurrentLen < spi_info[port].txTotalLen) {
					int toTX = spi_info[port].txTotalLen - spi_info[port].txCurrentLen;
					int canTX = MAX_TX_BUFFER - TX_BUFFER_LEFT(status);

					if(toTX > canTX)
						toTX = canTX;

					for(i = 0; i < toTX; i++) {
						SET_REG(SPIRegs[port].txData, spi_info[port].txBuffer[spi_info[port].txCurrentLen + i]);
					}

					spi_info[port].txCurrentLen += toTX;

				} else {
					spi_info[port].txDone = TRUE;
					spi_info[port].txBuffer = NULL;
				}
			}

dorx:
			// take care of rx
			if(spi_info[port].rxBuffer == NULL)
				break;

			int toRX = spi_info[port].rxTotalLen - spi_info[port].rxCurrentLen;
			int canRX = GET_BITS(status, 8, 4);

			if(toRX > canRX)
				toRX = canRX;

			for(i = 0; i < toRX; i++) {
				spi_info[port].rxBuffer[spi_info[port].rxCurrentLen + i] = GET_REG(SPIRegs[port].rxData);
			}

			spi_info[port].rxCurrentLen += toRX;

			if(spi_info[port].rxCurrentLen < spi_info[port].rxTotalLen)
				break;

			spi_info[port].rxDone = TRUE;
			spi_info[port].rxBuffer = NULL;

		}


	} else  if(status & (1 << 0)) {
		// jump into middle of the loop to handle rx only, stupidly
		goto dorx;
	}

	// acknowledge interrupt handling complete
	SET_REG(SPIRegs[port].status, status);
}

