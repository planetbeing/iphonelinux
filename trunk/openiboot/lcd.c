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
#include "pmu.h"

static int lcd_has_init = FALSE;
static int lcd_init_attempted = FALSE;

const static LCDInfo optC = {FrequencyBaseDisplay, 10800000, 320, 15, 15, 16, 480, 4, 4, 4, 1, 0, 0, 0, 0};
static LCDInfo curTimings;

Window* currentWindow;

volatile uint32_t* CurFramebuffer;

static int numWindows = 0;
static uint32_t nextFramebuffer = 0;

int LCDInitRegisterCount;
const uint16_t* LCDInitRegisters;

uint32_t LCDPanelID;

static const uint16_t TMDInitRegisters[] =
	{0x6B2E, 0x1943, 0x1844, 0x901D, 0xC030, 0x11E, 0x1831, 0x1832, 0x633, 0x634, 0x1137, 0x23B, 0x213C, 0x3E1F, 0x3E20, 0xE21, 0x422,
	0x623D, 0x43E, 0xD140, 0xE441, 0x942, 0x4848, 0x4A49, 0x684A, 0x204B, 0x514C, 0x264D, 0x434E, 0x484F, 0x4A50, 0x6851, 0x2057, 0x5158,
	0x2666,0x4367, 0x869, 0x416A, 0x6D6B, 0x206C, 0x724, 0x3A25, 0x4327, 0xE452, 0x39, 0x55};

static const int TMDInitRegisterCount = 46;

static const uint16_t SEInitRegisters[] = 
	{0x139, 0x113,0x841D,  0x3F,  0x30, 0x11E, 0x162,0x1431,
	0x1432, 0x433, 0xC34,0x5635, 0xC37, 0x63B,0x413C, 0x628,
	0xB29,0x351F,0x3520,0x1921,0x2922,0x3723,0x1C3D, 0xF3E,
	0x36,  0x38,     7, 0x108,0xDD40,0xEA41, 0x942,0x242E,
	0x1443,0x1044, 0xA47,0xA348,0x1849,0xDD4A, 0xF4B,0xC54C,
	0x744D,0x1E4E, 0x74F,0x1D50,0xDD51,0x1F57,0xE858,0x7466,
	0x1E67,0xC469,0x186A,0xDE6B,0x136C,0xC624,0x7825,0x1E27,
	0xE452, 0x42A,0x5078,  0x79,0x8F7A, 0x17B,0x287C,  0x7D,
	0x177E, 0x17F,  0x55};

static const int SEInitRegisterCount = 67;

static const uint16_t SharpInitRegisters[] =
	{0x113,0xD41D,  0x3F,0xA030,0x1954, 0x11E, 0x162, 0xA34,
	0x4E35,0x1531,0x1432, 0x533,0x4237, 0x73B,0x383C,0x331F,
	0x3320, 0xF21,0x1722,0x1F23,0x843D, 0x63E,     7, 0x108,
	0xD140,0xF441, 0x942, 0xA2E,0x1743,0x1944, 0x847,0xE248,
	0xC49,0xFC4A, 0xA4B,0x674C,0x704D,0x374E,0x274F,0x1550,
	0xFB51,0x1E57,0xA958,0x6C66,0x3767,0xE269, 0xC6A,0xFD6B,
	0xA6C,0x6724,0x7425,0x3727,0xE852, 0x42A,0x5078,  0x79,
	0x8F7A, 0x17B,0x287C,  0x7D,0x177E, 0x17F,0xFF19,0xFF1A,
	0x55};

static const int SharpInitRegisterCount = 65;

static const uint16_t SamsungInitRegisters[] =
	{0x452E,0x102D, 0xF2B,0x9646,0x1547,0x1143,0x1044,0x141D,
	0x4030, 0x139, 0x71E, 0x162,0x1431,0x1432, 0x733, 0xE34,
	0x5635, 0x237,0x443B,0x213C,0x9A18,0xAF48,0x4549,0x494A,
	0x244B,0x2C4C,0x2A4D,0x504E,0x6452, 0x22A,  0x28,  0x29,
	0x7B1F,0x7B20,0x1421,0x2C22,0x7F23,0x143D,0x903E, 0xC36,
	0x1938,0x7A40,0x5041,0x1942,  0x55};

