#include "openiboot.h"
#include "nvram.h"
#include "util.h"
#include "nor.h"

static uint8_t* bank1Data;
static uint8_t* bank2Data;

static NVRamAtom* bank1Atoms;
static NVRamAtom* bank2Atoms;
static uint32_t oldestBank;
static NVRamAtom* newestBank;
static uint8_t* newestBankData;

static EnvironmentVar* variables;

static uint8_t checkNVRamInfo(NVRamInfo* info) {
	uint32_t c = info->ckByteSeed;
	uint8_t* data = (uint8_t*) info;

	int i;
	for(i = 0x2; i < 0x10; i++) {
		c = (c + data[i]) & 0xffff;
	}

	while(c > 0xff) {
		c = (c >> 8) + (c & 0xff);
	}

	return c;
}

static void releaseAtoms(NVRamAtom* atoms) {
	NVRamAtom* toRelease;

	while(atoms != NULL) {
		toRelease = atoms;
		atoms = atoms->next;
		free(toRelease);
	}
}

static NVRamAtom* findAtom(NVRamAtom* atoms, const char* type) {
	while(atoms != NULL) {
		if(strcmp(atoms->info->type, type) == 0)
			return atoms;
		atoms = atoms->next;
	}

	return NULL;
}

static NVRamAtom* readAtoms(uint8_t* bankData) {
	NVRamAtom* lastAtom = NULL;
	NVRamAtom* firstAtom = NULL;
	size_t position = 0;
	while(position < NVRAM_SIZE) {
		if(lastAtom == NULL) {
			firstAtom = (NVRamAtom*) malloc(sizeof(NVRamAtom));
			lastAtom = firstAtom;
		} else {
			lastAtom->next = (NVRamAtom*) malloc(sizeof(NVRamAtom));
			lastAtom = lastAtom->next;
		}

		lastAtom->info = (NVRamInfo*) &bankData[position];
		lastAtom->size = lastAtom->info->size << 4;
		lastAtom->data = &bankData[position + sizeof(NVRamInfo)];
		lastAtom->next = NULL;

		position += lastAtom->size;

		if(checkNVRamInfo(lastAtom->info) != lastAtom->info->ckByte) {
			releaseAtoms(firstAtom);
			return NULL;
		}
	}

	NVRamAtom* ckDataAtom = findAtom(firstAtom, "nvram");
	if(ckDataAtom == NULL) {
		releaseAtoms(firstAtom);
		return NULL;
	}

	NVRamData* ckData = (NVRamData*) ckDataAtom->data;
	if(ckData->adler != adler32(bankData + 0x14, NVRAM_SIZE - 0x14)) {
		releaseAtoms(firstAtom);
		return NULL;	
	}

	return firstAtom;
}

static void releaseEnvironment(EnvironmentVar* vars) {
	EnvironmentVar* toRelease;

	while(vars != NULL) {
		toRelease = vars;
		vars = vars->next;
		free(toRelease);
	}
}

void nvram_save() {
	NVRamAtom* commonAtom = findAtom(newestBank, "common");
	EnvironmentVar* var = variables;
	memset(commonAtom->data, 0, commonAtom->size - sizeof(NVRamInfo));

	char* pos = (char*) commonAtom->data;
	while(var != NULL) {
		if(var->name[0] == '\0')
			break;

		sprintf(pos, "%s=%s", var->name, var->value);
		pos += strlen(pos) + 1;
		var = var->next;
	}

	NVRamAtom* ckDataAtom = findAtom(newestBank, "nvram");
	NVRamData* ckData = (NVRamData*) ckDataAtom->data;

	ckData->epoch++;
	ckData->adler = adler32(newestBankData + 0x14, NVRAM_SIZE - 0x14);

	NVRamAtom* atom = newestBank;
	uint32_t offset = oldestBank;
	while(atom != NULL) {
		nor_write(atom->info, offset, sizeof(NVRamInfo));
		offset += sizeof(NVRamInfo);
		nor_write(atom->data, offset, atom->size - sizeof(NVRamInfo));
		offset += atom->size - sizeof(NVRamInfo);
		atom = atom->next;
	}

	releaseAtoms(bank1Atoms);
	releaseAtoms(bank2Atoms);
	releaseEnvironment(variables);

	free(bank1Data);
	free(bank2Data);

	nvram_setup();
}

