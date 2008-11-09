#include "openiboot.h"
#include "framebuffer.h"
#include "lcd.h"
#include "util.h"
#include "pcf/6x10.h"

static int TWidth;
static int THeight;
static int X;
static int Y;
static OpenIBootFont* Font;

static uint32_t FBWidth;
static uint32_t FBHeight;

int FramebufferHasInit = FALSE;
static int DisplayText = FALSE;

inline int getCharPixel(OpenIBootFont* font, int ch, int x, int y) {
	register int bitIndex = ((font->width * font->height) * ch) + (font->width * y) + x;
	return (font->data[bitIndex / 8] >> (bitIndex % 8)) & 0x1;
}

inline volatile uint32_t* PixelFromCoords(register uint32_t x, register uint32_t y) {
	return CurFramebuffer + (y * FBWidth) + x;
}

int framebuffer_setup() {
	Font = (OpenIBootFont*) fontData;
	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;
	TWidth = FBWidth / Font->width;
	THeight = FBHeight / Font->height;
	framebuffer_clear();
	FramebufferHasInit = TRUE;
	return 0;
}

void framebuffer_setdisplaytext(int onoff) {
	DisplayText = onoff;
}

void framebuffer_clear() {
	lcd_fill(COLOR_BLACK);
	X = 0;
	Y = 0;
}

int framebuffer_width() {
	return TWidth;
}

int framebuffer_height() {
	return THeight;
}

int framebuffer_x() {
	return X;
}

int framebuffer_y() {
	return Y;
}

void framebuffer_setloc(int x, int y) {
	X = x;
	Y = y;
}

void framebuffer_print(const char* str) {
	if(!DisplayText)
		return;

	size_t len = strlen(str);
	int i;
	for(i = 0; i < len; i++) {
		framebuffer_putc(str[i]);
	}
}

static void scrollup() {
	register volatile uint32_t* newFirstLine = PixelFromCoords(0, Font->height);
	register volatile uint32_t* oldFirstLine = PixelFromCoords(0, 0);
	register volatile uint32_t* end = oldFirstLine + (FBWidth * FBHeight);
	while(newFirstLine < end) {
		*(oldFirstLine++) = *(newFirstLine++);
	}
	while(oldFirstLine < end) {
		*(oldFirstLine++) = COLOR_BLACK;
	}
	Y--;	
}

void framebuffer_putc(int c) {
	if(!DisplayText)
		return;

	if(c == '\r') {
		X = 0;
	} else if(c == '\n') {
		X = 0;
		Y++;
	} else {
		register uint32_t sx;
		register uint32_t sy;
		for(sy = 0; sy < Font->height; sy++) {
			for(sx = 0; sx < Font->width; sx++) {
				if(getCharPixel(Font, c, sx, sy)) {
					*(PixelFromCoords(sx + (Font->width * X), sy + (Font->height * Y))) = COLOR_WHITE;
				} else {
					*(PixelFromCoords(sx + (Font->width * X), sy + (Font->height * Y))) = COLOR_BLACK;
				}
			}
		}

		X++;
	}

	if(X == TWidth) {
		X = 0;
		Y++;
	}

	if(Y == THeight) {
		scrollup();
	}
}

void framebuffer_draw_image(uint32_t* image, int x, int y, int width, int height) {
	register uint32_t sx;
	register uint32_t sy;
	for(sy = 0; sy < height; sy++) {
		for(sx = 0; sx < width; sx++) {
			*(PixelFromCoords(sx + x, sy + y)) = image[(sy * width) + sx];
		}
	}
}

void framebuffer_draw_rect(uint32_t color, int x, int y, int width, int height) {
	currentWindow->framebuffer.hline(&currentWindow->framebuffer, x, y, width, color);
	currentWindow->framebuffer.hline(&currentWindow->framebuffer, x, y + height, width, color);
	currentWindow->framebuffer.vline(&currentWindow->framebuffer, y, x, height, color);
	currentWindow->framebuffer.vline(&currentWindow->framebuffer, y, x + width, height, color);
}

