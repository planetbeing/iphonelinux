#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "multitouch.h"
#include "hardware/multitouch.h"
#include "gpio.h"
#include "timer.h"
#include "util.h"
#include "spi.h"
#include "syscfg.h"

static void multitouch_atn(uint32_t token);

volatile int GotATN;

static uint8_t* OutputPacket;
static uint8_t* InputPacket;
static uint8_t* GetInfoPacket;
static uint8_t* GetResultPacket;

static int InterfaceVersion;
static int MaxPacketSize;
static int FamilyID;
static int FlipNOP;
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

int mt_spi_txrx(const MTSPISetting* setting, const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen);
int mt_spi_tx(const MTSPISetting* setting, const uint8_t* outBuffer, int outLen);

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

static uint16_t performHBPPATN_ACK()
{
	uint8_t tx[2];
	uint8_t rx[2];

	uint64_t startTime = timer_get_system_microtime();
	while(TRUE)
	{
		EnterCriticalSection();
		if(GotATN > 0)
		{
			--GotATN;
			LeaveCriticalSection();
			break;
		}
		LeaveCriticalSection();
		if(has_elapsed(startTime, 1000)) {
			return 0xFFFF;
		}
	}

	tx[0] = 0x1A;
	tx[1] = 0xA1;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return (rx[0] << 8) | rx[1];
}

static uint32_t performHBPPLongATN_ACK()
{
	uint8_t tx[8];
	uint8_t rx[8];

	while(GotATN == 0);
	--GotATN;

	tx[0] = 0x1A;
	tx[1] = 0xA1;
	tx[2] = 0x18;
	tx[3] = 0xE1;
	tx[4] = 0x18;
	tx[5] = 0xE1;
	tx[6] = 0x18;
	tx[7] = 0xE1;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return (rx[2] << 8) | rx[3] | (rx[4] << 24) | (rx[5] << 16);
}

static int makeBootloaderDataPacket(uint8_t* output, uint32_t destAddress, const uint8_t* data, int dataLen, int* cksumOut)
{
	uint32_t checksum;
	int i;

	// This seems to be middle-endian! I've never seen this before.

	output[0] = 0x30;
	output[1] = 0x01;
	output[2] = ((dataLen >> 2) >> 10) & 0xFF;
	output[3] = (dataLen >> 2) & 0xFF;
	output[4] = (destAddress >> 8) & 0xFF;
	output[5] = destAddress & 0xFF;
	output[6] = (destAddress >> 24) & 0xFF;
	output[7] = (destAddress >> 16) & 0xFF;

	checksum = 0;

	for(i = 2; i < 8; ++i)
		checksum += output[i];

	output[8] = (checksum >> 8) & 0xFF;
	output[9] = checksum & 0xFF;

	for(i = 0; i < dataLen; i += 4)
	{
		output[10 + i + 0] = data[i + 1];
		output[10 + i + 1] = data[i + 0];
		output[10 + i + 2] = data[i + 3];
		output[10 + i + 3] = data[i + 2];
	}

	checksum = 0;
	for(i = 10; i < (dataLen + 10); ++i)
		checksum += output[i];

	output[dataLen + 10] = (checksum >> 8) & 0xFF;
	output[dataLen + 11] = checksum & 0xFF;
	output[dataLen + 12] = (checksum >> 24) & 0xFF;
	output[dataLen + 13] = (checksum >> 16) & 0xFF;

	if(cksumOut)
		*cksumOut = checksum;

	return dataLen;
}

static int loadConstructedFirmware(const uint8_t* firmware, int len)
{
	int try;

	for(try = 0; try < 5; ++try)
	{
		bufferPrintf("multitouch: uploading data packet\r\n");

		GotATN = 0;
		mt_spi_tx(FAST_SPEED, firmware, len);

		if(performHBPPATN_ACK() == 0x4BC1)
			return TRUE;
	}

	return FALSE;
}

