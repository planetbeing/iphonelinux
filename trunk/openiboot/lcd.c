#include "openiboot.h"
#include "lcd.h"
#include "power.h"
#include "clock.h"
#include "gpio.h"
#include "hardware/lcd.h"
#include "hardware/power.h"
#include "util.h"
#include "spi.h"
#include "i2c.h"
#include "timer.h"

static int lcd_has_init = FALSE;
static int lcd_init_attempted = FALSE;

const static LCDInfo optC = {FrequencyBaseDisplay, 10800000, 320, 15, 15, 16, 480, 4, 4, 4, 1, 0, 0, 0, 0};
static LCDInfo curTimings;

static Window* currentWindow;

static int numWindows = 0;
static uint32_t nextFramebuffer = 0;

static int initDisplay();
static int syrah_init();
static void syrah_quiesce();
static void initLCD(LCDInfo* info);
static void setWindowBuffer(int window, uint32_t* buffer);
static void configWindow(int window, int option28, int option16, int option12, int option0);
static void configLCD(int option20, int option24, int option16);
static void configureLCDClock(LCDInfo* info);
static void configureClockCON0(int OTFClockDivisor, int clockSource, int option4);

static Window* createWindow(int zero0, int zero1, int width, int height, ColorSpace colorSpace);
static int calculateStrideLen(ColorSpace colorSpace, int option, int width);
static void createFramebuffer(Framebuffer* framebuffer, uint32_t framebufferAddr, int width, int height, int lineWidth, ColorSpace colorSpace);
static void setWindow(int window, int zero0, int zero1, ColorSpace colorSpace, int width1, uint32_t framebuffer, int zero2, int width2, int zero3, int height);
static void setLayer(int window, int zero0, int zero1);

static void framebuffer_fill(Framebuffer* framebuffer, int x, int y, int width, int height, int fill);
static void hline_rgb888(Framebuffer* framebuffer, int start, int line_no, int length, int fill);

static void setCommandMode(OnOff swt);
static void transmitCommandOnSPI1(int command, int subcommand);
static void transmitCommandOnSPI1ClearBit7(int command, int subcommand);
static void transmitShortCommandOnSPI1(int command);
static void resetLCD();
static void enterRegisterMode();
static int getPanelStatus(int status_id);

int lcd_setup() {
	int backlightLevel = 0;

	nextFramebuffer = 0xFE00000;
	numWindows = 2;

	if(!lcd_has_init) {
		if(!lcd_init_attempted) {
			if(initDisplay() == 0) {
				// get proper backlight level
			} else {
				backlightLevel = 0;
			}

			lcd_init_attempted = TRUE;
		} else {
			backlightLevel = 0;
		}
	} else {
		// get proper backlight level
	}

	// set backlight level

	return 0;
}

static int initDisplay() {
	LCDInfo* info = &curTimings;

	memcpy(info, &optC, sizeof(LCDInfo));

	power_ctrl(POWER_LCD, ON);
	clock_gate_switch(LCD_CLOCKGATE1, ON);
	clock_gate_switch(LCD_CLOCKGATE2, ON);

	if(GET_REG(LCD + VIDCON0) & VIDCON0_ENVID_F) {
		syrah_quiesce();
	}

	
	SET_REG(LCD + VIDCON0, GET_REG(LCD + VIDCON0) & ~VIDCON0_ENVID_F);
	gpio_pin_output(LCD_GPIO_PIXEL_CLOCK_ENABLE, 0);
	SET_REG(LCD + LCD_CON, 1);

	initLCD(info);

	currentWindow = createWindow(0, 0, info->width, info->height, RGB888);
	if(currentWindow == NULL)
		return -1;

	framebuffer_fill(&currentWindow->framebuffer, 0, 0, framebuffer->width, framebuffer->height, 0);

	if(syrah_init() != 0)
		return -1;

	return 0;
}

