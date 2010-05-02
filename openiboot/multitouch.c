#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "multitouch.h"
#include "hardware/multitouch.h"
#include "gpio.h"
#include "timer.h"
#include "util.h"
#include "spi.h"

static void multitouch_atn(uint32_t token);

volatile int GotATN;

static uint8_t* OutputPacket;
static uint8_t* InputPacket;
static uint8_t* GetInfoPacket;
static uint8_t* GetResultPacket;

static int InterfaceVersion;
static int MaxPacketSize;
static int FamilyID;
static int SensorWidth;
static int SensorHeight;
static int SensorColumns;
static int SensorRows;
static int BCDVersion;
static int Endianness;
static uint8_t* SensorRegionDescriptor;
static int SensorRegionDescriptorLen;
static uint8_t* SensorRegionParam;
static int SensorRegionParamLen;

// This is flipped between 0x64 and 0x65 for every transaction
static int CurNOP;

typedef struct MTSPISetting
{
	int speed;
	int txDelay;
	int rxDelay;
} MTSPISetting;

const MTSPISetting MTNormalSpeed = {83000, 5000, 10000};
const MTSPISetting MTFastSpeed= {4500000, 0, 10000};

#define NORMAL_SPEED (&MTNormalSpeed)
#define FAST_SPEED (&MTFastSpeed)

static int mt_spi_txrx(const MTSPISetting* setting, const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen);
static int mt_spi_tx(const MTSPISetting* setting, const uint8_t* outBuffer, int outLen);

static int makeBootloaderDataPacket(uint8_t* output, uint32_t destAddress, const uint8_t* data, int dataLen, int* cksumOut);
static int verifyUpload(int checksum);
static void sendExecutePacket();
static void sendBlankDataPacket();

static int loadASpeedFirmware(const uint8_t* firmware, int len);
static int loadMainFirmware(const uint8_t* firmware, int len);
static int determineInterfaceVersion();

static int getReportInfo(int id, uint8_t* err, uint16_t* len);
static int getReport(int id, uint8_t* buffer, int* outLen);

static int readFrameLength(int* len);
static int readFrame();
static int readResultData(int len);

static void newPacket(const uint8_t* data, int len);

int MultitouchOn = FALSE;

void multitouch_on()
{
	if(!MultitouchOn)
	{
		bufferPrintf("multitouch: powering on\r\n");
		gpio_pin_output(MT_GPIO_POWER, 0);
		udelay(200000);
		gpio_pin_output(MT_GPIO_POWER, 1);

		udelay(15000);
		MultitouchOn = TRUE;
	}
}

