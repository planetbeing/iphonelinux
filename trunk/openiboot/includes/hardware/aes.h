#ifndef HW_AES_H
#define HW_AES_H

#include "hardware/s5l8900.h"

// Device
#define AES 0x38C00000

// Registers
#define CONTROL 0x0
#define GO 0x4
#define UNKREG0 0x8
#define STATUS 0xC
#define UNKREG1 0x10
#define KEYLEN 0x14
#define INSIZE 0x18
#define INADDR 0x20
#define OUTSIZE 0x24
#define OUTADDR 0x28
#define AUXSIZE 0x2C
#define AUXADDR 0x30
#define SIZE3 0x34
#define KEY 0x4C
#define TYPE 0x6C
#define IV 0x74

// Values

#define KEYSIZE 0x20
#define IVSIZE 0x10

#define AES_ENCRYPT 1
#define AES_DECRYPT 0

#define GET_KEYLEN(x) GET_BITS(x, 16, 2)

#define AES_CLOCKGATE 0xA

#endif

