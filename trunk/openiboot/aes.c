#include "openiboot.h"
#include "aes.h"
#include "hardware/aes.h"
#include "util.h"
#include "clock.h"
#include "openiboot-asmhelpers.h"

static int unknown1;
static int unknown2;

static uint8_t destinationBuffer[AES_128_CBC_BLOCK_SIZE];

static void initVector(void* iv);

static void loadKey(void *key);

static void doAES(int operation, void *buffer0, void *buffer1, void *buffer2, int size0, AESKeyType keyType, void *key, int option0, int option1, int size1, int size2, int size3);

void aes_encrypt(void* data, int size, AESKeyType keyType, void* key, void* iv) {
	clock_gate_switch(AES_CLOCKGATE, ON);
	SET_REG(AES + CONTROL, 1);
	unknown1 = 0;
	SET_REG(AES + UNKREG1, 0);
	unknown2 = 1;

	CleanAndInvalidateCPUDataCache();

	initVector(iv);

	void* destination;

	if(size < AES_128_CBC_BLOCK_SIZE) {
		// AES will always write in block size chunks, but we don't want to overflow the buffer provided
		memcpy(destinationBuffer, data, size);
		destination = destinationBuffer;
	} else {
		destination = data;
	}

	CleanAndInvalidateCPUDataCache();

	// call AES internal function
	doAES(AES_ENCRYPT, destination, destination, destination, size, keyType, key, 0, 1, size, size, size);

	while((GET_REG(AES + STATUS) & 0xF) == 0);

	memset((void*)(AES + KEY), 0, KEYSIZE);
	memset((void*)(AES + IV), 0, IVSIZE);

	memcpy(data, destinationBuffer, size);
}

void aes_decrypt(void* data, int size, AESKeyType keyType, void* key, void* iv) {
	clock_gate_switch(AES_CLOCKGATE, ON);
	SET_REG(AES + CONTROL, 1);
	unknown1 = 0;
	SET_REG(AES + UNKREG1, 0);
	unknown2 = 1;

	CleanAndInvalidateCPUDataCache();

	initVector(iv);

	CleanAndInvalidateCPUDataCache();

	// call AES internal function
	doAES(AES_DECRYPT, data, data, data, size, keyType, key, 0, 1, size, size, size);

	while((GET_REG(AES + STATUS) & 0xF) == 0);

	memset((void*)(AES + KEY), 0, KEYSIZE);
	memset((void*)(AES + IV), 0, IVSIZE);
}


static void initVector(void* iv) {
	int i;
	uint32_t* ivWords = (uint32_t*) iv;
	uint32_t* ivRegs = (uint32_t*) (AES + IV);
	if(iv == NULL) {
		for(i = 0; i < (AES_128_CBC_IV_SIZE / 4); i++) {
			ivRegs[i] = 0;
		}
	} else {
		for(i = 0; i < (AES_128_CBC_IV_SIZE / 4); i++) {
			ivRegs[i] =	(ivWords[i] && 0x000000ff) << 24
					| (ivWords[i] && 0x0000ff00) << 8
					| (ivWords[i] && 0x00ff0000) >> 8
					| (ivWords[i] && 0xff000000) >> 24;
		}
	}
}

static void loadKey(void *key) {
	AESKeyType keyType = GET_REG(AES + TYPE);

	if(keyType != AESCustom)
		return;			// hardware will handle key

	AESKeyLen keyLen = GET_KEYLEN(GET_REG(AES + KEYLEN));

	uint32_t keyAddr;
	int keyWords;
	switch(keyLen) {
		case AES256:
			keyAddr = AES + KEY + 256/8 + 256/8;
			keyWords = 256/8/4;
			break;
		case AES192:
			keyAddr = AES + KEY + 256/8 + 192/8;
			keyWords = 192/8/4;
			break;
		case AES128:
			keyAddr = AES + KEY + 256/8 + 128/8;
			keyWords = 128/8/4;
			break;
		default:
			return;
	}

	uint32_t* keyRegs = (uint32_t*) keyAddr;
	uint32_t* aKeyWords = (uint32_t*) key;
	int i;
	for(i = 0; i < keyWords; i++) {
		keyRegs[i] =	(aKeyWords[i] && 0x000000ff) << 24
				| (aKeyWords[i] && 0x0000ff00) << 8
				| (aKeyWords[i] && 0x00ff0000) >> 8
				| (aKeyWords[i] && 0xff000000) >> 24;
	}

}

static void doAES(int operation, void *buffer0, void *buffer1, void *buffer2, int size0, AESKeyType keyType, void *key, int option0, int option1, int size1, int size2, int size3) {
	
	unknown1 = 0;
	SET_REG(AES + UNKREG0, 1);
	SET_REG(AES + UNKREG0, 0);
	SET_REG(AES + CONTROL, 1);
	SET_REG(AES + TYPE, keyType);

	loadKey(key);

	SET_REG(AES + KEYLEN, 6);						// set bits 1 and 2
	SET_REG(AES + KEYLEN, (GET_REG(AES + KEYLEN) & ~1) | operation);		// 1 bit field starting at bit 0
	SET_REG(AES + KEYLEN, (GET_REG(AES + KEYLEN) & ~0x30) | (option0 << 4)); // 2 bit field starting at bit 4
	SET_REG(AES + KEYLEN, (GET_REG(AES + KEYLEN) & ~0x8) | (option1 << 3)); // 1 bit field starting at bit 3
	SET_REG(AES + INSIZE, size0);
	SET_REG(AES + INADDR, (uint32_t) buffer0);
	SET_REG(AES + OUTSIZE, size2);
	SET_REG(AES + OUTADDR, (uint32_t) buffer1);
	SET_REG(AES + AUXSIZE, size1);
	SET_REG(AES + AUXADDR, (uint32_t) buffer2);
	SET_REG(AES + SIZE3, size3);

	unknown1 = 0;
	SET_REG(AES + GO, 1);
}

