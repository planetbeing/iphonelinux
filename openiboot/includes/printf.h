#ifndef PRINTF_H
#define PRINTF_H

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
int vprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...);
int puts(const char *str);
#endif