#ifndef CONFIG_IPOD
static int loadProxCal(const uint8_t* firmware, int len)
{
	uint32_t address = 0x400180;
	const uint8_t* data = firmware;
	int left = (len + 3) & ~0x3;
	int try;

	while(left > 0)
	{
		int toUpload = left;
		if(toUpload > 0x3F0)
			toUpload = 0x3F0;

		OutputPacket[0] = 0x18;
		OutputPacket[1] = 0xE1;

		makeBootloaderDataPacket(OutputPacket + 2, address, data, toUpload, NULL);

		for(try = 0; try < 5; ++try)
		{
			bufferPrintf("multitouch: uploading prox calibration data packet\r\n");

			GotATN = 0;
			mt_spi_tx(FAST_SPEED, OutputPacket, toUpload + 0x10);

			if(performHBPPATN_ACK() == 0x4BC1)
				break;
		}

		if(try == 5)
			return FALSE;

		address += toUpload;
		data += toUpload;
		left -= toUpload;
	}

	return TRUE;
}
#endif

static int loadCal(const uint8_t* firmware, int len)
{
	uint32_t address = 0x400200;
	const uint8_t* data = firmware;
	int left = (len + 3) & ~0x3;
	int try;

	while(left > 0)
	{
		int toUpload = left;
		if(toUpload > 0x3F0)
			toUpload = 0x3F0;

		OutputPacket[0] = 0x18;
		OutputPacket[1] = 0xE1;

		makeBootloaderDataPacket(OutputPacket + 2, address, data, toUpload, NULL);

		for(try = 0; try < 5; ++try)
		{
			bufferPrintf("multitouch: uploading calibration data packet\r\n");

			GotATN = 0;
			mt_spi_tx(FAST_SPEED, OutputPacket, toUpload + 0x10);

			if(performHBPPATN_ACK() == 0x4BC1)
				break;
		}

		if(try == 5)
			return FALSE;

		address += toUpload;
		data += toUpload;
		left -= toUpload;
	}

	return TRUE;
}

static uint32_t readRegister(uint32_t address)
{
	uint8_t tx[8];
	uint8_t rx[8];
	uint32_t checksum;
	int i;

	tx[0] = 0x1C;
	tx[1] = 0x73;
	tx[2] = (address >> 8) & 0xFF;
	tx[3] = address & 0xFF;
	tx[4] = (address >> 24) & 0xFF;
	tx[5] = (address >> 16) & 0xFF;

	checksum = 0;
	for(i = 2; i < 6; ++i)
		checksum += tx[i];

	tx[6] = (checksum >> 8) & 0xFF;
	tx[7] = checksum & 0xFF;

	GotATN = 0;
	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return performHBPPLongATN_ACK();
}

static uint32_t writeRegister(uint32_t address, uint32_t value, uint32_t mask)
{
	uint8_t tx[16];
	uint8_t rx[16];
	uint32_t checksum;
	int i;

	tx[0] = 0x1E;
	tx[1] = 0x33;
	tx[2] = (address >> 8) & 0xFF;
	tx[3] = address & 0xFF;
	tx[4] = (address >> 24) & 0xFF;
	tx[5] = (address >> 16) & 0xFF;
	tx[6] = (mask >> 8) & 0xFF;
	tx[7] = mask & 0xFF;
	tx[8] = (mask >> 24) & 0xFF;
	tx[9] = (mask >> 16) & 0xFF;
	tx[10] = (value >> 8) & 0xFF;
	tx[11] = value & 0xFF;
	tx[12] = (value >> 24) & 0xFF;
	tx[13] = (value >> 16) & 0xFF;

	checksum = 0;
	for(i = 2; i < 14; ++i)
		checksum += tx[i];

	tx[14] = (checksum >> 8) & 0xFF;
	tx[15] = checksum & 0xFF;

	GotATN = 0;
	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	if(performHBPPATN_ACK() == 0x4AD1)
		return TRUE;
	else
		return FALSE;
}