static const int SamsungInitRegisterCount = 45;

static const uint16_t AUOInitRegisters[] = {0xB90A, 0x55};

static const int AUOInitRegisterCount = 2;

static const GammaTableDescriptor gammaTables[] =
	{{0xD100, 0xFF70,
		{{0x34328EB6, 0x34003400, 0x34003400, 0xD000D00, 0x3400D00, 0x71DC771D, 0x340D37, 0xD0, 0x1C007000, 0x1C1C070, 0xC771C707, 0xDDC7771D, 0xDC7771DD, 0xDDDDDDDD, 0x7477774, 0x70001C00, 0xE9C, 0}},
		{{0x343400F6, 0x434D0D0D, 0xD0D0D343, 0x34343434, 0x434D0D0D, 0x34000D03, 0x7000, 0x1C070070, 0xC0700700, 0x71C1C1C1, 0xC7071C1C, 0xDC771C71, 0xC71DC71D, 0xDDDC771D, 0x71D71D, 0xF701C01C, 0, 0}},
		{{0x74DDD0F6, 0xDD377777, 0x7774DDDD, 0xDDDDDD37, 0x77777774, 0x4034DDD3, 0xD00003, 0x70070000, 0x1C070700, 0x7771C707, 0xDDDC7777, 0xDD377774, 0x434D3434, 0x3434343, 0x1D010D, 0xF701C01C, 0, 0}}
	},
	{0xC200, 0xFF70,
		{{0xDDD37F2, 0x1DDDD34D, 0x4D341C, 0x40D1C4D0, 0x3400D3, 0x1C000000, 0xDDDDDC1C, 0xD34374, 0x3400D, 0, 0x70000000, 0, 0, 0, 0, 0x1C034, 0, 0}},
		{{0x774DCA98, 0x1C1C, 0x1C1DD34, 0x77774340, 0x70701C1C, 0xDDDC71C0, 0x3434DDD, 0xD, 0x1C000, 0xC07001C0, 0x701C01, 0x1C007, 0xD00035C, 0x40D0D034, 0xD37434D3, 0xD34DD374, 0xD, 0}},
		{{0x37774A98, 0x74DD374D, 0x34D374D3, 0x34D3434D, 0x74D34D34, 0xDD374DD3, 0xC771DDDD, 0x7071C1D, 0xD374D347, 0x34D34374, 0xD343434, 0x34D3434D, 0xDDDD374D, 0x71C771D, 0x7007, 0x340D00D0, 0, 0}}
	}};


static const PMURegisterData backlightOffData = {0x0, 0x29, 0x0};

static const PMURegisterData backlightData[] = {
	{0x0, 0x17, 0x1},
	{0x0, 0x2A, 0x0},
	{0x0, 0x28, 0x22},
	{0x0, 0x29, 0x1},
	{0x0, 0x2A, 0x6}
};

static int initDisplay();
static int syrah_init();
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
static void vline_rgb888(Framebuffer* framebuffer, int start, int line_no, int length, int fill);

static void setCommandMode(OnOff swt);
static void transmitCommandOnSPI0(int command, int subcommand);
static void transmitCommandOnSPI1(int command, int subcommand);
static void setPanelRegister(int command, int subcommand);
static void transmitShortCommandOnSPI1(int command);
static void resetLCD();
static void enterRegisterMode();
static int getPanelRegister(int status_id);
static void togglePixelClock(OnOff swt);
static void displayPanelInfo(uint8_t* panelID);

static void installGammaTables(uint32_t panelID);
static void installGammaTable(int tableNo, uint8_t* table);

