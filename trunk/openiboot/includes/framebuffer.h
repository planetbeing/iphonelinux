#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "openiboot.h"

#define COLOR_WHITE 0xffffff
#define COLOR_BLACK 0x0

typedef struct OpenIBootFont {
	uint32_t width;
	uint32_t height;
	uint8_t data[];
} OpenIBootFont;

extern int FramebufferHasInit;

int framebuffer_setup();
int framebuffer_width();
int framebuffer_height();
int framebuffer_x();
int framebuffer_y();
void framebuffer_putc(int c);
void framebuffer_print(const char* str);
void framebuffer_setloc(int x, int y);
void framebuffer_clear();

#endif
