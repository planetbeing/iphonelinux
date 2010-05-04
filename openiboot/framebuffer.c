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

static uint32_t BackgroundColor;
static uint32_t ForegroundColor;

#define BGR16(x) ((((((x) >> 16) & 0xFF) >> 3) << 11) | (((((x) >> 8) & 0xFF) >> 2) << 5) | (((x) & 0xFF) >> 3))
#define BGR32(x) ((((((x) >> 11) & 0x1F) << 3) << 16) | (((((x) >> 5) & 0x3F) << 2) << 8) | (((x) & 0x1F) << 3))

#define RGB16(x) (((((x) & 0xFF) >> 3) << 11) | (((((x) >> 8) & 0xFF) >> 2) << 5) | ((((x) >> 16) & 0xFF) >> 3))

#define RGBA2BGR(x) ((((x) >> 16) & 0xFF) | ((((x) >> 8) & 0xFF) << 8) | (((x) & 0xFF) << 16))

inline int getCharPixel(OpenIBootFont* font, int ch, int x, int y) {
	register int bitIndex = ((font->width * font->height) * ch) + (font->width * y) + x;
	return (font->data[bitIndex / 8] >> (bitIndex % 8)) & 0x1;
}

inline volatile uint32_t* PixelFromCoords(register uint32_t x, register uint32_t y) {
	return CurFramebuffer + (y * FBWidth) + x;
}

inline volatile uint16_t* PixelFromCoords565(register uint32_t x, register uint32_t y) {
	return ((uint16_t*)CurFramebuffer) + (y * FBWidth) + x;
}

int framebuffer_setup() {
	Font = (OpenIBootFont*) fontData;
	BackgroundColor = COLOR_BLACK;
	ForegroundColor = COLOR_WHITE;
	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;
	TWidth = FBWidth / Font->width;
	THeight = FBHeight / Font->height;
	framebuffer_clear();
	FramebufferHasInit = TRUE;
	return 0;
}

void framebuffer_setcolors(uint32_t fore, uint32_t back) {
	ForegroundColor = fore;
	BackgroundColor = back;
}

void framebuffer_setdisplaytext(int onoff) {
	DisplayText = onoff;
}

void framebuffer_clear() {
	lcd_fill(BackgroundColor);
	X = 0;
	Y = 0;
}

int framebuffer_width() {
	return FBWidth;
}

int framebuffer_height() {
	return FBHeight;
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

void framebuffer_print_force(const char* str) {
	size_t len = strlen(str);
	int i;
	for(i = 0; i < len; i++) {
		framebuffer_putc(str[i]);
	}
}

static void scrollup888() {
	register volatile uint32_t* newFirstLine = PixelFromCoords(0, Font->height);
	register volatile uint32_t* oldFirstLine = PixelFromCoords(0, 0);
	register volatile uint32_t* end = oldFirstLine + (FBWidth * FBHeight);
	while(newFirstLine < end) {
		*(oldFirstLine++) = *(newFirstLine++);
	}
	while(oldFirstLine < end) {
		*(oldFirstLine++) = BackgroundColor;
	}
	Y--;
}

static void scrollup565() {
	register volatile uint16_t* newFirstLine = PixelFromCoords565(0, Font->height);
	register volatile uint16_t* oldFirstLine = PixelFromCoords565(0, 0);
	register volatile uint16_t* end = oldFirstLine + (FBWidth * FBHeight);
	uint16_t bgcolor = BGR16(BackgroundColor);
	while(newFirstLine < end) {
		*(oldFirstLine++) = *(newFirstLine++);
	}
	while(oldFirstLine < end) {
		*(oldFirstLine++) = bgcolor;
	}
	Y--;
}

void framebuffer_putc888(int c) {
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
					*(PixelFromCoords(sx + (Font->width * X), sy + (Font->height * Y))) = ForegroundColor;
				} else {
					*(PixelFromCoords(sx + (Font->width * X), sy + (Font->height * Y))) = BackgroundColor;
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
		scrollup888();
	}
}

void framebuffer_putc565(int c) {
	uint16_t fgcolor = BGR16(ForegroundColor);
	uint16_t bgcolor = BGR16(BackgroundColor);

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
					*(PixelFromCoords565(sx + (Font->width * X), sy + (Font->height * Y))) = fgcolor;
				} else {
					*(PixelFromCoords565(sx + (Font->width * X), sy + (Font->height * Y))) = bgcolor;
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
		scrollup565();
	}
}

void framebuffer_putc(int c)
{
	if(currentWindow->framebuffer.colorSpace == RGB888)
		framebuffer_putc888(c);
	else
		framebuffer_putc565(c);
}

