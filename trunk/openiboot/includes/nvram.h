#ifndef NVRAM_H
#define NVRAM_H

#include "openiboot.h"

#define NVRAM_START 0xFC000
#define NVRAM_SIZE 0x2000

typedef struct NVRamInfo {
	uint8_t ckByteSeed;
	uint8_t ckByte;
	uint16_t size;
	char type[12];
} __attribute__ ((packed)) NVRamInfo;

typedef struct NVRamData {
	uint32_t adler;
	uint32_t epoch;
} NVRamData;

typedef struct NVRamAtom {
	NVRamInfo* info;
	uint32_t size;
	struct NVRamAtom* next;
	uint8_t* data;
} NVRamAtom;

typedef struct EnvironmentVar {
	char* name;
	char* value;
	struct EnvironmentVar* next;
} EnvironmentVar;

int nvram_setup();
void nvram_listvars();
const char* nvram_getvar(const char* name);
void nvram_setvar(const char* name, const char* value);
void nvram_save();

#endif
