#include "openiboot.h"
#include "hardware/s5l8900.h"
#include "uart.h"
#include "util.h"
#include "openiboot-asmhelpers.h"

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

/* Adapted from public domain memmove function from  David MacKenzie <djm@gnu.ai.mit.edu>.  */
void* memmove(void *dest, const void* src, size_t length)
{
	register uint8_t* myDest = dest;
	const register uint8_t* mySrc = src;
	if (mySrc < myDest)
		/* Moving from low mem to hi mem; start at end.  */
		for(src += length, myDest += length; length; --length)
			*--myDest = *--mySrc;
	else if (src != myDest)
	{
		/* Moving from hi mem to low mem; start at beginning.  */
		for (; length; --length)
			*myDest++ = *mySrc++;
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
	return uart_write(0, str, strlen(str));
}

void dump_memory(uint32_t start, int length) {
	uint32_t curPos = start;
	int x = 0;
	while(curPos < (start + length)) {
		if(x == 0) {
			printf("0x%08x:", (unsigned int) curPos);
		}
		printf(" %08x", (unsigned int) GET_REG(curPos));
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

static char* MyBuffer= NULL;
static char* pMyBuffer = NULL;
static size_t MyBufferLen = 0;

#define SCROLLBACK_LEN (1024*1024)

void bufferPrint(const char* toBuffer) {
	EnterCriticalSection();
	if(pMyBuffer == NULL) {
		MyBuffer = (char*) malloc(SCROLLBACK_LEN);
		MyBufferLen = 0;
		pMyBuffer = MyBuffer;
		*pMyBuffer = '\0';
	}
	int len = strlen(toBuffer);
	MyBufferLen += len;

	if((MyBufferLen + 1) > SCROLLBACK_LEN) {
		len -= (MyBufferLen + 1) - SCROLLBACK_LEN;
		MyBufferLen = SCROLLBACK_LEN;
	}

	memcpy(pMyBuffer, toBuffer, len + 1);
	pMyBuffer += len;
	LeaveCriticalSection();
}

void bufferPrintf(const char* format, ...) {
	char buffer[1000];
	buffer[0] = '\0';

	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	bufferPrint(buffer);
}

void bufferFlush(char* destination, size_t length) {
	EnterCriticalSection();
	memcpy(destination, MyBuffer, length);
	memmove(MyBuffer, MyBuffer + length, MyBufferLen - length);
	MyBufferLen -= length;
	pMyBuffer -= length;
	*pMyBuffer = '\0';
	LeaveCriticalSection();
}

char* getScrollback() {
	return MyBuffer;
}

size_t getScrollbackLen() {
	return MyBufferLen;
}