int lcd_setup() {
	int backlightLevel = 0;

	nextFramebuffer = 0x0fd00000;
	numWindows = 2;

	if(!lcd_has_init) {
		if(!lcd_init_attempted) {
			if(initDisplay() == 0) {
				backlightLevel = 20;
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

	lcd_set_backlight_level(backlightLevel);

	CurFramebuffer = currentWindow->framebuffer.buffer;

	return 0;
}

void lcd_fill(uint32_t color) {
	framebuffer_fill(&currentWindow->framebuffer, 0, 0, currentWindow->framebuffer.width, currentWindow->framebuffer.height, color);
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

	SET_REG(LCD + 0x24, 0);

	currentWindow = createWindow(0, 0, info->width, info->height, RGB888);
	if(currentWindow == NULL)
		return -1;


	framebuffer_fill(&currentWindow->framebuffer, 0, 0, currentWindow->framebuffer.width, currentWindow->framebuffer.height, 0x0);

	if(syrah_init() != 0)
		return -1;

	installGammaTables(LCDPanelID);

	return 0;
}

static void installGammaTables(uint32_t panelID) {
	const GammaTableDescriptor* curTable = gammaTables;

	while(curTable < (gammaTables + sizeof(gammaTables))) {
		if((curTable->panelIDMask & panelID) == curTable->panelIDMatch) {
			bufferPrintf("Installing gamma table 0x%08lx / 0x%08lx\r\n", curTable->panelIDMatch, curTable->panelIDMask);
			installGammaTable(0, (uint8_t*) curTable->table0.data);
			installGammaTable(1, (uint8_t*) curTable->table1.data);
			installGammaTable(2, (uint8_t*) curTable->table2.data);
			return;
		}
		curTable++;
	}
	bufferPrintf("No matching gamma table found\r\n");

}

static int gammaVar1;
static int gammaVar2;
static uint8_t gammaVar3;

static uint8_t installGammaTableHelper(uint8_t* table) {
	if(gammaVar2 == 0) {
		gammaVar3 = table[gammaVar1++];
	}

	int toRet = (gammaVar3 >> gammaVar2) & 0x3;
	gammaVar2 += 2;

	if(gammaVar2 == 8)
		gammaVar2 = 0;

	return toRet;
}


static void installGammaTable(int tableNo, uint8_t* table) {
	uint32_t baseReg;
	switch(tableNo) {
		case 0:
			baseReg = LCD + 0x400;
			break;
		case 1:
			baseReg = LCD + 0x800;
			break;
		case 2:
			baseReg = LCD + 0xC00;
			break;

		default:
			return;
	}

	gammaVar1 = 0;
	gammaVar2 = 0;
	gammaVar3 = 0;

	uint8_t r4 = 0;
	uint16_t r6 = 0;
	uint8_t r8 = 0;

	SET_REG(baseReg, 0x0);

	uint32_t curReg = baseReg + 4;

	int i;
	for(i = 1; i < 0xFF; i++) {
		switch(installGammaTableHelper(table)) {
			case 0:
				r4 = 0;
				break;
			case 1:
				r4 = 1;
				break;
			case 2:
				{
					uint8_t a = installGammaTableHelper(table);
					uint8_t b = installGammaTableHelper(table);				
					r4 = a | (b << 2);
					if((r4 & (1 << 3)) != 0) {
						r4 |= 0xF0;
					}
				}
				break;
			case 3:
				r4 = 0xFF;
				break;
		}

		if(i == 1) {
			r4 = r4 + 8;
		}

		r8 += r4;
		r6 += r8;

		SET_REG(curReg, (uint32_t)r6);

		curReg += 4;
	}
	
	SET_REG(curReg, 0x3ff);
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
	framebuffer->vline = vline_rgb888;
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

	static uint32_t buffer[0x100];
	int i;
	for(i = 0; i < 0x100; i++) {
		buffer[i] = 0;
	}

	for(i = 0; i < NUM_WINDOWS; i++) {
		setWindowBuffer(i, buffer);
	}

	for(i = 0; i < NUM_WINDOWS; i++) {
		configWindow(i, 0, 0, 1, 0);
		SET_REG(LCD + 0xDC + (i * 0x8), 0);
	}

	SET_REG(LCD + 0x1EC, 0);
	SET_REG(LCD + 0x1F0, 0);
	SET_REG(LCD + 0x1F4, 0x10);
	SET_REG(LCD + 0x1F8, 0);
	SET_REG(LCD + 0x1FC, 0);

	SET_REG(LCD + 0xD4, GET_REG(LCD + 0xD4) & ~(0x7 << 8));
	SET_REG(LCD + 0xD4, GET_REG(LCD + 0xD4) & ~(0x7));
	SET_REG(LCD + LCD_CON2, (GET_REG(LCD + LCD_CON2) & ~((1 << 0) | (1 << 29))) | ((1 << 0) | (1 << 29)));

	configLCD(2, 2, 2);

	SET_REG(LCD + 0xC, 0);

	SET_REG(LCD + LCD_CON2, (GET_REG(LCD + LCD_CON2) & ~(1 << 15)) | (1 << 15));
	SET_REG(LCD + LCD_CON2, GET_REG(LCD + LCD_CON2) & ~(0x3F << 2));	// field of 6 bits starting at bit 2
	SET_REG(LCD + LCD_CON2, GET_REG(LCD + LCD_CON2) & ~(1 << 1));

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

	SET_REG(LCD + LCD_CON2, (GET_REG(LCD + LCD_CON2) & ~((0x3 << 16) | (0x3 << 20) | (0x3 << 24))) | ((option16 & 0x3) << 16) | ((option20 & 0x3) << 20) | ((option24 & 0x3) << 24));

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

	SET_REG(LCD + VIDTCON2, (((info->width - 1) & VIDTCON2_HOZVALMASK) << VIDTCON2_HOZVALSHIFT) | (((info->height - 1) & VIDTCON2_LINEVALMASK) << VIDTCON2_LINEVALSHIFT));

	bufferPrintf("fps set to: %d.%03d\r\n", framesPer1000Second / 1000, framesPer1000Second % 1000);
}

static void configureClockCON0(int OTFClockDivisor, int clockSource, int option4) {
	bufferPrintf("otf clock divisor: %d\r\n", OTFClockDivisor);

	SET_REG(LCD + VIDCON0,
		(GET_REG(LCD + VIDCON0) & ~((VIDCON0_CLOCKMASK << VIDCON0_CLOCKSHIFT)
			| (VIDCON0_OPTION4MASK << VIDCON0_OPTION4SHIFT)
			| (VIDCON0_OTFCLOCKDIVISORMASK <<  VIDCON0_OTFCLOCKDIVISORSHIFT)))
		| ((clockSource & VIDCON0_CLOCKMASK) << VIDCON0_CLOCKSHIFT)
		| ((option4 & VIDCON0_OPTION4MASK) << VIDCON0_OPTION4SHIFT)
		| (((OTFClockDivisor - 1) & VIDCON0_OTFCLOCKDIVISORMASK) << VIDCON0_OTFCLOCKDIVISORSHIFT));
}

static int syrah_init() {
	bufferPrintf("syrah_init() -- Hurray for displays\r\n");

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

	setPanelRegister(0x56, 0x8F);
	udelay(40000);

	setPanelRegister(0x5F, 1);

	transmitShortCommandOnSPI1(0xDF);
	udelay(10000);

	transmitShortCommandOnSPI1(0x11);
	udelay(70000);

	gpio_pin_output(LCD_GPIO_MPL_RX_ENABLE, 1);

	transmitCommandOnSPI0(0x16, 0xFF);
	transmitCommandOnSPI0(0x0, 0x10);
	transmitCommandOnSPI0(0xA, 0x2);

	// mpl dither is always off
	transmitCommandOnSPI0(0x8, 0x1);

	transmitCommandOnSPI0(0x2, 0x0);
	transmitCommandOnSPI0(0x3, 0x0);
	transmitCommandOnSPI0(0x4, 0x0);
	transmitCommandOnSPI0(0x5, 0x0);
	transmitCommandOnSPI0(0x6, 0x0);
	transmitCommandOnSPI0(0x7, 0x0);
	transmitCommandOnSPI0(0x0, 0x16);

	togglePixelClock(ON);
	udelay(40000);

	setPanelRegister(0x6D, 0x0);
	transmitCommandOnSPI1(0x36, 0x8);
	udelay(30000);

	uint8_t lcdCommand[2];
	uint8_t panelID[3];

	memset(panelID, 0, 3);

	lcdCommand[0] = 0xDA;
	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 1, TRUE, 0);
	spi_rx(1, &panelID[0], 1, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);

	lcdCommand[0] = 0xDB;
	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 1, TRUE, 0);
	spi_rx(1, &panelID[1], 1, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);

	lcdCommand[0] = 0xDC;
	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 1, TRUE, 0);
	spi_rx(1, &panelID[2], 1, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);

	if((panelID[2] & 0x7) == 1 || (panelID[2] & 0x7) == 3) {
		panelID[2] |= 0x8;
	}

	displayPanelInfo(panelID);

	if((panelID[2] & (1 << 3)) == 0) {
		enterRegisterMode();
		udelay(1000);

		setPanelRegister(0x55, 0x2);
		setPanelRegister(0x2E, 0xA0);
		udelay(1000);

		if(panelID[1] == 0 || panelID[1] == 0x80) {
			panelID[0] = getPanelRegister(0x5A);
			panelID[1] = getPanelRegister(0x5B);
			panelID[2] = getPanelRegister(0x5C);
			bufferPrintf("Trying to reading panel ID again...\r\n");
			displayPanelInfo(panelID);
			if(panelID[1] == 0 || panelID[1] == 0x80) {
				// Try to set panelID manually from environment variables. Basically we're screwed at this point.
			}
		}

		switch(panelID[1]) {
			case 0xC2:
				LCDInitRegisterCount = TMDInitRegisterCount;
				LCDInitRegisters = TMDInitRegisters;

				int originalVcs = getPanelRegister(0x47);
				bufferPrintf("Original value of Vcs (0x47): 0x%02x\r\n", originalVcs);
				if((panelID[2] & 0x70) == 0 && panelID[0] == 0x71 && originalVcs == 8) {
					bufferPrintf("Overwriting TMD register 0x47 to 0x0B from 0x08\r\n");
					uint16_t* myRegs = malloc((TMDInitRegisterCount  + 1) * sizeof(uint16_t));
					memcpy(myRegs, TMDInitRegisters, TMDInitRegisterCount * sizeof(uint16_t));
					myRegs[TMDInitRegisterCount] = 0x0B47;
					LCDInitRegisters = myRegs;
					LCDInitRegisterCount++;
				}
				break;
			case 0xB3:
				LCDInitRegisterCount = SEInitRegisterCount;
				LCDInitRegisters = SEInitRegisters;
				break;
			case 0xD1:
				LCDInitRegisterCount = SharpInitRegisterCount;
				LCDInitRegisters = SharpInitRegisters;
				break;
			case 0xE5:
				LCDInitRegisterCount = SamsungInitRegisterCount;
				LCDInitRegisters = SamsungInitRegisters;
				break;
			default:
				bufferPrintf("Unknown panel, we are screwed.\r\n");
				break;
		}
	} else {
		if((panelID[2] & 0x7) == 1) {
			enterRegisterMode();
			setPanelRegister(0x7E, 0x0);
			setPanelRegister(0x5F, 0x1);
			transmitShortCommandOnSPI1(0xDF);
		}
		transmitShortCommandOnSPI1(0x29);
		udelay(18000);
		enterRegisterMode();
		if(panelID[1] == 0xD1 && (panelID[2] & 0x7) == 2 && panelID[0] == 0xA1) {
			LCDInitRegisterCount = AUOInitRegisterCount;
			LCDInitRegisters = AUOInitRegisters;
		}
	}

	LCDPanelID = (panelID[0] << 16) | (panelID[1] << 8) | panelID[2];

	bufferPrintf("Writing LCD init registers...\r\n");
	int i;
	for(i = 0; i < LCDInitRegisterCount; i++) {
		uint8_t* regInfo = (uint8_t*) &LCDInitRegisters[i];
		setPanelRegister(regInfo[0], regInfo[1]);
	}

	switch(panelID[2] & 0x7) {
		case 0:
			bufferPrintf("Do init for Merlot\r\n");
			setPanelRegister(0x2E, getPanelRegister(0x2E) & 0x7F);
			break;
		case 2:
			bufferPrintf("turning on parity error flag");
			setPanelRegister(0x53, 0x5);
			setPanelRegister(0xB, 0x10);
			break;
		case 3:
			bufferPrintf("changing to continuous calibration mode");
			setPanelRegister(0x50, 0x3);
			// drop down to init for Novatek-5.x
		case 1:
			setPanelRegister(0x55, 0xB);
			break;
	}

	setCommandMode(OFF);

	udelay(40000);

	bufferPrintf("syrah_init success!\r\n");

	return 0;
}

