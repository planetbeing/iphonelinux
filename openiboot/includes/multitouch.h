#ifndef MULTITOUCH_H
#define MULTITOUCH_H

#include "openiboot.h"

typedef struct MTFrameHeader
{
	uint8_t type;
	uint8_t frameNum;
	uint8_t headerLen;
	uint8_t unk_3;
	uint32_t timestamp;
	uint8_t unk_8;
	uint8_t unk_9;
	uint8_t unk_A;
	uint8_t unk_B;
	uint16_t unk_C;
	uint16_t isImage;

	uint8_t numFingers;
	uint8_t fingerDataLen;
	uint16_t unk_12;
	uint16_t unk_14;
	uint16_t unk_16;
} MTFrameHeader;

typedef struct FingerData
{
	uint8_t id;
	uint8_t event;
	uint8_t unk_2;
	uint8_t unk_3;
	int16_t x;
	int16_t y;
	int16_t velX;
	int16_t velY;
	uint16_t radius2;
	uint16_t radius3;
	uint16_t angle;
	uint16_t radius1;
	uint16_t contactDensity;
	uint16_t unk_16;
	uint16_t unk_18;
	uint16_t unk_1A;
} FingerData;

#ifdef CONFIG_IPHONE
int multitouch_setup(const uint8_t* ASpeedFirmware, int ASpeedFirmwareLen, const uint8_t* mainFirmware, int mainFirmwareLen);
#else
int multitouch_setup(const uint8_t* constructedFirmware, int constructedFirmwareLen);
#endif

void multitouch_on();

#endif