static int calibrate()
{
	uint32_t z2_version;
	uint8_t tx[2];
	uint8_t rx[2];

	z2_version = readRegister(0x10008FFC);

	bufferPrintf("multitouch: detected Zephyr2 version 0x%0X\r\n", z2_version);

	if(z2_version != 0x5A020028)
	{
		// Apparently we have to write registers here

		if(!writeRegister(0x10001C04, 0x16E4, 0x1FFF))
		{
			bufferPrintf("multitouch: error writing to register 0x10001C04\r\n");
			return FALSE;
		}

		if(!writeRegister(0x10001C08, 0x840000, 0xFF0000))
		{
			bufferPrintf("multitouch: error writing to register 0x10001C08\r\n");
			return FALSE;
		}

		if(!writeRegister(0x1000300C, 0x05, 0x85))
		{
			bufferPrintf("multitouch: error writing to register 0x1000300C\r\n");
			return FALSE;
		}

		if(!writeRegister(0x1000304C, 0x20, 0xFFFFFFFF))
		{
			bufferPrintf("multitouch: error writing to register 0x1000304C\r\n");
			return FALSE;
		}

		bufferPrintf("multitouch: initialized registers\r\n");
	}

	tx[0] = 0x1F;
	tx[1] = 0x01;

	bufferPrintf("multitouch: requesting calibration...\r\n");

	GotATN = 0;
	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	udelay(65000);

	bufferPrintf("multitouch: calibration complete with 0x%x\r\n", performHBPPATN_ACK());

	return TRUE;
}

static void sendExecutePacket()
{
	uint8_t tx[12];
	uint8_t rx[12];
	uint32_t checksum;
	int i;

	tx[0] = 0x1D;
	tx[1] = 0x53;
	tx[2] = 0x18;
	tx[3] = 0x00;
	tx[4] = 0x10;
	tx[5] = 0x00;
	tx[6] = 0x00;
	tx[7] = 0x01;
	tx[8] = 0x00;
	tx[9] = 0x00;

	checksum = 0;
	for(i = 2; i < 10; ++i)
		checksum += tx[i];

	tx[10] = (checksum >> 8) & 0xFF;
	tx[11] = checksum & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	bufferPrintf("multitouch: execute packet sent\r\n");
}

static int determineInterfaceVersion()
{
	uint8_t tx[16];
	uint8_t rx[16];
	uint32_t checksum;
	int i;
	int try;

	memset(tx, 0, sizeof(tx));

	tx[0] = 0xE2;

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += tx[i];

	// Note that the byte order changes to little-endian after main firmware load

	tx[14] = checksum & 0xFF;
	tx[15] = (checksum >> 8) & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	for(try = 0; try < 4; ++try)
	{
		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] == 0xE2)
		{
			checksum = 0;
			for(i = 0; i < 14; ++i)
				checksum += rx[i];

			if((rx[14] | (rx[15] << 8)) == checksum)
			{
				InterfaceVersion = rx[2];
				MaxPacketSize = (rx[4] << 8) | rx[3];
				bufferPrintf("multitouch: interface version %d, max packet size: %d\r\n", InterfaceVersion, MaxPacketSize);

				return TRUE;
			}
		}

		InterfaceVersion = 0;
		MaxPacketSize = 1000;
		udelay(3000);
	}

	bufferPrintf("multitouch: failed getting interface version!\r\n");

	return FALSE;
}

static int getReportInfo(int id, uint8_t* err, uint16_t* len)
{
	uint8_t tx[16];
	uint8_t rx[16];
	uint32_t checksum;
	int i;
	int try;

	for(try = 0; try < 4; ++try)
	{
		memset(tx, 0, sizeof(tx));

		tx[0] = 0xE3;
		tx[1] = id;

		checksum = 0;
		for(i = 0; i < 14; ++i)
			checksum += tx[i];

		tx[14] = checksum & 0xFF;
		tx[15] = (checksum >> 8) & 0xFF;

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		udelay(25);

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] != 0xE3)
			continue;

		checksum = 0;
		for(i = 0; i < 14; ++i)
			checksum += rx[i];

		if((rx[14] | (rx[15] << 8)) != checksum)
			continue;

		*err = rx[2];
		*len = (rx[4] << 8) | rx[3];

		return TRUE;
	}

	return FALSE;
}