static void displayPanelInfo(uint8_t* panelID) {
	bufferPrintf("Syrah Panel ID (0x%02x%02x%02x):\r\n", (int)panelID[0], (int)panelID[1], (int)panelID[2]);

	bufferPrintf("   Build:          ");
	switch(panelID[0] & 0xF0) {
		case 0x10:
			bufferPrintf("EVT%d ", panelID[0] & 0x0F);
			break;
		case 0x50:
			bufferPrintf("DVT%d ", panelID[0] & 0x0F);
			break;
		case 0x70:
			bufferPrintf("PVT%d ", panelID[0] & 0x0F);
			break;
		case 0xA0:
			bufferPrintf("MP%d ", panelID[0] & 0x0F);
			break;
		default:
			bufferPrintf("UNKNOWN ");
	}

	bufferPrintf("\r\n   Type:           ");
	switch(panelID[1]) {
		case 0xC2:
			bufferPrintf("TMD ");
			break;
		case 0xA4:
			bufferPrintf("AUO ");
			break;
		case 0xB3:
			bufferPrintf("SE ");
			break;
		case 0xD1:
			bufferPrintf("SHARP ");
			break;
		case 0xE5:
			bufferPrintf("SAMSUNG ");
			break;
		default:
			bufferPrintf("UNKNOWN ");
	}

	bufferPrintf("\r\n   Project/Driver: ");
	if((panelID[2] & 0x70) == 0) {
		bufferPrintf("M68/");
	} else if((panelID[2] & 0x70) == 0x10) {
		bufferPrintf("N45/");
	} else {
		bufferPrintf("UNKNOWN/");
	}

	switch(panelID[2] & 0x7) {
		case 0:
			bufferPrintf("NSC-Merlot ");
			break;
		case 1:
			bufferPrintf("Novatek-5.x ");
			break;
		case 2:
			bufferPrintf("NSC-Muscat ");
			break;
		case 3:
			bufferPrintf("Novatek-6.x ");
			break;
		default:
			bufferPrintf("UNKNOWN ");
	}
	bufferPrintf("\r\n");
}

