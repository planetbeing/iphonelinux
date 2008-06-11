#ifndef UTIL_H
#define UTIL_H

#include "openiboot.h"
#include <stdarg.h>

void* memset(void* x, int fill, uint32_t size);
void* memcpy(void* dest, const void* src, uint32_t size);
int strlen(const char* str);
int putchar(int c);
void dump_memory(uint32_t start, int length);

#include "printf.h"
#include "malloc-2.8.3.h"

#endif