static int shortControlRead(int id, uint8_t* buffer, int size)
{
	uint32_t checksum;
	int i;

	memset(GetInfoPacket, 0, 0x200);
	GetInfoPacket[0] = 0xE6;
	GetInfoPacket[1] = id;
	GetInfoPacket[2] = 0;
	GetInfoPacket[3] = size & 0xFF;
	GetInfoPacket[4] = (size >> 8) & 0xFF;

	checksum = 0;
	for(i = 0; i < 5; ++i)
		checksum += GetInfoPacket[i];

	GetInfoPacket[14] = checksum & 0xFF;
	GetInfoPacket[15] = (checksum >> 8) & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, GetInfoPacket, 16, InputPacket, 16);

	udelay(25);

	GetInfoPacket[2] = 1;

	mt_spi_txrx(NORMAL_SPEED, GetInfoPacket, 16, InputPacket, 16);

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += InputPacket[i];

	if((InputPacket[14] | (InputPacket[15] << 8)) != checksum)
		return FALSE;

	memcpy(buffer, &InputPacket[3], size);

	return TRUE;
}

static int longControlRead(int id, uint8_t* buffer, int size)
{
	uint32_t checksum;
	int i;

	memset(GetInfoPacket, 0, 0x200);
	GetInfoPacket[0] = 0xE7;
	GetInfoPacket[1] = id;
	GetInfoPacket[2] = 0;
	GetInfoPacket[3] = size & 0xFF;
	GetInfoPacket[4] = (size >> 8) & 0xFF;

	checksum = 0;
	for(i = 0; i < 5; ++i)
		checksum += GetInfoPacket[i];

	GetInfoPacket[14] = checksum & 0xFF;
	GetInfoPacket[15] = (checksum >> 8) & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, GetInfoPacket, 16, InputPacket, 16);

	udelay(25);

	GetInfoPacket[2] = 1;
	GetInfoPacket[14] = 0;
	GetInfoPacket[15] = 0;
	GetInfoPacket[3 + size] = checksum & 0xFF;
	GetInfoPacket[3 + size + 1] = (checksum >> 8) & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, GetInfoPacket, size + 5, InputPacket, size + 5);

	checksum = 0;
	for(i = 0; i < (size + 3); ++i)
		checksum += InputPacket[i];

	if((InputPacket[3 + size] | (InputPacket[3 + size + 1] << 8)) != checksum)
		return FALSE;

	memcpy(buffer, &InputPacket[3], size);

	return TRUE;
}

static int getReport(int id, uint8_t* buffer, int* outLen)
{
	uint8_t err;
	uint16_t len;
	if(!getReportInfo(id, &err, &len))
		return FALSE;

	if(err)
		return FALSE;

	*outLen = len;

	int try = 0;
	for(try = 0; try < 4; ++try)
	{
		if(len < 12)
		{
			if(shortControlRead(id, buffer, len))
				return TRUE;
		} else
		{
			if(longControlRead(id, buffer, len))
				return TRUE;
		}
	}

	return FALSE;
}