static Window* createWindow(int zero0, int zero1, int width, int height, ColorSpace colorSpace) {
	Window* newWindow;
	int currentWindowNo = numWindows;

	if(currentWindowNo > 5)
		return NULL;

	numWindows++;

	newWindow = (Window*) malloc(sizeof(Window));
	newWindow->created = FALSE;
	newWindow->lcdConPtr = newWindow->lcdCon;
	newWindow->lcdCon[0] = currentWindowNo;
	newWindow->lcdCon[1] = (1 << (5 - currentWindowNo));
	newWindow->width = width;
	newWindow->height = height;

	int lineWidth = width + calculateStrideLen(colorSpace, 0, width);
	int lineBytes;

	switch(colorSpace) {
		case RGB565:
			lineBytes = lineWidth * 2;
			break;
		case RGB888:
			lineBytes = lineWidth * 4;
			break;
		default:
			return NULL;
	}

	newWindow->lineBytes = lineBytes;

	uint32_t currentFramebuffer = nextFramebuffer;

	nextFramebuffer = (currentFramebuffer
		+ (lineBytes * height)	// size we need
		+ 0xFFF)		// round up
		& 0xFFFFF000;		// align

	createFramebuffer(&newWindow->framebuffer, currentFramebuffer, width, height, lineWidth, colorSpace);

	setWindow(currentWindowNo, 0, 0, colorSpace, width, currentFramebuffer, 0, width, 0, height);
	setLayer(currentWindowNo, zero0, zero1);

	SET_REG(LCD + LCD_CON2, GET_REG(LCD + LCD_CON2) | ((newWindow->lcdConPtr[1] & 0x3F) << 2));

	newWindow->created = TRUE;

	return newWindow;
}

static void createFramebuffer(Framebuffer* framebuffer, uint32_t framebufferAddr, int width, int height, int lineWidth, ColorSpace colorSpace) {
	framebuffer->buffer = (uint32_t*) framebufferAddr;
	framebuffer->width = width;
	framebuffer->height = height;
	framebuffer->lineWidth = lineWidth;
	framebuffer->colorSpace = colorSpace;
	framebuffer->hline = hline_rgb888;
}

static void setWindow(int window, int zero0, int zero1, ColorSpace colorSpace, int width1, uint32_t framebuffer, int zero2, int width2, int zero3, int height) {
	int nibblesPerPixel;

	switch(colorSpace) {
		case RGB565:
			nibblesPerPixel = 4;
			break;
		case RGB888:
			nibblesPerPixel = 8;
			break;
		default:
			return;
	}

	int x = (((width2 * nibblesPerPixel) >> 3) - ((zero2 * nibblesPerPixel) >> 3)) & 0x3;
	int stride;
	if(x) {
		stride = 4 - x;
	} else if((width2 * nibblesPerPixel) & 0x7) {
		stride = 4;
	} else {
		stride = 0;
	}

	uint32_t sfn = ((stride & 0x7) << 24) | ((zero0 & 0x7) << 16) | ((zero1 & 0x3) << 12) | ((colorSpace & 0x7) << 8) | ((zero2 * nibblesPerPixel) & 0x7);
	uint32_t addr = (zero3 * ((((width1 * nibblesPerPixel) & 0x7) ? (((width1 * nibblesPerPixel) >> 3) + 1) : ((width1 * nibblesPerPixel) >> 3)) << 2)) +  framebuffer + (((zero2 * nibblesPerPixel) >> 3) << 2);
	uint32_t size = (height - zero3) | ((width2 - zero2) << 16);
	uint32_t hspan = (((width1 * nibblesPerPixel) & 0x7) ? (((width1 * nibblesPerPixel) >> 3) + 1) : ((width1 * nibblesPerPixel) >> 3)) << 2;
	uint32_t qlen = ((width2 * nibblesPerPixel) & 0x7) ? (((width2 * nibblesPerPixel) >> 3) + 1) : ((width2 * nibblesPerPixel) >> 3);
	bufferPrintf("SFN: 0x%x, Addr: 0x%x, Size: 0x%x, hspan: 0x%x, QLEN: 0x%x\r\n", sfn, addr, size, hspan, qlen);

	uint32_t windowBase;
	switch(window) {
		case 1:
			windowBase = LCD + 0x58;
			break;
		case 2:
			windowBase = LCD + 0x70;
			break;
		case 3:
			windowBase = LCD + 0x88;
			break;
		case 4:
			windowBase = LCD + 0xA0;
			break;
		case 5:
			windowBase = LCD + 0xB8;
			break;
		default:
			return;
	}

	SET_REG(windowBase + 4, sfn);
	SET_REG(windowBase + 8, addr);
	SET_REG(windowBase + 12, size);
	SET_REG(windowBase, hspan);
	SET_REG(windowBase + 16, qlen);
}