static void framebuffer_draw_image888(uint32_t* image, int x, int y, int width, int height) {
	register uint32_t sx;
	register uint32_t sy;
	for(sy = 0; sy < height; sy++) {
		for(sx = 0; sx < width; sx++) {
			*(PixelFromCoords(sx + x, sy + y)) = RGBA2BGR(image[(sy * width) + sx]);
		}
	}
}

static void framebuffer_draw_image565(uint32_t* image, int x, int y, int width, int height) {
	register uint32_t sx;
	register uint32_t sy;
	for(sy = 0; sy < height; sy++) {
		for(sx = 0; sx < width; sx++) {
			*(PixelFromCoords565(sx + x, sy + y)) = RGB16(image[(sy * width) + sx]);
		}
	}
}

void framebuffer_draw_image(uint32_t* image, int x, int y, int width, int height)
{
	if(currentWindow->framebuffer.colorSpace == RGB888)
		framebuffer_draw_image888(image, x, y, width, height);
	else
		framebuffer_draw_image565(image, x, y, width, height);
}

static void framebuffer_capture_image888(uint32_t* image, int x, int y, int width, int height) {
	register uint32_t sx;
	register uint32_t sy;
	for(sy = 0; sy < height; sy++) {
		for(sx = 0; sx < width; sx++) {
			image[(sy * width) + sx] = *(PixelFromCoords(sx + x, sy + y));
		}
	}
}

static void framebuffer_capture_image565(uint32_t* image, int x, int y, int width, int height) {
	register uint32_t sx;
	register uint32_t sy;
	for(sy = 0; sy < height; sy++) {
		for(sx = 0; sx < width; sx++) {
			image[(sy * width) + sx] = BGR32(*(PixelFromCoords565(sx + x, sy + y)));
		}
	}
}

void framebuffer_capture_image(uint32_t* image, int x, int y, int width, int height)
{
	if(currentWindow->framebuffer.colorSpace == RGB888)
		framebuffer_capture_image888(image, x, y, width, height);
	else
		framebuffer_capture_image565(image, x, y, width, height);
}

void framebuffer_draw_rect(uint32_t color, int x, int y, int width, int height) {
	currentWindow->framebuffer.hline(&currentWindow->framebuffer, x, y, width, color);
	currentWindow->framebuffer.hline(&currentWindow->framebuffer, x, y + height, width, color);
	currentWindow->framebuffer.vline(&currentWindow->framebuffer, y, x, height, color);
	currentWindow->framebuffer.vline(&currentWindow->framebuffer, y, x + width, height, color);
}

#ifndef SMALL
#ifndef NO_STBIMAGE
#include "stb_image.h"

uint32_t* framebuffer_load_image(const char* data, int len, int* width, int* height, int alpha) {
	int components;
	uint32_t* stbiData = (uint32_t*) stbi_load_from_memory((stbi_uc const*)data, len, width, height, &components, 4);
	if(!stbiData) {
		bufferPrintf("framebuffer: %s\r\n", stbi_failure_reason());
		return NULL;
	}
	return stbiData;
}
#endif
#endif

void framebuffer_blend_image(uint32_t* dst, int dstWidth, int dstHeight, uint32_t* src, int srcWidth, int srcHeight, int x, int y) {
	register uint32_t sx;
	register uint32_t sy;
	for(sy = 0; sy < srcHeight; sy++) {
		for(sx = 0; sx < srcWidth; sx++) {
			register uint32_t* dstPixel = &dst[((sy + y) * dstWidth) + (sx + x)];
			register uint32_t* srcPixel = &src[(sy * srcWidth) + sx];
			*dstPixel =
				((((*dstPixel & 0xFF) * (0x100 - (*srcPixel >> 24))) >> 8) + ((((*srcPixel >> 16) & 0xFF) * ((*srcPixel >> 24) + 1)) >> 8)) << 16
				| (((((*dstPixel >> 8) & 0xFF) * (0x100 - (*srcPixel >> 24))) >> 8) + ((((*srcPixel >> 8) & 0xFF) * ((*srcPixel >> 24) + 1)) >> 8)) << 8
				| (((((*dstPixel >> 16) & 0xFF) * (0x100 - (*srcPixel >> 24))) >> 8) + (((*srcPixel & 0xFF) * ((*srcPixel >> 24) + 1)) >> 8));
		}
	}
}	

void framebuffer_draw_rect_hgradient(int starting, int ending, int x, int y, int width, int height) {
	int step = (ending - starting) * 1000 / height;
	int level = starting * 1000;
	int i;
	for(i = 0; i < height; i++) {
		int color = level / 1000;
		currentWindow->framebuffer.hline(&currentWindow->framebuffer, x, y + i, width, (color & 0xFF) | ((color & 0xFF) << 8) | ((color & 0xFF) << 16));
		level += step;
	}
}