static void multitouch_atn(uint32_t token)
{
	++GotATN;
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

static int readFrameLength(int* len)
{
	uint8_t tx[16];
	uint8_t rx[16];
	uint32_t checksum;
	int i;

	memset(tx, 0, sizeof(tx));

	if(FlipNOP)
		tx[0] = 0xEB;
	else
		tx[0] = 0xEA;

	tx[1] = CurNOP;
	tx[2] = 0;

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += tx[i];

	tx[14] = checksum & 0xFF;
	tx[15] = (checksum >> 8) & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += rx[i];

	if((rx[14] | (rx[15] << 8)) != checksum)
	{
		udelay(1000);
		return FALSE;
	}

	*len = (rx[1] & 0xFF) | ((rx[2] & 0xFF) << 8);

	return TRUE;
}

static int readResultData(int len)
{
	uint32_t checksum;
	int i;
	int packetLen;

	if(len > 0x200)
		len = 0x200;

	memset(GetResultPacket, 0, 0x200);

	if(FlipNOP)
		GetResultPacket[0] = 0xEB;
	else
		GetResultPacket[0] = 0xEA;

	GetResultPacket[1] = CurNOP;
	GetResultPacket[2] = 1;

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += GetResultPacket[i];

	GetResultPacket[len - 2] = checksum & 0xFF;
	GetResultPacket[len - 1] = (checksum >> 8) & 0xFF;

	mt_spi_txrx(NORMAL_SPEED, GetResultPacket, len, InputPacket, len);

	if(InputPacket[0] != 0xEA && !(FlipNOP && InputPacket[0] == 0xEB))
	{
		bufferPrintf("multitouch: frame header wrong: got 0x%02X\r\n", InputPacket[0]);
		udelay(1000);
		return FALSE;
	}

	checksum = 0;
	for(i = 0; i < 5; ++i)
		checksum += InputPacket[i];

	if((checksum & 0xFF) != 0)
	{
		bufferPrintf("multitouch: LSB of first five bytes of frame not zero: got 0x%02X\r\n", checksum);
		udelay(1000);
		return FALSE;
	}

	packetLen = (InputPacket[2] & 0xFF) | ((InputPacket[3] & 0xFF) << 8);

	if(packetLen <= 2)
		return TRUE;

	checksum = 0;
	for(i = 5; i < (5 + packetLen - 2); ++i)
		checksum += InputPacket[i];

	if((InputPacket[len - 2] | (InputPacket[len - 1] << 8)) != checksum)
	{
		bufferPrintf("multitouch: packet checksum wrong 0x%02X instead of 0x%02X\r\n", checksum, (InputPacket[len - 2] | (InputPacket[len - 1] << 8)));
		udelay(1000);
		return FALSE;
	}

	newPacket(InputPacket + 5, packetLen - 2);
	return TRUE;
}

static int readFrame()
{
	int ret = 0;
	int len = 0;

	if(!readFrameLength(&len))
	{
		bufferPrintf("multitouch: error getting frame length\r\n");
		len = 0;
		ret = -1;
	}

	if(len)
	{
		if(!readResultData(len + 5))
		{
			bufferPrintf("multitouch: error getting frame data\r\n");
			udelay(1000);
			ret = -1;
		}

		ret = 1;
	}

	if(FlipNOP)
	{
		if(CurNOP == 1)
			CurNOP = 2;
		else
			CurNOP = 1;
	}

	return ret;
}

int multitouch_setup(const uint8_t* constructedFirmware, int constructedFirmwareLen)
{
	int err;
	uint8_t* reportBuffer;
	int reportLen;
	int i;

#ifndef CONFIG_IPOD
	uint8_t* prox_cal;
	int prox_cal_size;
#endif

	uint8_t* cal;
	int cal_size;

#ifndef CONFIG_IPOD
	prox_cal = syscfg_get_entry(SCFG_PxCl, &prox_cal_size);
	if(!prox_cal)
	{
		bufferPrintf("multitouch: could not find proximity calibration data\r\n");
		return -1;
	}
#endif

	cal = syscfg_get_entry(SCFG_MtCl, &cal_size);
	if(!cal)
	{
		bufferPrintf("multitouch: could not find calibration data\r\n");
		return -1;
	}

	OutputPacket = (uint8_t*) malloc(0x400);
	InputPacket = (uint8_t*) malloc(0x400);
	GetInfoPacket = (uint8_t*) malloc(0x400);
	GetResultPacket = (uint8_t*) malloc(0x400);

	gpio_register_interrupt(MT_ATN_INTERRUPT, 0, 0, 0, multitouch_atn, 0);
	gpio_interrupt_enable(MT_ATN_INTERRUPT);

	multitouch_on();

	for(i = 0; i < 4; ++i)
	{
		gpio_pin_output(0x606, 0);
		udelay(200000);
		gpio_pin_output(0x606, 1);
		udelay(15000);

		while(performHBPPATN_ACK() != 0xFFFF);

		if(loadConstructedFirmware(constructedFirmware, constructedFirmwareLen))
			break;
	}

	if(i == 4)
	{
		bufferPrintf("multitouch: could not load preconstructed firmware\r\n");
		err = -1;
		goto out;
	}

	bufferPrintf("multitouch: loaded %d byte preconstructed firmware\r\n", constructedFirmwareLen);

#ifndef CONFIG_IPOD
	if(!loadProxCal(prox_cal, prox_cal_size))
	{
		bufferPrintf("multitouch: could not load proximity calibration data\r\n");
		err = -1;
		goto out;
	}

	bufferPrintf("multitouch: loaded %d byte proximity calibration data\r\n", prox_cal_size);
#endif

	if(!loadCal(cal, cal_size))
	{
		bufferPrintf("multitouch: could not load calibration data\r\n");
		err = -1;
		goto out;
	}

	bufferPrintf("multitouch: loaded %d byte calibration data\r\n", cal_size);

	if(!calibrate())
	{
		err = -1;
		goto out;
	}

	sendExecutePacket();

	udelay(1000);

	bufferPrintf("multitouch: Determining interface version...\r\n");
	if(!determineInterfaceVersion())
	{
		err = -1;
		goto out;
	}

	reportBuffer = (uint8_t*) malloc(MaxPacketSize);

	if(!getReport(MT_INFO_FAMILYID, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting family id!\r\n");
		err = -1;
		goto out_free_rb;
	}

	FamilyID = reportBuffer[0];

	if(!getReport(MT_INFO_SENSORINFO, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor info!\r\n");
		err = -1;
		goto out_free_rb;
	}

	SensorColumns = reportBuffer[2];
	SensorRows = reportBuffer[1];
	BCDVersion = ((reportBuffer[3] & 0xFF) << 8) | (reportBuffer[4] & 0xFF);
	Endianness = reportBuffer[0];

	if(!getReport(MT_INFO_SENSORREGIONDESC, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor region descriptor!\r\n");
		err = -1;
		goto out_free_rb;
	}

	SensorRegionDescriptorLen = reportLen;
	SensorRegionDescriptor = (uint8_t*) malloc(reportLen);
	memcpy(SensorRegionDescriptor, reportBuffer, reportLen);

	if(!getReport(MT_INFO_SENSORREGIONPARAM, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor region param!\r\n");
		err = -1;
		goto out_free_srd;
	}

	SensorRegionParamLen = reportLen;
	SensorRegionParam = (uint8_t*) malloc(reportLen);
	memcpy(SensorRegionParam, reportBuffer, reportLen);

	if(!getReport(MT_INFO_SENSORDIM, reportBuffer, &reportLen))
	{
		bufferPrintf("multitouch: failed getting sensor surface dimensions!\r\n");
		goto out_free_srp;
	}

	SensorWidth = *((uint32_t*)&reportBuffer[0]);
	SensorHeight = *((uint32_t*)&reportBuffer[4]);

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

	if(BCDVersion > 0x23)
		FlipNOP = TRUE;
	else
		FlipNOP = FALSE;

	free(reportBuffer);

	GotATN = 0;
	CurNOP = 1;

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

		readFrame();
	}

	return 0;

out_free_srp:
	free(SensorRegionParam);

out_free_srd:
	free(SensorRegionDescriptor);

out_free_rb:
	free(reportBuffer);

out:
	free(InputPacket);
	free(OutputPacket);
	free(GetInfoPacket);
	free(GetResultPacket);
	return err;
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
