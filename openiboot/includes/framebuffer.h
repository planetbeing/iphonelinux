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
void framebuffer_setdisplaytext(int onoff);
int framebuffer_width();
int framebuffer_height();
int framebuffer_x();
int framebuffer_y();
void framebuffer_putc(int c);
void framebuffer_print(const char* str);
void framebuffer_setloc(int x, int y);
void framebuffer_clear();
void framebuffer_draw_image(uint32_t* image, int x, int y, int width, int height);
void framebuffer_draw_rect(uint32_t color, int x, int y, int width, int height);

#endif
