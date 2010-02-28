#include "openiboot.h"
#include "multitouch.h"
#include "hardware/multitouch.h"
#include "gpio.h"
#include "timer.h"
#include "util.h"
#include "spi.h"

static void multitouch_atn(uint32_t token);

volatile int GotATN;

static uint8_t* OutputPacket;

static int makeBootloaderDataPacket(uint8_t* output, uint32_t destAddress, const uint8_t* data, int dataLen, int* cksumOut);
static int verifyUpload(int checksum);
static void sendExecutePacket();
static void sendBlankDataPacket();

static int loadASpeedFirmware(const uint8_t* firmware, int len);
static int loadMainFirmware(const uint8_t* firmware, int len);

int multitouch_setup(const uint8_t* ASpeedFirmware, int ASpeedFirmwareLen, const uint8_t* mainFirmware, int mainFirmwareLen)
{
	bufferPrintf("multitouch: A-Speed firmware at 0x%08x - 0x%08x, Main firmware at 0x%08x - 0x%08x\r\n",
			(uint32_t) ASpeedFirmware, (uint32_t)(ASpeedFirmware + ASpeedFirmwareLen),
			(uint32_t) mainFirmware, (uint32_t)(mainFirmware + mainFirmwareLen));

	OutputPacket = (uint8_t*) malloc(0x400);

	gpio_register_interrupt(MT_ATN_INTERRUPT, 0, 0, 0, multitouch_atn, 0);
	gpio_interrupt_enable(MT_ATN_INTERRUPT);

	spi_set_baud(MT_SPI, MT_NORMAL_SPEED, SPIOption13Setting0, 1, 1, 1);

	gpio_pin_output(MT_GPIO_POWER, 0);
	udelay(200000);
	gpio_pin_output(MT_GPIO_POWER, 1);

	udelay(15000);

	bufferPrintf("multitouch: Sending A-Speed firmware...\r\n");
	loadASpeedFirmware(ASpeedFirmware, ASpeedFirmwareLen);

	udelay(1000);

	bufferPrintf("multitouch: Sending main firmware...\r\n");
	loadMainFirmware(mainFirmware, mainFirmwareLen);

	udelay(1000);

	return 0;
}

static int loadASpeedFirmware(const uint8_t* firmware, int len)
{
	uint32_t address = 0x40000000;
	const uint8_t* data = firmware;
	int left = len;

	while(left > 0)
	{
		int checksum;
		int toUpload = left;
		if(toUpload > 0x3F8)
			toUpload = 0x3F8;

		makeBootloaderDataPacket(OutputPacket, address, data, toUpload, &checksum);

		int try;
		for(try = 0; try < 5; ++try)
		{
			bufferPrintf("multitouch: uploading data packet\r\n");
			gpio_pin_output(MT_SPI_CS, 0);
			spi_tx(MT_SPI, OutputPacket, 0x400, TRUE, 0);
			gpio_pin_output(MT_SPI_CS, 1);

			udelay(300);

			if(verifyUpload(checksum))
				break;
		}

		if(try == 5)
			return FALSE;

		address += toUpload;
		data += toUpload;
		left -= toUpload;
	}

	sendExecutePacket();

	return TRUE;
}

static int loadMainFirmware(const uint8_t* firmware, int len)
{
	int checksum = 0;

	int i;
	for(i = 0; i < len; ++i)
		checksum += firmware[i];

	sendBlankDataPacket();

	for(i = 0; i < 5; ++i)
	{
		bufferPrintf("multitouch: uploading main firmware\r\n");
		spi_set_baud(MT_SPI, MT_ASPEED, SPIOption13Setting0, 1, 1, 1);
		gpio_pin_output(MT_SPI_CS, 0);
		spi_tx(MT_SPI, firmware, len, TRUE, 0);
		gpio_pin_output(MT_SPI_CS, 1);
		spi_set_baud(MT_SPI, MT_NORMAL_SPEED, SPIOption13Setting0, 1, 1, 1);

		if(verifyUpload(checksum))
			break;
	}

	if(i == 5)
		return FALSE;

	sendExecutePacket();

	return TRUE;
}

static int verifyUpload(int checksum)
{
	uint8_t tx[4];
	uint8_t rx[4];

	tx[0] = 5;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 6;

	memset(rx, 0, sizeof(rx));

	gpio_pin_output(MT_SPI_CS, 0);
	spi_txrx(MT_SPI, tx, sizeof(tx), rx, sizeof(rx), TRUE);
	gpio_pin_output(MT_SPI_CS, 1);

	if(rx[0] != 0xD0 || rx[1] != 0x0)
	{
		bufferPrintf("multitouch: data verification failed type bytes, got %02x %02x %02x %02x -- %x\r\n", rx[0], rx[1], rx[2], rx[3], checksum);
		return FALSE;
	}

	if(rx[2] != ((checksum >> 8) & 0xFF))
	{
		bufferPrintf("multitouch: data verification failed upper checksum, %02x != %02x\r\n", rx[2], (checksum >> 8) & 0xFF);
		return FALSE;
	}

	if(rx[3] != (checksum & 0xFF))
	{
		bufferPrintf("multitouch: data verification failed lower checksum, %02x != %02x\r\n", rx[3], checksum & 0xFF);
		return FALSE;
	}

	bufferPrintf("multitouch: data verification successful\r\n");
	return TRUE;
}


static void sendExecutePacket()
{
	uint8_t tx[4];
	uint8_t rx[4];

	tx[0] = 0xC4;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 0xC4;

	gpio_pin_output(MT_SPI_CS, 0);
	spi_txrx(MT_SPI, tx, sizeof(tx), rx, sizeof(rx), TRUE);
	gpio_pin_output(MT_SPI_CS, 1);

	bufferPrintf("multitouch: execute packet sent\r\n");
}

static void sendBlankDataPacket()
{
	uint8_t tx[4];
	uint8_t rx[4];

	tx[0] = 0xC2;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 0;

	gpio_pin_output(MT_SPI_CS, 0);
	spi_txrx(MT_SPI, tx, sizeof(tx), rx, sizeof(rx), TRUE);
	gpio_pin_output(MT_SPI_CS, 1);

	bufferPrintf("multitouch: blank data packet sent\r\n");
}

static int makeBootloaderDataPacket(uint8_t* output, uint32_t destAddress, const uint8_t* data, int dataLen, int* cksumOut)
{
	if(dataLen > 0x3F8)
		dataLen = 0x3F8;

	output[0] = 0xC2;
	output[1] = (destAddress >> 24) & 0xFF;
	output[2] = (destAddress >> 16) & 0xFF;
	output[3] = (destAddress >> 8) & 0xFF;
	output[4] = destAddress & 0xFF;
	output[5] = 0;

	int checksum = 0;

	int i;
	for(i = 0; i < dataLen; ++i)
	{
		uint8_t byte = data[i];
		checksum += byte;
		output[6 + i] = byte;
	}

	for(i = 0; i < 6; ++i)
	{
		checksum += output[i];
	}

	memset(output + dataLen + 6, 0, 0x3F8 - dataLen);
	output[0x3FE] = (checksum >> 8) & 0xFF;
	output[0x3FF] = checksum & 0xFF;

	*cksumOut = checksum;

	return dataLen;
}

static void multitouch_atn(uint32_t token)
{
	bufferPrintf("Actual ATN!\r\n");
	GotATN = 1;
}

