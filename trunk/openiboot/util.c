#include "openiboot.h"

void* memset(void* x, int fill, uint32_t size) {
	uint32_t i;
	for(i = 0; i < size; i++) {
		((uint8_t*)x)[i] = (uint8_t) fill;
	}
	return x;
}

void* memcpy(void* dest, const void* src, uint32_t size) {
	uint32_t i;
	for(i = 0; i < size; i++) {
		((uint8_t*)dest)[i] = ((uint8_t*)src)[i];
	}
	return dest;
}