int multitouch_setup(const uint8_t* ASpeedFirmware, int ASpeedFirmwareLen, const uint8_t* mainFirmware, int mainFirmwareLen)
{
	bufferPrintf("multitouch: A-Speed firmware at 0x%08x - 0x%08x, Main firmware at 0x%08x - 0x%08x\r\n",
			(uint32_t) ASpeedFirmware, (uint32_t)(ASpeedFirmware + ASpeedFirmwareLen),
			(uint32_t) mainFirmware, (uint32_t)(mainFirmware + mainFirmwareLen));

	OutputPacket = (uint8_t*) malloc(0x400);
	InputPacket = (uint8_t*) malloc(0x400);
	GetInfoPacket = (uint8_t*) malloc(0x400);
	GetResultPacket = (uint8_t*) malloc(0x400);

	memset(GetInfoPacket, 0x82, 0x400);
	memset(GetResultPacket, 0x68, 0x400);

	gpio_register_interrupt(MT_ATN_INTERRUPT, 0, 0, 0, multitouch_atn, 0);
	gpio_interrupt_enable(MT_ATN_INTERRUPT);

	multitouch_on();

	bufferPrintf("multitouch: Sending A-Speed firmware...\r\n");
	if(!loadASpeedFirmware(ASpeedFirmware, ASpeedFirmwareLen))
	{
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		return -1;
	}

	udelay(1000);

	bufferPrintf("multitouch: Sending main firmware...\r\n");
	if(!loadMainFirmware(mainFirmware, mainFirmwareLen))
	{
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		return -1;
	}

	udelay(1000);

	bufferPrintf("multitouch: Determining interface version...\r\n");
	if(!determineInterfaceVersion())
	{
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		return -1;
	}

	uint8_t reportBuffer[MaxPacketSize];
	int reportLen;

	if(!getReport(MT_INFO_FAMILYID, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting family id!\r\n");
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		return -1;
	}

	FamilyID = reportBuffer[0];

	if(!getReport(MT_INFO_SENSORINFO, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor info!\r\n");
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		return -1;
	}

	SensorColumns = reportBuffer[2];
	SensorRows = reportBuffer[1];
	BCDVersion = ((reportBuffer[3] & 0xFF) << 8) | (reportBuffer[4] & 0xFF);
	Endianness = reportBuffer[0];

	if(!getReport(MT_INFO_SENSORREGIONDESC, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor region descriptor!\r\n");
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		return -1;
	}

	SensorRegionDescriptorLen = reportLen;
	SensorRegionDescriptor = (uint8_t*) malloc(reportLen);
	memcpy(SensorRegionDescriptor, reportBuffer, reportLen);

	if(!getReport(MT_INFO_SENSORREGIONPARAM, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor region param!\r\n");
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		free(SensorRegionDescriptor);
		return -1;
	}

	SensorRegionParamLen = reportLen;
	SensorRegionParam = (uint8_t*) malloc(reportLen);
	memcpy(SensorRegionParam, reportBuffer, reportLen);

	if(!getReport(MT_INFO_SENSORDIM, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor surface dimensions!\r\n");
		free(InputPacket);
		free(OutputPacket);
		free(GetInfoPacket);
		free(GetResultPacket);
		free(SensorRegionDescriptor);
		free(SensorRegionParam);
		return -1;
	}

	SensorWidth = reportBuffer[0] | (reportBuffer[1] >> 8) | (reportBuffer[2] >> 16) | (reportBuffer[3] >> 24);
	SensorHeight = reportBuffer[4] | (reportBuffer[5] >> 8) | (reportBuffer[6] >> 16) | (reportBuffer[7] >> 24);

	int i;

	bufferPrintf("Family ID                : 0x%x\r\n", FamilyID);
	bufferPrintf("Sensor rows              : 0x%x\r\n", SensorRows);
	bufferPrintf("Sensor columns           : 0x%x\r\n", SensorColumns);
	bufferPrintf("Sensor width             : 0x%x\r\n", SensorWidth);
	bufferPrintf("Sensor height            : 0x%x\r\n", SensorHeight);
	bufferPrintf("BCD Version              : 0x%x\r\n", BCDVersion);
	bufferPrintf("Endianness               : 0x%x\r\n", Endianness);
	bufferPrintf("Sensor region descriptor :");
	for(i = 0; i < SensorRegionDescriptorLen; ++i)
		bufferPrintf(" %02x", SensorRegionDescriptor[i]);
	bufferPrintf("\r\n");

	bufferPrintf("Sensor region param      :");
	for(i = 0; i < SensorRegionParamLen; ++i)
		bufferPrintf(" %02x", SensorRegionParam[i]);
	bufferPrintf("\r\n");

	CurNOP = 0x64;

	GotATN = 0;

	while(TRUE)
	{
		EnterCriticalSection();
		if(!GotATN)
		{
			LeaveCriticalSection();
			continue;
		}
		--GotATN;
		LeaveCriticalSection();

		while(readFrame() == 1);
	}

	return 0;
}

static void newPacket(const uint8_t* data, int len)
{
	MTFrameHeader* header = (MTFrameHeader*) data;
	if(header->type != 0x44 && header->type != 0x43)
		bufferPrintf("multitouch: unknown frame type 0x%x\r\n", header->type);

	FingerData* finger = (FingerData*)(data + (header->headerLen));

	if(header->headerLen < 12)
		bufferPrintf("multitouch: no finger data in frame\r\n");

	bufferPrintf("------START------\r\n");

	int i;
	for(i = 0; i < header->numFingers; ++i)
	{
		bufferPrintf("multitouch: finger %d -- id=%d, event=%d, X(%d/%d, vel: %d), Y(%d/%d, vel: %d), radii(%d, %d, %d, angle: %d), contactDensity: %d\r\n",
				i, finger->id, finger->event,
				finger->x, SensorWidth, finger->velX,
				finger->y, SensorHeight, finger->velY,
				finger->radius1, finger->radius2, finger->radius3, finger->angle,
				finger->contactDensity);

		//framebuffer_draw_rect(0xFF0000, (finger->x * framebuffer_width()) / SensorWidth - 2 , ((SensorHeight - finger->y) * framebuffer_height()) / SensorHeight - 2, 4, 4);
		//hexdump((uint32_t) finger, sizeof(FingerData));
		finger = (FingerData*) (((uint8_t*) finger) + header->fingerDataLen);
	}

	bufferPrintf("-------END-------\r\n");
}

static int readFrame()
{
	int try = 0;

	for(try = 0; try < 4; ++try)
	{
		int len = 0;
		if(!readFrameLength(&len))
		{
			bufferPrintf("multitouch: error getting frame length\r\n");
			udelay(1000);
			continue;
		}

		if(len)
		{
			if(!readResultData(len + 1))
			{
				bufferPrintf("multitouch: error getting frame data\r\n");
				udelay(1000);
				continue;
			}

			if(CurNOP == 0x64)
				CurNOP = 0x65;
			else
				CurNOP = 0x64;

			return 1;
		}

		return 0;
	}

	return -1;
}

static int readResultData(int len)
{
	int try = 0;
	for(try = 0; try < 4; ++try)
	{
		mt_spi_txrx(NORMAL_SPEED, GetResultPacket, len, InputPacket, len);
		
		if(InputPacket[0] != 0xAA)
		{
			udelay(1000);
			continue;
		}

		int checksum = ((InputPacket[len - 2] & 0xFF) << 8) | (InputPacket[len - 1] & 0xFF);
		int myChecksum = 0;

		int i;
		for(i = 1; i < (len - 2); ++i)
			myChecksum += InputPacket[i];

		myChecksum &= 0xFFFF;

		if(myChecksum != checksum)
		{
			udelay(1000);
			continue;
		}

		newPacket(InputPacket + 1, len - 3);
		return TRUE;
	}

	return FALSE;

}

static int readFrameLength(int* len)
{
	uint8_t tx[8];
	uint8_t rx[8];
	memset(tx, CurNOP, sizeof(tx));

	int try = 0;
	for(try = 0; try < 4; ++try)
	{
		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] != 0xAA)
		{
			udelay(1000);
			continue;
		}
		
		int tLen = ((rx[4] & 0xFF) << 8) | (rx[5] & 0xFF);
		int tLenCkSum = (rx[4] + rx[5]) & 0xFFFF;
		int checksum = ((rx[6] & 0xFF) << 8) | (rx[7] & 0xFF);
		if(tLenCkSum != checksum)
		{
			udelay(1000);
			continue;
		}

		if(tLen > MaxPacketSize)
		{
			bufferPrintf("multitouch: device unexpectedly requested to transfer a %d byte packet. Max size = %d\r\n", tLen, MaxPacketSize);
			udelay(1000);
			continue;
		}

		*len = tLen;

		return TRUE;
	}

	return FALSE;
}

static int getReport(int id, uint8_t* buffer, int* outLen)
{
	uint8_t err;
	uint16_t len;
	if(!getReportInfo(id, &err, &len))
		return FALSE;

	if(err)
		return FALSE;

	int try = 0;
	for(try = 0; try < 4; ++try)
	{
		GetInfoPacket[1] = id;
		mt_spi_txrx(NORMAL_SPEED, GetInfoPacket, len + 6, InputPacket, len + 6);

		if(InputPacket[0] != 0xAA)
		{
			udelay(1000);
			continue;
		}

		int checksum = ((InputPacket[len + 4] & 0xFF) << 8) | (InputPacket[len + 5] & 0xFF);
		int myChecksum = id;

		int i;
		for(i = 0; i < len; ++i)
			myChecksum += InputPacket[i + 4];

		myChecksum &= 0xFFFF;

		if(myChecksum != checksum)
		{
			udelay(1000);
			continue;
		}

		*outLen = len;
		memcpy(buffer, &InputPacket[4], len);

		return TRUE;
	}

	return FALSE;
}

static int getReportInfo(int id, uint8_t* err, uint16_t* len)
{
	uint8_t tx[8];
	uint8_t rx[8];

	int try;
	for(try = 0; try < 4; ++try)
	{
		memset(tx, 0x8F, 8);
		tx[1] = id;

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] != 0xAA)
		{
			udelay(1000);
			continue;
		}

		int checksum = ((rx[6] & 0xFF) << 8) | (rx[7] & 0xFF);
		int myChecksum = (id + rx[4] + rx[5]) & 0xFFFF;

		if(checksum != myChecksum)
		{
			udelay(1000);
			continue;
		}

		*err = (rx[4] >> 4) & 0xF;
		*len = ((rx[4] & 0xF) << 8) | (rx[5] & 0xFF);

		return TRUE;
	}

	return FALSE;
}

static int determineInterfaceVersion()
{
	uint8_t tx[4];
	uint8_t rx[4];

	int try;
	for(try = 0; try < 4; ++try)
	{
		memset(tx, 0xD0, 4);

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] == 0xAA)
		{
			InterfaceVersion = rx[1];
			MaxPacketSize = (rx[2] << 8) | rx[3];

			bufferPrintf("multitouch: interface version %d, max packet size: %d\r\n", InterfaceVersion, MaxPacketSize);
			return TRUE;
		}

		InterfaceVersion = 0;
		MaxPacketSize = 1000;
		udelay(3000);
	}

	bufferPrintf("multitouch: failed getting interface version!\r\n");

	return FALSE;
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
			mt_spi_tx(NORMAL_SPEED, OutputPacket, 0x400);

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

	for(i = 0; i < 5; ++i)
	{
		sendBlankDataPacket();

		bufferPrintf("multitouch: uploading main firmware\r\n");
		mt_spi_tx(FAST_SPEED, firmware, len);

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

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

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

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

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

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

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
	++GotATN;
}


int mt_spi_tx(const MTSPISetting* setting, const uint8_t* outBuffer, int outLen)
{
	spi_set_baud(MT_SPI, setting->speed, SPIOption13Setting0, 1, 1, 1);
	gpio_pin_output(MT_SPI_CS, 0);
	udelay(setting->txDelay);
	int ret = spi_tx(MT_SPI, outBuffer, outLen, TRUE, 0);
	gpio_pin_output(MT_SPI_CS, 1);
	return ret;
}

int mt_spi_txrx(const MTSPISetting* setting, const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen)
{
	spi_set_baud(MT_SPI, setting->speed, SPIOption13Setting0, 1, 1, 1);
	gpio_pin_output(MT_SPI_CS, 0);
	udelay(setting->rxDelay);
	int ret = spi_txrx(MT_SPI, outBuffer, outLen, inBuffer, inLen, TRUE);
	gpio_pin_output(MT_SPI_CS, 1);
	return ret;
}