static void togglePixelClock(OnOff swt) {
	if(swt) {
		SET_REG(LCD + LCD_0, 1);
		SET_REG(LCD + VIDCON0, GET_REG(LCD + VIDCON0) | 1);
		gpio_custom_io(LCD_GPIO_PIXEL_CLOCK_ENABLE, 0x2);
	} else {
		SET_REG(LCD + VIDCON0, GET_REG(LCD + VIDCON0) & ~1);
		gpio_pin_output(LCD_GPIO_PIXEL_CLOCK_ENABLE, 0);
		SET_REG(LCD + LCD_CON, 1);
	}
}

static void resetLCD() {
	gpio_pin_output(LCD_GPIO_RESET, 1);
	udelay(10000);
	gpio_pin_output(LCD_GPIO_RESET, 0);
}

static void setCommandMode(OnOff swt) {
	if(swt) {
		pmu_write_reg(LCD_I2C_BUS, LCD_I2C_COMMAND, LCD_I2C_COMMANDMODE_ON, 0);
	} else {
		pmu_write_reg(LCD_I2C_BUS, LCD_I2C_COMMAND, LCD_I2C_COMMANDMODE_OFF, 0);
	}
}

static void enterRegisterMode() {
	int tries = 0;
	int status;

	do {
		transmitShortCommandOnSPI1(0xDE);
		status = getPanelRegister(0x15);
		tries += 1;

		if(tries >= 20000) {
			bufferPrintf("enter register mode timeout %x\r\n", status);
			break;
		}
	} while((status & 0x1) != 1);
}