static void setLayer(int window, int zero0, int zero1) {
	uint32_t data = zero0 << 16 | zero1;
	switch(window) {
		case 1:
			SET_REG(LCD + 0x6C, data);
			break;
		case 2:
			SET_REG(LCD + 0x84, data);
			break;
		case 3:
			SET_REG(LCD + 0x9C, data);
			break;
		case 4:
			SET_REG(LCD + 0xB4, data);
			break;
		case 5:
			SET_REG(LCD + 0xCC, data);
			break;
	}
}

static int calculateStrideLen(ColorSpace colorSpace, int extra, int width) {
	int nibblesPerPixel;

	switch(colorSpace) {
		case RGB565:
			nibblesPerPixel = 4;
			break;
		case RGB888:
			nibblesPerPixel = 8;
			break;
		default:
			return -1;
	}

	int ddPerLine = (nibblesPerPixel * width)/(sizeof(uint32_t) * 2);

	if(ddPerLine & 0x3) {
		// ddPerLine is not a multiple of 4
		return (sizeof(uint32_t) - (ddPerLine & 0x3));
	} else if((nibblesPerPixel * width) & 0x7) {
		return 4;
	} else {
		// already aligned
		return 0;
	}
}

static void initLCD(LCDInfo* info) {
	SET_REG(LCD + WND_CON, 1);

	uint32_t buffer[0x100];
	int i;
	for(i = 0; i < 0x100; i++) {
		buffer[i] = 0;
	}

	for(i = 0; i < NUM_WINDOWS; i++) {
		setWindowBuffer(i, buffer);
		configWindow(i, 0, 0, 1, 0);
	}

	SET_REG(LCD + 0xDC, 0);
	SET_REG(LCD + 0xE4, 0);
	SET_REG(LCD + 0xEC, 0);

	SET_REG(LCD + 0x1EC, 0);
	SET_REG(LCD + 0x1F0, 0);
	SET_REG(LCD + 0x1F4, 0x10);
	SET_REG(LCD + 0x1F8, 0);
	SET_REG(LCD + 0x1FC, 0);

	SET_REG(LCD + 0xD4, GET_REG(LCD + 0xD4) & ~(0x7 << 8));
	SET_REG(LCD + 0xD4, GET_REG(LCD + 0xD4) & ~(0x7));
	SET_REG(LCD + LCD_CON2, (GET_REG(LCD + 0x8) & ~((1 << 0) | (1 << 29))) | ((1 << 0) | (1 << 29)));

	configLCD(2, 2, 2);

	SET_REG(LCD + 0xC, 0);

	SET_REG(LCD + 0xD4, (GET_REG(LCD + 0xD4) & ~(1 << 15)) | (1 << 15));
	SET_REG(LCD + 0xD4, (GET_REG(LCD + 0xD4) & ~(1 << 15)) | (1 << 15));
	SET_REG(LCD + 0xD4, GET_REG(LCD + 0xD4) & ~(0x3F << 2));	// field of 6 bits starting at bit 2
	SET_REG(LCD + 0xD4, GET_REG(LCD + 0xD4) & ~(1 << 1));

	configureLCDClock(info);

}

static void setWindowBuffer(int window, uint32_t* buffer) {
	volatile uint32_t* windowBuffer;
	switch(window) {
		case 0:
			windowBuffer = (uint32_t*)(LCD + 0x400);
			break;
		case 1:
			windowBuffer = (uint32_t*)(LCD + 0x800);
			break;
		case 2:
			windowBuffer = (uint32_t*)(LCD + 0xC00);
			break;
		default:
			return;
	}

	int i;
	for(i = 0; i < 0x100; i++) {
		windowBuffer[i] = buffer[i];
	}
}

static void configWindow(int window, int option28, int option16, int option12, int option0) {
	uint32_t options = (option28 << 28) | (option16 << 16) | ((option12 & 0xF) << 12) | option0;

	switch(window) {
		case 0:
			SET_REG(LCD + 0xD8, options);
			break;
		case 1:
			SET_REG(LCD + 0xE0, options);
			break;
		case 2:
			SET_REG(LCD + 0xE8, options);
			break;
	}
}

