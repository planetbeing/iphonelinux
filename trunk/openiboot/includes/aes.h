#ifndef AES_H
#define AES_H

#include "openiboot.h"

#define AES_128_CBC_IV_SIZE 16
#define AES_128_CBC_BLOCK_SIZE 64

typedef enum AESKeyType {
	AESCustom = 0,
	AESUID = 1,
	AESGID = 2
} AESKeyType;

typedef enum AESKeyLen {
	AES128 = 0,
	AES192 = 1,
	AES256 = 2
} AESKeyLen;

void aes_encrypt(void* data, int size, AESKeyType keyType, void* key, void* iv);
void aes_decrypt(void* data, int size, AESKeyType keyType, void* key, void* iv);

#endif

