#include "openiboot.h"
#include "hardware/s5l8900.h"
#include "uart.h"
#include "util.h"
#include "openiboot-asmhelpers.h"
#include "framebuffer.h"

void __assert(const char* file, int line, const char* m) {
	bufferPrintf("ASSERT FAILED: %s at %s:%d\r\n", m, file, line);
	panic();
}

void abort() {
	bufferPrintf("openiboot ABORT!!\r\n");
	while(TRUE);
}

void panic() {
	bufferPrintf("openiboot PANIC!!\r\n");
	while(TRUE);
}

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

int strcmp(const char* s1, const char* s2) {
	while(*s1 == *(s2++)) {
		if(*(s1++) == '\0')
			return 0;
	}

	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

char* strchr(const char* s1, int s2)
{
	while(*s1)
	{
		if(*s1 == s2)
			return ((char*) s1);

		++s1;
	}

	return NULL;
}

char* strstr(const char* s1, const char* s2)
{
	while(*s1)
	{
		const char* c = s1;
		const char* p = s2;

		while(*c && *p && (*c == *p))
		{
			++c;
			++p;
		}

		if(*p == '\0')
		{
			return ((char*) s1);
		}

		++s1;
	}

	return NULL;
}

char* strdup(const char* str) {
	size_t len = strlen(str);
	char* toRet = (char*) malloc(len + 1);
	memcpy(toRet, str, len);
	toRet[len] = '\0';
	return toRet;
}

char* strcpy(char* dst, const char* src) {
	char* origDest =dst;
	while(*src != '\0') {
		*dst = *src;
		dst++;
		src++;
	}
	*dst = '\0';
	return origDest;
}

int memcmp(const void* s1, const void* s2, uint32_t size) {
	uint32_t i;
	const uint8_t* a = s1;
	const uint8_t* b = s2;
	for(i = 0; i < size; i++) {
		if(a[i] == b[i])
			continue;

		if(a[i] < b[i])
			return -1;

		if(a[i] > b[i])
			return 1;
	}

	return 0;

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

size_t strlen(const char* str) {
	int ret = 0;
	while(*str != '\0') {
		++str;
		++ret;
	}
	return ret;
}

int tolower(int c) {
	if(c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	else
		return c;
}

int putchar(int c) {
	char toPrint[] = {c, 0};
	bufferPrint(toPrint);
	return c;
}

int puts(const char *str) {
	bufferPrint(str);
	return 0;
}

unsigned long int parseNumber(const char* str) {
	int base = 10;
	if(*str == '0') {
		if(*(str + 1) == 'x') {
			base = 16;
			str += 2;
		} else if(*(str + 1) == 'o') {
			base = 8;
			str += 2;
		} else if(*(str + 1) == 'd') {
			base = 10;
			str += 2;
		} else if(*(str + 1) == 'b') {
			base = 2;
			str += 2;
		} else {
			base = 8;
			str++;
		}
	}

	return strtoul(str, NULL, base);
}

unsigned long int strtoul(const char* str, char** endptr, int base) {
	if(base == 16) {
		if(*str == '0' && *(str + 1) == 'x')
			str += 2;
	}

	char maxDigit;

	if(base <= 10) {
		maxDigit = '0' + base - 1;
	} else {
		maxDigit = 'a' + base - 11;
	}

	const char *myEndPtr = str;
	while(*myEndPtr != '\0' && ((*myEndPtr >= '0' && *myEndPtr <= '9') || (tolower(*myEndPtr) >= 'a' && tolower(*myEndPtr) <= maxDigit))) {
		myEndPtr++;
	}

	if(endptr != NULL) {
		*endptr = (char*) myEndPtr;
	}

	myEndPtr--;

	unsigned long int result = 0;
	unsigned long int currentPlace = 1;
	while(myEndPtr >= str) {
		if(*myEndPtr >= '0' && *myEndPtr <= '9') {
			result += currentPlace * (*myEndPtr - '0');
		} else if(tolower(*myEndPtr) >= 'a' && tolower(*myEndPtr) <= maxDigit) {
			result += currentPlace * (10 + (tolower(*myEndPtr) - 'a'));
		}

		currentPlace *= base;
		myEndPtr--;
	}

	return result;
}

void hexToBytes(const char* hex, uint8_t** buffer, int* bytes) {
	*bytes = strlen(hex) / 2;
	*buffer = (uint8_t*) malloc(*bytes);
	int i;
	for(i = 0; i < *bytes; i++) {
		uint32_t byte;
		char buf[3];
		buf[0] = hex[0];
		buf[1] = hex[1];
		buf[2] = '\0';
		byte = strtoul(buf, NULL, 16);
		(*buffer)[i] = byte;
		hex += 2;
	}
}

void bytesToHex(const uint8_t* buffer, int bytes) {
	while(bytes > 0) {
		bufferPrintf("%02x", *buffer);
		buffer++;
		bytes--;
	}
}

char** tokenize(char* commandline, int* argc) {
	char* pos;
	char** arguments;
	int curArg = 1;
	int inQuote = FALSE;
	int inEscape = TRUE;

	pos = commandline;
	arguments = (char**) malloc(sizeof(char*) * 10);
	arguments[0] = commandline;
	while(*commandline != '\0') {
		if(pos != commandline)
		{
			*pos = *commandline;
		}

		if(inEscape)
			inEscape = FALSE;
		else if(*commandline == '\"') {
		       	if(inQuote) {
				inQuote = FALSE;
				*pos = '\0';
			} else {
				inQuote = TRUE;
			}
		} else if(*commandline == ' ' && inQuote == FALSE) {
			*pos = '\0';
			arguments[curArg] = pos + 1;
			if(*(commandline + 1) == '\"')
				arguments[curArg]++;

			curArg++;
		} else if(*commandline == '\r' || *commandline =='\n') {
			*pos = '\0';
			break;
		} else if(*commandline == '\\') {
			pos--;
			inEscape = TRUE;
		}

		commandline++;
		pos++;
	}

	*argc = curArg;

	return arguments;
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

void buffer_dump_memory(uint32_t start, int length) {
	uint32_t curPos = start;
	int x = 0;
	while(curPos < (start + length)) {
		if(x == 0) {
			bufferPrintf("0x%08x:", (unsigned int) curPos);
		}
		bufferPrintf(" %08x", (unsigned int) GET_REG(curPos));
		if(x == 1) {
			bufferPrintf(" ");
		}
		if(x == 3) {
			bufferPrintf("\r\n");
			x = 0;
		} else {
			x++;
		}

		curPos += 4;
	}

	bufferPrintf("\r\n");
}

void buffer_dump_memory2(uint32_t start, int length, int width) {
	uint32_t curPos = start;
	int x = 0;
	while(curPos < (start + length)) {
		if(x == 0) {
			bufferPrintf("0x%08x:", (unsigned int) curPos);
		}
		bufferPrintf(" %08x", (unsigned int) GET_REG(curPos));
		if(x == (width - 1)) {
			bufferPrintf("\r\n");
			x = 0;
		} else {
			x++;
		}

		curPos += 4;
	}

	bufferPrintf("\r\n");
}


void hexdump(uint32_t start, int length) {
	uint32_t curPos = start;
	int x = 0;

	uint8_t line[16];
	int idx = 0;

	while(curPos < (start + length)) {
		if(x == 0) {
			bufferPrintf("%08x ", (unsigned int) curPos);
		}
		uint32_t mem = (uint32_t) GET_REG(curPos);
		uint8_t* memBytes = (uint8_t*) &mem;
 
		bufferPrintf(" %02x", (unsigned int)memBytes[0]);
		bufferPrintf(" %02x", (unsigned int)memBytes[1]);
		bufferPrintf(" %02x", (unsigned int)memBytes[2]);
		bufferPrintf(" %02x", (unsigned int)memBytes[3]);

		line[idx] = memBytes[0]; idx = (idx + 1) % 16;
		line[idx] = memBytes[1]; idx = (idx + 1) % 16;
		line[idx] = memBytes[2]; idx = (idx + 1) % 16;
		line[idx] = memBytes[3]; idx = (idx + 1) % 16;

		if(x == 1) {
			bufferPrintf(" ");
		}
		if(x == 3) {
			bufferPrintf("  |");
			int i;
			for(i = 0; i < 16; i++) {
				if(line[i] >= 32 && line[i] <= 126) {
					bufferPrintf("%c", line[i]);
				} else {
					bufferPrintf(".");
				}
			}
			bufferPrintf("|\r\n");
			x = 0;
		} else {
			x++;
		}

		curPos += 4;
	}

	bufferPrintf("\r\n");
}

static char* MyBuffer= NULL;
static char* pMyBuffer = NULL;
static size_t MyBufferLen = 0;

#define SCROLLBACK_LEN (1024*16)

void bufferDump(uint32_t location, unsigned int len) {
	EnterCriticalSection();
	if(pMyBuffer == NULL) {
		MyBuffer = (char*) malloc(SCROLLBACK_LEN);
		MyBufferLen = 0;
		pMyBuffer = MyBuffer;
	}

	memcpy(pMyBuffer, &len, sizeof(uint32_t));
	memcpy(pMyBuffer + sizeof(uint32_t), &location, sizeof(uint32_t));
	memcpy(pMyBuffer + sizeof(uint32_t) + sizeof(uint32_t), (void*) location, len);
	uint32_t crc = 0;
	crc32(&crc, (void*) location, len);
	*((uint32_t*)(pMyBuffer + sizeof(uint32_t) + sizeof(uint32_t) + len)) = crc;
	int totalLen = sizeof(uint32_t) + sizeof(uint32_t) + len + sizeof(uint32_t);
	if(totalLen % 0x80 != 0) {
		totalLen += 0x80 - (totalLen % 0x80);
	}
	MyBufferLen += totalLen;
	pMyBuffer += totalLen;
	LeaveCriticalSection();
}

int addToBuffer(const char* toBuffer, int len) {
	EnterCriticalSection();
	if(pMyBuffer == NULL) {
		MyBuffer = (char*) malloc(SCROLLBACK_LEN);
		MyBufferLen = 0;
		pMyBuffer = MyBuffer;
		*pMyBuffer = '\0';
	}

	if((MyBufferLen + len) > SCROLLBACK_LEN) {
		LeaveCriticalSection();
		return 0;
	}

	MyBufferLen += len;

	if((MyBufferLen + 1) > SCROLLBACK_LEN) {
		len -= (MyBufferLen + 1) - SCROLLBACK_LEN;
		MyBufferLen = SCROLLBACK_LEN;
	}

	memcpy(pMyBuffer, toBuffer, len);
	pMyBuffer[len] = '\0';
	pMyBuffer += len;
	LeaveCriticalSection();

	return 1;
}

void bufferPrint(const char* toBuffer) {
	if(UartHasInit)
		uartPrint(toBuffer);

	if(FramebufferHasInit)
		framebuffer_print(toBuffer);

	int len = strlen(toBuffer);
	addToBuffer(toBuffer, len);
}

void uartPrint(const char* toBuffer) {
	uart_write(0, toBuffer, strlen(toBuffer));
}

void bufferPrintf(const char* format, ...) {
	static char buffer[1000];
	EnterCriticalSection();
	buffer[0] = '\0';

	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	bufferPrint(buffer);
	LeaveCriticalSection();
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

void uartPrintf(const char* format, ...) {
	static char buffer[1000];
	EnterCriticalSection();
	buffer[0] = '\0';

	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	uartPrint(buffer);
	LeaveCriticalSection();
}

void fbPrintf(const char* format, ...) {
	static char buffer[1000];
	EnterCriticalSection();
	buffer[0] = '\0';

	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	framebuffer_print(buffer);
	LeaveCriticalSection();
}

char* getScrollback() {
	return MyBuffer;
}

size_t getScrollbackLen() {
	return MyBufferLen;
}

/*
 * CRC32 code ripped off (and adapted) from the zlib-1.1.3 distribution by Jean-loup Gailly and Mark Adler.
 *
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
static uint64_t crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8l, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
uint32_t crc32(uint32_t* ckSum, const void *buffer, size_t len)
{
  uint32_t crc;
  const uint8_t* buf = buffer;
  
  if(ckSum == NULL)
	  crc = 0;
  else
	  crc = *ckSum;
  
  if (buf == NULL) return crc;
  
  crc = crc ^ 0xffffffffL;
  while (len >= 8)
  {
    DO8(buf);
    len -= 8;
  }
  if (len)
  {
    do {
DO1(buf);
    } while (--len);
  }
  
  crc = crc ^ 0xffffffffL;
  
  if(ckSum != NULL)
	  *ckSum = crc;

  return crc;
}

#define BASE 65521L /* largest prime smaller than 65536 */
#define NMAX 5000  
// NMAX (was 5521) the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1

#define ADLER_DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define ADLER_DO2(buf,i)  ADLER_DO1(buf,i); ADLER_DO1(buf,i+1);
#define ADLER_DO4(buf,i)  ADLER_DO2(buf,i); ADLER_DO2(buf,i+2);
#define ADLER_DO8(buf,i)  ADLER_DO4(buf,i); ADLER_DO4(buf,i+4);
#define ADLER_DO16(buf)   ADLER_DO8(buf,0); ADLER_DO8(buf,8);

uint32_t adler32(uint8_t *buf, int32_t len)
{
    unsigned long s1 = 1; // adler & 0xffff;
    unsigned long s2 = 0; // (adler >> 16) & 0xffff;
    int k;

    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            ADLER_DO16(buf);
            buf += 16;
            k -= 16;
        }
        if (k != 0) do {
            s1 += *buf++;
            s2 += s1;
        } while (--k);
        s1 %= BASE;
        s2 %= BASE;
    }
    return (s2 << 16) | s1;
}