static void transmitCommandOnSPI0(int command, int subcommand) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = command;
	lcdCommand[1] = subcommand;

	gpio_custom_io(LCD_GPIO_CONTROL_ENABLE, 0x2 | 1);
	gpio_pin_output(GPIO_SPI0_CS0, 0);
	spi_tx(0, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(GPIO_SPI0_CS0, 1);
	gpio_custom_io(LCD_GPIO_CONTROL_ENABLE, 0x2 | 0);

}

static void transmitCommandOnSPI1(int command, int subcommand) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = command;
	lcdCommand[1] = subcommand;

	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);
}

static void setPanelRegister(int reg, int value) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = reg & 0x7F;
	lcdCommand[1] = value;

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

static int getPanelRegister(int reg) {
	uint8_t lcdCommand[1];
	uint8_t buffer[1];
	lcdCommand[0] = 0x80 | reg;

	gpio_pin_output(GPIO_SPI1_CS0, 0);
	spi_tx(1, lcdCommand, 1, TRUE, 0);
	spi_rx(1, buffer, 1, TRUE, 0);
	gpio_pin_output(GPIO_SPI1_CS0, 1);

	return buffer[0];
}

void syrah_quiesce() {
	bufferPrintf("syrah_quiesce()\r\n");
	spi_set_baud(1, 1000000, SPIOption13Setting0, 1, 1, 1);
	spi_set_baud(0, 500000, SPIOption13Setting0, 1, 0, 0);
	enterRegisterMode();
	udelay(1000);
	setPanelRegister(0x55, 0x2);
	udelay(40000);
	setPanelRegister(0x7A, 0xDF);
	setPanelRegister(0x7B, 0x1);
	setPanelRegister(0x7B, 0x1);
	transmitShortCommandOnSPI1(0x10);
	udelay(50000);
	setPanelRegister(0x7B, 0x0);
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
	volatile uint32_t* line;

	fill = fill & 0xffffff;	// no alpha
	line = &framebuffer->buffer[line_no * framebuffer->lineWidth];

	int stop = start + length;
	for(i = start; i < stop; i++) {
		line[i] = fill;
	}
}

static void vline_rgb888(Framebuffer* framebuffer, int start, int line_no, int length, int fill) {
	int i;
	volatile uint32_t* line;

	fill = fill & 0xffffff;	// no alpha
	line = &framebuffer->buffer[line_no];

	int stop = start + length;
	for(i = start; i < stop; i++) {
		line[i * framebuffer->lineWidth] = fill;
	}
}

void lcd_set_backlight_level(int level) {
	if(level == 0) {
		pmu_write_regs(&backlightOffData, 1);
	} else { 
		PMURegisterData myBacklightData[sizeof(backlightData)/sizeof(PMURegisterData)];

		memcpy(myBacklightData, backlightData, sizeof(myBacklightData));

		if(level <= LCD_MAX_BACKLIGHT) {
			int i;
			for(i = 0; i < (sizeof(myBacklightData)/sizeof(PMURegisterData)); i++) {
				if(myBacklightData[i].reg == LCD_BACKLIGHT_REG) {
					myBacklightData[i].data = level & LCD_BACKLIGHT_REGMASK;
				}
			}
		}
		pmu_write_regs(myBacklightData, sizeof(myBacklightData)/sizeof(PMURegisterData));
	}
}
