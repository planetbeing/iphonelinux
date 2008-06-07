#include "openiboot.h"
#include "hardware/s5l8900.h"
#include "uart.h"

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

int strlen(const char* str) {
	int ret = 0;
	while(*str != '\0') {
		++str;
		++ret;
	}
	return ret;
}

int putchar(int c) {
	char ch = (char) c;
	if(uart_write(0, &ch, 1) == 0)
		return c;
	else
		return -1;
}

int puts(const char *str) {
	uart_write(0, str, strlen(str));
	return 0;
}

void dump_memory(uint32_t start, int length) {
	uint32_t curPos = start;
	int x = 0;
	while(curPos < (start + length)) {
		if(x == 0) {
			printf("0x%08x:", curPos);
		}
		printf(" %08x", GET_REG(curPos));
		if(x == 1) {
			printf(" ");
		}
		if(x == 3) {
			printf("\r\n\n");
			x = 0;
		} else {
			x++;
		}

		curPos += 4;
	}

	printf("\r\n");
}
