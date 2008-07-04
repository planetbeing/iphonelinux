#ifndef UTIL_H
#define UTIL_H

#include "openiboot.h"
#include <stdarg.h>
#include <stdlib.h>

void* memset(void* x, int fill, uint32_t size);
void* memcpy(void* dest, const void* src, uint32_t size);
void* memmove(void *dest, const void* src, size_t length);
int strlen(const char* str);
int putchar(int c);
void dump_memory(uint32_t start, int length);
void buffer_dump_memory(uint32_t start, int length);
void bufferPrint(const char* toBuffer);
void bufferPrintf(const char* format, ...);
void bufferFlush(char* destination, size_t length);
char* getScrollback();
size_t getScrollbackLen();

#include "printf.h"
#include "malloc-2.8.3.h"

#endif