static void configLCD(int option20, int option24, int option16) {
	switch(option20) {
		case 0:
			option20 = 2;
			break;
		case 1:
			option20 = 1;
			break;
		case 2:	
			option20 = 0;
			break;
		default:
			return;
	}

	switch(option24) {
		case 0:
			option24 = 2;
			break;
		case 1:
			option24 = 1;
			break;
		case 2:
			option24 = 0;
			break;
		default:
			return;
	}

	switch(option16) {
		case 0:
			option16 = 2;
			break;
		case 1:
			option16 = 1;
			break;
		case 2:
			option16 = 0;
			break;
		default:
			return;
	}

	SET_REG(LCD + LCD_CON2, GET_REG(LCD + LCD_CON2) | ((option16 & 0x3) << 16) | ((option20 & 0x3) << 20) | ((option24 & 0x3) << 24));

}

static void configureLCDClock(LCDInfo* info) {
	int clockSource;
	if(info->freqBase == FrequencyBaseBus) {
		clockSource = VIDCON0_BUSCLOCK;
	} else if(info->freqBase == FrequencyBaseDisplay) {
		clockSource = VIDCON0_DISPLAYCLOCK;
	} else {
		return;
	}

	uint32_t frequency = clock_get_frequency(info->freqBase);

	info->OTFClockDivisor = frequency/info->pixelsPerSecond;

	uint32_t pixelsPerFrame = (info->width + info->horizontalBackPorch + info->horizontalFrontPorch + info->horizontalSyncPulseWidth)
					* (info->height + info->verticalBackPorch + info->verticalFrontPorch + info->verticalSyncPulseWidth);


	uint32_t framesPer1000Second = ((uint64_t)frequency * (uint64_t)1000000)/info->OTFClockDivisor/pixelsPerFrame/1000;

	SET_REG(LCD + VIDCON1,
		((info->IVClk ? 1 : 0) << VIDCON1_IVCLKSHIFT)
		| ((info->IHSync ? 1 : 0) << VIDCON1_IHSYNCSHIFT)
		| ((info->IVSync ? 1 : 0) << VIDCON1_IVSYNCSHIFT)
		| ((info->IVDen ? 1 : 0) << VIDCON1_IVDENSHIFT));

	SET_REG(LCD + VIDTCON3, 1);

	SET_REG(LCD + VIDTCON0,
		(((info->verticalBackPorch - 1) & VIDTCON_BACKPORCHMASK) << VIDTCON_BACKPORCHSHIFT)
		| (((info->verticalFrontPorch - 1) & VIDTCON_FRONTPORCHMASK) << VIDTCON_FRONTPORCHSHIFT)
		| (((info->verticalSyncPulseWidth - 1) & VIDTCON_SYNCPULSEWIDTHMASK) << VIDTCON_SYNCPULSEWIDTHSHIFT));

	SET_REG(LCD + VIDTCON1,
		(((info->horizontalBackPorch - 1) & VIDTCON_BACKPORCHMASK) << VIDTCON_BACKPORCHSHIFT)
		| (((info->horizontalFrontPorch - 1) & VIDTCON_FRONTPORCHMASK) << VIDTCON_FRONTPORCHSHIFT)
		| (((info->horizontalSyncPulseWidth - 1) & VIDTCON_SYNCPULSEWIDTHMASK) << VIDTCON_SYNCPULSEWIDTHSHIFT));

	configureClockCON0(info->OTFClockDivisor, clockSource, 0);

	SET_REG(LCD + VIDTCON2, ((info->width & VIDTCON2_HOZVALMASK) << VIDTCON2_HOZVALSHIFT) | ((info->width & VIDTCON2_LINEVALMASK) << VIDTCON2_LINEVALSHIFT));

	bufferPrintf("fps set to: %d.%03d\r\n", framesPer1000Second / 1000, framesPer1000Second % 1000);
}

