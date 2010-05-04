#ifndef LCD_H
#define LCD_H

#include "openiboot.h"
#include "clock.h"

typedef enum ColorSpace {
	RGB565 = 3,
	RGB888 = 6
} ColorSpace;

typedef struct LCDInfo {
	FrequencyBase freqBase;
	uint32_t pixelsPerSecond;
	uint32_t width;
	uint32_t horizontalBackPorch;
	uint32_t horizontalFrontPorch;
	uint32_t horizontalSyncPulseWidth;
	uint32_t height;
	uint32_t verticalBackPorch;
	uint32_t verticalFrontPorch;
	uint32_t verticalSyncPulseWidth;
	uint32_t IVClk;
	uint32_t IHSync;
	uint32_t IVSync;
	uint32_t IVDen;
	uint32_t OTFClockDivisor;
} LCDInfo;

struct Framebuffer;

typedef void (*HLineFunc)(struct Framebuffer* framebuffer, int start, int line_no, int length, int fill);
typedef void (*VLineFunc)(struct Framebuffer* framebuffer, int start, int line_no, int length, int fill);

typedef struct Framebuffer {
	volatile uint32_t* buffer;
	uint32_t width;
	uint32_t height;
	uint32_t lineWidth;
	ColorSpace colorSpace;
	HLineFunc hline;
	VLineFunc vline;
} Framebuffer;

typedef struct Window {
	int created;
	int width;
	int height;
	uint32_t lineBytes;
	Framebuffer framebuffer;
	uint32_t* lcdConPtr;
	uint32_t lcdCon[2];
} Window;

typedef struct GammaTable {
	uint32_t data[18];
} GammaTable;

typedef struct GammaTableDescriptor {
	uint32_t panelIDMatch;
	uint32_t panelIDMask;
	GammaTable table0;
	GammaTable table1;
	GammaTable table2;
} GammaTableDescriptor;

extern int LCDInitRegisterCount;
extern const uint16_t* LCDInitRegisters;
extern uint32_t LCDPanelID;

extern Window* currentWindow;
extern volatile uint32_t* CurFramebuffer;
extern uint32_t NextFramebuffer;

int lcd_setup();
void lcd_fill(uint32_t color);
void lcd_shutdown();
void lcd_set_backlight_level(int level);
void lcd_window_address(int window, uint32_t framebuffer);

#endif

