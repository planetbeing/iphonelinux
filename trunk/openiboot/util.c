#include "openiboot.h"
#include "uart.h"
#include "s5l8900.h"

void* memset(void* x, int fill, uint32_t size) {
	uint32_t i;
	for(i = 0; i < size; i++) {
		((uint8_t*)x)[i] = (uint8_t) fill;
	}
	return x;
}