const char* nvram_getvar(const char* name) {
	EnvironmentVar* var = variables;
	while(var != NULL) {
		if(strcmp(var->name, name) == 0) {
			return var->value;
		}
		var = var->next;
	}
	return NULL;
}

void nvram_setvar(const char* name, const char* value) {
	EnvironmentVar* var = variables;
	while(var != NULL) {
		if(strcmp(var->name, name) == 0) {
			free(var->value);
			var->value = strdup(value);
			return;
		}
		var = var->next;
	}

	var = (EnvironmentVar*) malloc(sizeof(EnvironmentVar));
	var->next = variables;
	var->name = strdup(name);
	var->value = strdup(value);
	variables = var;
}

void nvram_listvars() {
	EnvironmentVar* var = variables;
	while(var != NULL) {
		bufferPrintf("%s = %s\r\n", var->name, var->value);
		var = var->next;
	}
}

static EnvironmentVar* loadEnvironment(NVRamAtom* atoms) {
	EnvironmentVar* toRet = NULL;
	EnvironmentVar* curVar = NULL;

	NVRamAtom* commonAtom = findAtom(atoms, "common");
	char* loc = (char*) commonAtom->data;
	char* name = loc;
	char* value = NULL;
	size_t nameLen = 0;
	while(TRUE) {
		if(*loc == '\0') {
			if(curVar == NULL) {
				toRet = curVar = (EnvironmentVar*) malloc(sizeof(EnvironmentVar));
			} else {
				curVar->next = (EnvironmentVar*) malloc(sizeof(EnvironmentVar)); 
				curVar = curVar->next;
			}

			curVar->next = NULL;

			curVar->name = (char*) malloc(nameLen + 1);
			memcpy(curVar->name, name, nameLen);
			curVar->name[nameLen] = '\0';

			if(value) {
				size_t valueLen = strlen(value);
				curVar->value = (char*) malloc(valueLen + 1);
				memcpy(curVar->value, value, valueLen);
				curVar->value[valueLen] = '\0';
			} else {
				curVar->value = (char*) malloc(1);
				curVar->value[0] = '\0';
			}

			if(nameLen == 0) {
				break;
			}

			name = loc + 1;
			nameLen = 0;
			value = NULL;
		} else if(*loc == '=') {
			value = loc + 1;
		} else if(value == NULL) {
			nameLen++;
		}
		loc++;
	}

	return toRet;
}

int nvram_setup() {
	bank1Data = (uint8_t*) malloc(NVRAM_SIZE);
	bank2Data = (uint8_t*) malloc(NVRAM_SIZE);
	nor_read(bank1Data, NVRAM_START, NVRAM_SIZE);	
	nor_read(bank2Data, NVRAM_START + NVRAM_SIZE, NVRAM_SIZE);

	bank1Atoms = readAtoms(bank1Data);
	bank2Atoms = readAtoms(bank2Data);

	if(bank1Atoms) {
		bufferPrintf("Successfully loaded bank1 nvram\r\n");
	}

	if(bank2Atoms) {
		bufferPrintf("Successfully loaded bank2 nvram\r\n");
	}

	if(!bank1Atoms && !bank2Atoms) {
		bufferPrintf("Could not load either bank1 nor bank2 nvram!\r\n");
		return -1;
	}

	NVRamAtom* ckDataAtom1 = findAtom(bank1Atoms, "nvram");
	NVRamAtom* ckDataAtom2 = findAtom(bank2Atoms, "nvram");

	if(ckDataAtom1 == NULL) {
		oldestBank = NVRAM_START + NVRAM_SIZE;
		newestBank = bank2Atoms;
		newestBankData = bank2Data;
	} else if(ckDataAtom2 == NULL) {
		oldestBank = NVRAM_START;
		newestBank = bank1Atoms;
		newestBankData = bank1Data;
	} else if(((NVRamData*)(ckDataAtom1->data))->epoch < ((NVRamData*)(ckDataAtom2->data))->epoch) {
		oldestBank = NVRAM_START;
		newestBank = bank2Atoms;
		newestBankData = bank2Data;
	} else {
		oldestBank = NVRAM_START + NVRAM_SIZE;
		newestBank = bank1Atoms;
		newestBankData = bank1Data;
	}

	variables = loadEnvironment(newestBank);

	return 0;
}
