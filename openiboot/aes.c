#include "openiboot.h"
#include "aes.h"
#include "hardware/aes.h"
#include "util.h"
#include "clock.h"
#include "openiboot-asmhelpers.h"

static int unknown1;
static int unknown2;

static uint8_t destinationBuffer[AES_128_CBC_BLOCK_SIZE];

static void initVector(const void* iv);

static void loadKey(const void *key);

static void doAES(int operation, void *buffer0, void *buffer1, void *buffer2, int size0, AESKeyType keyType, const void *key, int option0, int option1, int size1, int size2, int size3);

static const uint8_t Gen836[] = {0x00, 0xE5, 0xA0, 0xE6, 0x52, 0x6F, 0xAE, 0x66, 0xC5, 0xC1, 0xC6, 0xD4, 0xF1, 0x6D, 0x61, 0x80};
static const uint8_t Gen838[] = {0x8C, 0x83, 0x18, 0xA2, 0x7D, 0x7F, 0x03, 0x07, 0x17, 0xD2, 0xB8, 0xFC, 0x55, 0x14, 0xF8, 0xE1};

static const uint8_t GenImg2VerifyData[] =	{0xCD, 0xF3, 0x45, 0xB3, 0x12, 0xE7, 0x48, 0x85, 0x8B, 0xBE, 0x21, 0x47, 0xF0, 0xE5, 0x80, 0x88};
static const uint8_t GenImg2VerifyIV[] =	{0x41, 0x70, 0x5D, 0x11, 0x6F, 0x98, 0x4B, 0x82, 0x9C, 0x6C, 0x99, 0xBB, 0xA5, 0xF1, 0x78, 0x69};

static uint8_t Key836[16];
static uint8_t Key838[16];
static uint8_t KeyImg2Verify[16];

int aes_setup() {
	memcpy(Key836, Gen836, 16);
	aes_encrypt(Key836, 16, AESUID, NULL, NULL);

	memcpy(Key838, Gen838, 16);
	aes_encrypt(Key838, 16, AESUID, NULL, NULL);

	memcpy(KeyImg2Verify, GenImg2VerifyData, 16);
	aes_encrypt(KeyImg2Verify, 16, AESUID, NULL, GenImg2VerifyIV);

	return 0;
}

void aes_img2verify_encrypt(void* data, int size, const void* iv) {
	aes_encrypt(data, size, AESCustom, KeyImg2Verify, iv);
}

void aes_img2verify_decrypt(void* data, int size, const void* iv) {
	aes_decrypt(data, size, AESCustom, KeyImg2Verify, iv);
}


void aes_836_encrypt(void* data, int size, const void* iv) {
	aes_encrypt(data, size, AESCustom, Key836, iv);
}

void aes_836_decrypt(void* data, int size, const void* iv) {
	aes_decrypt(data, size, AESCustom, Key836, iv);
}

void aes_838_encrypt(void* data, int size, const void* iv) {
	aes_encrypt(data, size, AESCustom, Key838, iv);
}

void aes_838_decrypt(void* data, int size, const void* iv) {
	aes_decrypt(data, size, AESCustom, Key838, iv);
}


void aes_encrypt(void* data, int size, AESKeyType keyType, const void* key, const void* iv) {
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

	if(size < AES_128_CBC_BLOCK_SIZE)
		memcpy(data, destinationBuffer, size);

}

void aes_decrypt(void* data, int size, AESKeyType keyType, const void* key, const void* iv) {
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


static void initVector(const void* iv) {
	int i;
	uint32_t* ivWords = (uint32_t*) iv;
	uint32_t* ivRegs = (uint32_t*) (AES + IV);
	if(iv == NULL) {
		for(i = 0; i < (AES_128_CBC_IV_SIZE / 4); i++) {
			ivRegs[i] = 0;
		}
	} else {
		for(i = 0; i < (AES_128_CBC_IV_SIZE / 4); i++) {
			ivRegs[i] =	(ivWords[i] & 0x000000ff) << 24
					| (ivWords[i] & 0x0000ff00) << 8
					| (ivWords[i] & 0x00ff0000) >> 8
					| (ivWords[i] & 0xff000000) >> 24;
		}
	}
}

static void loadKey(const void *key) {
	AESKeyType keyType = GET_REG(AES + TYPE);

	if(keyType != AESCustom)
		return;			// hardware will handle key

	AESKeyLen keyLen = GET_KEYLEN(GET_REG(AES + KEYLEN));

	uint32_t keyAddr;
	int keyWords;
	switch(keyLen) {
		case AES256:
			keyAddr = AES + KEY + 256/8 - 256/8;
			keyWords = 256/8/4;
			break;
		case AES192:
			keyAddr = AES + KEY + 256/8 - 192/8;
			keyWords = 192/8/4;
			break;
		case AES128:
			keyAddr = AES + KEY + 256/8 - 128/8;
			keyWords = 128/8/4;
			break;
		default:
			return;
	}

	uint32_t* keyRegs = (uint32_t*) keyAddr;
	uint32_t* aKeyWords = (uint32_t*) key;
	int i;
	for(i = 0; i < keyWords; i++) {
		keyRegs[i] =	(aKeyWords[i] & 0x000000ff) << 24
				| (aKeyWords[i] & 0x0000ff00) << 8
				| (aKeyWords[i] & 0x00ff0000) >> 8
				| (aKeyWords[i] & 0xff000000) >> 24;
	}

}

static void doAES(int operation, void *buffer0, void *buffer1, void *buffer2, int size0, AESKeyType keyType, const void *key, int option0, int option1, int size1, int size2, int size3) {
	
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