static void configureClockCON0(int OTFClockDivisor, int clockSource, int option4) {
	bufferPrintf("otf clock divisor %d\r\n", OTFClockDivisor);

	SET_REG(LCD + VIDCON0,
		(GET_REG(LCD + VIDCON0) & ~((VIDCON0_CLOCKMASK << VIDCON0_CLOCKSHIFT)
			| (VIDCON0_OPTION4MASK << VIDCON0_OPTION4SHIFT)
			| (VIDCON0_OTFCLOCKDIVISORMASK <<  VIDCON0_OTFCLOCKDIVISORSHIFT)))
		| ((clockSource & VIDCON0_CLOCKMASK) << VIDCON0_CLOCKSHIFT)
		| ((clockSource & VIDCON0_OPTION4MASK) << VIDCON0_OPTION4SHIFT)
		| (((OTFClockDivisor - 1) & VIDCON0_OTFCLOCKDIVISORMASK) << VIDCON0_OTFCLOCKDIVISORSHIFT));
}

static int syrah_init() {
	bufferPrintf("syrah_ihit() -- OMG-I'm-dreading-this-function code version I-Don't-Even-Know-What-The-Date-Is\r\n");

	return 0;
	spi_set_baud(1, 1000000, SPIOption13Setting0, 1, 1, 1);
	spi_set_baud(0, 500000, SPIOption13Setting0, 1, 0, 0);

	setCommandMode(ON);

	gpio_pin_output(LCD_GPIO_MPL_RX_ENABLE, 0);
	gpio_pin_output(LCD_GPIO_POWER_ENABLE, 0);
	resetLCD();

	gpio_pin_output(LCD_GPIO_POWER_ENABLE, 1);
	udelay(10000);

	transmitCommandOnSPI1(0x6D, 0x40);

	enterRegisterMode();

	udelay(1000);

	setCommandMode(OFF);

	return 0;
}

static void resetLCD() {
	gpio_pin_output(LCD_GPIO_RESET, 1);
	udelay(10000);
	gpio_pin_output(LCD_GPIO_RESET, 0);
}

static void setCommandMode(OnOff swt) {
	uint8_t lcdCommand[2];

	lcdCommand[0] = LCD_I2C_COMMAND;

	if(swt) {
		lcdCommand[1] = LCD_I2C_COMMANDMODE_ON;
	} else {
		lcdCommand[1] = LCD_I2C_COMMANDMODE_OFF;
	}

	i2c_tx(LCD_I2C_BUS, LCD_I2C_ADDR, lcdCommand, sizeof(lcdCommand));
}

static void enterRegisterMode() {
	int tries = 0;
	int status;

	do {
		transmitShortCommandOnSPI1(0xDE);
		status = getPanelStatus(0x15);
		tries += 1;

		if(tries >= 20000) {
			bufferPrintf("enter register mode timeout\r\n");
			break;
		}
	} while((status & 0x1) != 0);

	bufferPrintf("enter register mode success!\r\n");
}

static void transmitCommandOnSPI1(int command, int subcommand) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = command;
	lcdCommand[1] = subcommand;

	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);
}

static void transmitCommandOnSPI1ClearBit7(int command, int subcommand) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = command & 0x7F;
	lcdCommand[1] = subcommand;

	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);
}


static void transmitShortCommandOnSPI1(int command) {
	uint8_t lcdCommand[1];
	lcdCommand[0] = command;

	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 1, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);
}

static int getPanelStatus(int status_id) {
	uint8_t lcdCommand[1];
	uint8_t buffer[1];
	lcdCommand[0] = 0x80 | status_id;

	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 1, TRUE, 0);
	spi_rx(1, buffer, 1, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);

	return buffer[0];
}

static void syrah_quiesce() {

}

static void framebuffer_fill(Framebuffer* framebuffer, int x, int y, int width, int height, int fill) {
	if(x >= framebuffer->width)
		return;

	if(y >= framebuffer->height)
		return;

	if((x + width) > framebuffer->width) {
		width = framebuffer->width - x;
	}

	int maxLine;
	if((y + height) > framebuffer->height) {
		maxLine = framebuffer->height;
	} else {
		maxLine = y + height;
	}

	int line;
	for(line = y; line < height; line++) {
		framebuffer->hline(framebuffer, x, line, width, fill);
	}
}

static void hline_rgb888(Framebuffer* framebuffer, int start, int line_no, int length, int fill) {
	int i;
	uint32_t* line;

	fill = fill & 0xffffff;	// no alpha
	line = &framebuffer->buffer[line_no * framebuffer->lineWidth];

	for(i = start; i < length; i++) {
		line[i] = fill;
	}
}

