#include "openiboot.h"
#include "lcd.h"
#include "power.h"
#include "clock.h"
#include "gpio.h"
#include "hardware/lcd.h"
#include "hardware/power.h"
#include "util.h"
#include "spi.h"
#include "hardware/spi.h"
#include "i2c.h"
#include "timer.h"
#include "pmu.h"
#include "nvram.h"

static int lcd_has_init = FALSE;
static int lcd_init_attempted = FALSE;

const static LCDInfo optC = {FrequencyBaseDisplay, 10800000, 320, 15, 15, 16, 480, 4, 4, 4, 1, 0, 0, 0, 0};
static LCDInfo curTimings;

Window* currentWindow;

volatile uint32_t* CurFramebuffer;

static int numWindows = 0;
uint32_t NextFramebuffer = 0;

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
	{
		{0xB300, 0xFF70,
			{{0x34328EB6, 0x34003400, 0x34003400, 0xD000D00, 0x3400D00, 0x71DC771D, 0x340D37, 0xD0, 0x1C007000, 0x1C1C070, 0xC771C707, 0xDDC7771D, 0xDC7771DD, 0xDDDDDDDD, 0x7477774, 0x70001C00, 0xE9C, 0}},
			{{0x343400F6, 0x434D0D0D, 0xD0D0D343, 0x34343434, 0x434D0D0D, 0x34000D03, 0x7000, 0x1C070070, 0xC0700700, 0x71C1C1C1, 0xC7071C1C, 0xDC771C71, 0xC71DC71D, 0xDDDC771D, 0x71D71D, 0xF701C01C, 0, 0}},
			{{0x74DDD0F6, 0xDD377777, 0x7774DDDD, 0xDDDDDD37, 0x77777774, 0x4034DDD3, 0xD00003, 0x70070000, 0x1C070700, 0x7771C707, 0xDDDC7777, 0xDD377774, 0x434D3434, 0x3434343, 0x1D010D, 0xF701C01C, 0, 0}}
		},
		{0xD100, 0xFF70,
			{{0x34328EB6, 0x34003400, 0x34003400, 0xD000D00, 0x3400D00, 0x71DC771D, 0x340D37, 0xD0, 0x1C007000, 0x1C1C070, 0xC771C707, 0xDDC7771D, 0xDC7771DD, 0xDDDDDDDD, 0x7477774, 0x70001C00, 0xE9C, 0}},
			{{0x343400F6, 0x434D0D0D, 0xD0D0D343, 0x34343434, 0x434D0D0D, 0x34000D03, 0x7000, 0x1C070070, 0xC0700700, 0x71C1C1C1, 0xC7071C1C, 0xDC771C71, 0xC71DC71D, 0xDDDC771D, 0x71D71D, 0xF701C01C, 0, 0}},
			{{0x74DDD0F6, 0xDD377777, 0x7774DDDD, 0xDDDDDD37, 0x77777774, 0x4034DDD3, 0xD00003, 0x70070000, 0x1C070700, 0x7771C707, 0xDDDC7777, 0xDD377774, 0x434D3434, 0x3434343, 0x1D010D, 0xF701C01C, 0, 0}}
		},
		{0xC200, 0xFF70,
			{{0xDDD37F2, 0x1DDDD34D, 0x4D341C, 0x40D1C4D0, 0x3400D3, 0x1C000000, 0xDDDDDC1C, 0xD34374, 0x3400D, 0, 0x70000000, 0, 0, 0, 0, 0x1C034, 0, 0}},
			{{0x774DCA98, 0x1C1C, 0x1C1DD34, 0x77774340, 0x70701C1C, 0xDDDC71C0, 0x3434DDD, 0xD, 0x1C000, 0xC07001C0, 0x701C01, 0x1C007, 0xD00035C, 0x40D0D034, 0xD37434D3, 0xD34DD374, 0xD, 0}},
			{{0x37774A98, 0x74DD374D, 0x34D374D3, 0x34D3434D, 0x74D34D34, 0xDD374DD3, 0xC771DDDD, 0x7071C1D, 0xD374D347, 0x34D34374, 0xD343434, 0x34D3434D, 0xDDDD374D, 0x71C771D, 0x7007, 0x340D00D0, 0, 0}}
		},
		{0xE510, 0xFF70,
			{{0xDDDDD0F2, 0xDDDDDDDD, 0xDDDDDDDD, 0x774DDDDD, 0x70777777, 0x701C00, 0x74007000, 0xDDC71C70, 0x71DDD374, 0x340DD377, 0xD0D340D0, 0x34D0DD0, 0xC771DD0D, 0x34DDDC71, 0x71DC770C, 0x72945DC7, 0, 0}},
			{{0x771C74F2, 0xC771DC77, 0x71DC771D, 0xC771DC77, 0x1C7771D, 0x1C0, 0x340000, 0x34000, 0x743401C0, 0x3434D3, 0x774D0000, 0x74DDDDD3, 0x71D37, 0x3000D000, 0x1C0037, 0x770D0440, 3, 0}},
			{{0xDDDC74F2, 0xDDC7771D, 0x1DDC7771, 0x1DDC7777, 0x1DC777, 0, 0x77400000, 0xDD374C77, 0x71C774D0, 0x34DDC, 0xDDDD340D, 0xD34D34DD, 0x707071DD, 0xCDDC770, 0x34D34D37, 0x45474D, 0, 0}}
		},
		{0xD110, 0xFF70,
			{{0xDDDDD0F2, 0x77777774, 0xDDDDDDD3, 0xD3777774, 0x1DDDDD, 0x1E903434, 0x7771DDC, 0x377774, 0x74D34DD3, 0xD3407007, 0x34374D, 0x34D34D0D, 0x1DC70D0D, 0xDD34D0D, 0x77000000, 0x42B41D07, 4, 0}},
			{{0xD374D0F2, 0x74DD374D, 0xD374DDD3, 0x74DD374D, 0x40774DD3, 0x1F434DD3, 0x4071C71C, 0xDC0DDDC7, 0x1DD374DD, 0x77740000, 0x4D034377, 0xD34DD37, 0xD01DC00D, 0x1C034D34, 0x44501C1C, 0x37770FDC, 0, 0}},
			{{0x4DC774F2, 0x7771DDDC, 0x1DDDDC77, 0xDDC77777, 0xCAC771DD, 0x700D34D0, 0xD007771C, 0x7DC000CA, 0x5C1C7070, 0x74D0D03, 0xD34DDDC, 0x3434D, 0xC7700000, 0x3707001, 0x2B400D00, 0xE8E8D528, 0xD300DC, 0}}
		},
		{0xC210, 0xFF70,
			{{0xC1C771F2, 0x71C7071, 0x7071C1C7, 0x71C7071C, 0x75C1C70, 0xD0D0000, 0xDC, 0, 0x400D0000, 0xC0000003, 0x77777701, 0x777771C7, 0x3400077, 0x4D31C1E9, 0x4D447777, 0x7404C3C3, 0x30, 0}},
			{{0x1C01F2, 0x70007, 0xC001C007, 0x1C007001, 0x1D001C00, 0x701C001C, 0x1C01C003, 0xC1C0701C, 0x70001, 0x1C001C00, 0x1DDDDDDC, 0xDDDDDC77, 0x4D340771, 0x71C07003, 0x71D44070, 0x30F, 3, 0}},
			{{0x1DC1C4F2, 0xC71C71C7, 0x71C71C71, 0x1C71C71C, 0x4D7071C7, 0x374DDDC7, 0xD3774DDD, 0x43434D34, 0x4DD374D3, 0xD34D34D3, 0x70070000, 0x7071C1C0, 0x74DDD000, 0x70000003, 0xC1C1C70C, 0x14284071, 0x37, 0}}
		},
		{0xC223, 0xFF77,
			{{0x1DC702A6, 0xC771DC77, 0xDD7471DD, 0x1C01CD, 0x77777774, 0x377000D0, 7, 0, 0, 0xC7007000, 0x1C1C7071, 0xC0707070, 0x1C707001, 0x1DD34D00, 0x401DC707, 0x1374D3, 0x30, 0}},
			{{0x34D3A4A6, 0x434D0D0D, 0x1C74D0D3, 0x1C71C7, 0x1DD3774, 0xDC000D, 0x434000D0, 0xD0343403, 0x340D0, 0, 0x700000, 0x7001C, 0x34340000, 0x4D0D00D0, 0x700D0343, 0x37774000, 0x300, 0}},
				{{0x340C3A6, 0x400D0034, 0x35DD003, 0x1D000000, 0xDD0001C7, 0xD000DF4D, 0x34034000, 0xD0340D00, 0x1C000000, 0x1C1C0700, 0xC71DC71C, 0x1DC71C71, 0x707777, 0xC0070000, 0x70701C01, 0x15CAC070, 0x31D, 0}}
		},
		{0xC220, 0xFF77,
			{{0x340D1CF6, 0xD00D0340, 0x40D00D00, 0x3403403, 0x7740D00D, 0xDDDDDC, 0x1C0701C0, 0x71C71C7, 0x1C07001C, 0x4DD37777, 0xC1C771D3, 0xDDDDDC71, 0x7007774, 0x1C01C000, 0x34010070, 0x700, 3, 0}},
			{{0x3774D0F6, 0x4DD3774D, 0x374DD377, 0xDD3774DD, 0xD71D3774, 0x37340, 0xC0007000, 0x1C01, 0x71C070D0, 0x7071DC7, 0xC70701C0, 0x1DC71D, 0, 0xC7070070, 0x1D37771D, 0x34371C7, 0, 0}},
			{{0x137134F6, 0x77777777, 0x77777777, 0xDDDDDDDC, 0xD01DDDDD, 0x70003034, 0x1C1C0700, 0x7071C, 0x377701C0, 0x77774DDD, 0xDDDDDDC7, 0xD34D374D, 0x70701C1, 0x701C0707, 0x1C034C, 0x444D0, 0, 0}}
		},
		{0xD123, 0xFF77,
			{{0x1C71C4F2, 0x71C71C77, 0xDC71C71C, 0x71C71C71, 0x1C71DC, 0x340740D0, 0x1C707000, 0x40D03777, 0x4DDD3743, 0x71C1C073, 0xC1C701C0, 0xDDDDDD01, 0xDDDDDDC0, 0x701C034, 0x4D01C707, 0xA0AD34D3, 0x38A48, 0}},
			{{0x771C74F2, 0x1DC771DC, 0xDC771C77, 0x771DC771, 0xC1C771DC, 0x1D1C1C1, 0x77770700, 0xD00D3777, 0xDDDDD340, 0xC701C034, 0x1C000701, 0xC7777400, 0xC771C00D, 0xD34DDDDD, 0x74D0, 0x728101C, 0x30, 0}},
			{{0x1C71C4F2, 0xC71C71C7, 0x71C71C71, 0x1C71C71C, 0x71C71C7, 0x740070, 0xDDC7070D, 0xD0D0DDD, 0xD3434D0, 0xC7771C00, 0x700701DD, 0x74DD0000, 0xDDDDC003, 0xD0DDD, 0x1D000000, 0x8A5071C7, 0x10E2, 0}}
		},
		{0xE522, 0xFF77,
			{{0x328EB6, 0xD00000, 0xD000000, 0x40000000, 3, 0x771C7000, 0xC77401DC, 0x34D37771, 0x77771D34, 0x340037, 0x4D34D340, 0x40000003, 0xC0701C13, 0xC00340D, 0x34DD34D3, 0x135D774D, 0xD0, 0}},
			{{0x1C071F2, 0x1C07007, 0xC0701C07, 0x701C0701, 0x701C0, 0x70, 0x1C707000, 0xC771DC77, 0x1C1DC71, 0x1C1C0707, 0x7071DC7, 0xD1C000D0, 0xC4000000, 0x7001C1C1, 0x340DC00, 0x73DD1100, 3, 0}},
			{{0xC07071F2, 0x70701C1, 0xC070701C, 0x70701C1, 0x701C1C, 0xC707001C, 0x1C71, 0x1C000000, 0xD00D7000, 0x1C000000, 0xD007070, 0x7000D00, 0x7135C070, 0x7771C71C, 0xC037, 0xA4A11C00, 0x10, 0}}
		}
	};

static const PMURegisterData backlightOffData = {0x29, 0x0};

static const PMURegisterData backlightData[] = {
	{0x17, 0x1},
	{0x2A, 0x0},
	{0x28, 0x22},
	{0x29, 0x1},
	{0x2A, 0x6}
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
static void setWindow(int window, int wordSetting, int zero1, ColorSpace colorSpace, int width1, uint32_t framebuffer, int zero2, int width2, int zero3, int height);
static void setLayer(int window, int zero0, int zero1);

static void framebuffer_fill(Framebuffer* framebuffer, int x, int y, int width, int height, int fill);
static void hline_rgb888(Framebuffer* framebuffer, int start, int line_no, int length, int fill);
static void hline_rgb565(Framebuffer* framebuffer, int start, int line_no, int length, int fill);
static void vline_rgb888(Framebuffer* framebuffer, int start, int line_no, int length, int fill);
static void vline_rgb565(Framebuffer* framebuffer, int start, int line_no, int length, int fill);

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

static void syrah_quiesce();

int lcd_setup() {
	int backlightLevel = 0;

	NextFramebuffer = 0x0fd00000;
	numWindows = 2;

	if(!lcd_has_init) {
		if(!lcd_init_attempted) {
			if(initDisplay() == 0) {
				const char* envBL = nvram_getvar("backlight-level");
				if(envBL) {
					backlightLevel = parseNumber(envBL);
				}

				if(backlightLevel == 0)
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

void lcd_shutdown() {
	lcd_fill(0x000000);
	udelay(40000);
	lcd_set_backlight_level(0);
	lcd_fill(0xFFFFFF);
	udelay(40000);
	syrah_quiesce();
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

	currentWindow = createWindow(0, 0, info->width, info->height, RGB565);
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

	int i;
	for(i = 0; i < (sizeof(gammaTables)/sizeof(GammaTableDescriptor)); i++) {
		if((curTable->panelIDMask & panelID) == curTable->panelIDMatch) {
			bufferPrintf("Installing gamma table 0x%08x / 0x%08x\r\n", curTable->panelIDMatch, curTable->panelIDMask);
			installGammaTable(0, (uint8_t*) curTable->table0.data);
			installGammaTable(1, (uint8_t*) curTable->table1.data);
			installGammaTable(2, (uint8_t*) curTable->table2.data);
			return;
		}
		curTable++;
	}
	bufferPrintf("No matching gamma table found in %d tables\r\n", (sizeof(gammaTables)/sizeof(GammaTableDescriptor)));

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

	uint32_t currentFramebuffer = NextFramebuffer;

	NextFramebuffer = (currentFramebuffer
		+ (lineBytes * height)	// size we need
		+ 0xFFF)		// round up
		& 0xFFFFF000;		// align

	createFramebuffer(&newWindow->framebuffer, currentFramebuffer, width, height, lineWidth, colorSpace);

	if(colorSpace == RGB565)
		setWindow(currentWindowNo, 1, 0, colorSpace, width, currentFramebuffer, 0, width, 0, height);
	else
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
	if(colorSpace == RGB888)
	{
		framebuffer->hline = hline_rgb888;
		framebuffer->vline = vline_rgb888;
	} else
	{
		framebuffer->hline = hline_rgb565;
		framebuffer->vline = vline_rgb565;
	}
}

static void setWindow(int window, int wordSetting, int zero1, ColorSpace colorSpace, int width1, uint32_t framebuffer, int zero2, int width2, int zero3, int height) {
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

	uint32_t sfn = ((stride & 0x7) << 24) | ((wordSetting & 0x7) << 16) | ((zero1 & 0x3) << 12) | ((colorSpace & 0x7) << 8) | ((zero2 * nibblesPerPixel) & 0x7);
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

void lcd_window_address(int window, uint32_t framebuffer) {
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

	SET_REG(windowBase + 8, framebuffer);
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

#ifdef CONFIG_3G
	gpio_pin_output(LCD_GPIO_3G_ENABLE, 0);
#endif

	gpio_pin_output(LCD_GPIO_MPL_RX_ENABLE, 0);
	gpio_pin_output(LCD_GPIO_POWER_ENABLE, 0);
	resetLCD();

	gpio_pin_output(LCD_GPIO_POWER_ENABLE, 1);
	udelay(10000);

#ifdef CONFIG_IPOD
	togglePixelClock(ON);
	transmitCommandOnSPI1(0x6D, 0x0);
	transmitCommandOnSPI1(0x36, 0x8);
	udelay(15000);
#endif
#ifdef CONFIG_3G
	togglePixelClock(ON);
	transmitCommandOnSPI1(0x6D, 0x0);
#endif
#ifdef CONFIG_IPHONE
	transmitCommandOnSPI1(0x6D, 0x40);
#endif

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

#ifndef CONFIG_IPOD
	togglePixelClock(ON);
	udelay(40000);
#endif

	setPanelRegister(0x6D, 0x0);
	transmitCommandOnSPI1(0x36, 0x8);
	udelay(30000);

	uint8_t lcdCommand[2];
	uint8_t panelID[3];

	memset(panelID, 0, 3);

	lcdCommand[0] = 0xDA;
	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 1, TRUE, 0);
	spi_rx(LCD_PANEL_SPI, &panelID[0], 1, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);

	lcdCommand[0] = 0xDB;
	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 1, TRUE, 0);
	spi_rx(LCD_PANEL_SPI, &panelID[1], 1, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);

	lcdCommand[0] = 0xDC;
	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 1, TRUE, 0);
	spi_rx(LCD_PANEL_SPI, &panelID[2], 1, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);

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
		bufferPrintf("Autoboot display...\r\n");

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
		} else {
			LCDInitRegisterCount = 0;
		}
	}

	LCDPanelID = (panelID[0] << 16) | (panelID[1] << 8) | panelID[2];

	if(LCDInitRegisterCount > 0) {
		bufferPrintf("Writing LCD init registers...\r\n");
		int i;
		for(i = 0; i < LCDInitRegisterCount; i++) {
			uint8_t* regInfo = (uint8_t*) &LCDInitRegisters[i];
			setPanelRegister(regInfo[0], regInfo[1]);
		}
	}

	switch(panelID[2] & 0x7) {
		case 0:
			bufferPrintf("Do init for Merlot\r\n");
			setPanelRegister(0x2E, getPanelRegister(0x2E) & 0x7F);
			break;
		case 2:
			bufferPrintf("turning on parity error flag\r\n");
			setPanelRegister(0x53, 0x5);
			setPanelRegister(0xB, 0x10);
			break;
		case 3:
			bufferPrintf("changing to continuous calibration mode 3\r\n");
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
	} else if((panelID[2] & 0x70) == 0x20) {
		bufferPrintf("N82/");
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

	if((panelID[2] & (1 << 3)) != 0)
		bufferPrintf("AutoBoot Enabled ");

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

#ifdef CONFIG_3G
static int detect_lcd_gpio_config() {
	static int hasDetected = FALSE;
	static int detectedConfig = 0;

	if(hasDetected) {
		return detectedConfig;
	}

	detectedConfig = (gpio_pin_state(LCD_GPIO_CONFIG3) ? 1 : 0) | ((gpio_pin_state(LCD_GPIO_CONFIG2) ? 1 : 0) << 1) | ((gpio_pin_state(LCD_GPIO_CONFIG1) ? 1 : 0) << 2);
	hasDetected = TRUE;
	return detectedConfig;
}
#endif

static void resetLCD() {
	int altResetDirection = FALSE;

#ifdef CONFIG_3G
	if(detect_lcd_gpio_config() < 3)
		altResetDirection = TRUE;
#endif

#ifdef CONFIG_IPOD
	altResetDirection = TRUE;
#endif

	if(altResetDirection) {
		gpio_pin_output(LCD_GPIO_RESET, 0);
		udelay(10000);
		gpio_pin_output(LCD_GPIO_RESET, 1);
	} else {
		gpio_pin_output(LCD_GPIO_RESET, 1);
		udelay(10000);
		gpio_pin_output(LCD_GPIO_RESET, 0);
	}
}

static void setCommandMode(OnOff swt) {
	if(swt) {
		pmu_write_reg(LCD_I2C_COMMAND, LCD_I2C_COMMANDMODE_ON, 0);
	} else {
		pmu_write_reg(LCD_I2C_COMMAND, LCD_I2C_COMMANDMODE_OFF, 0);
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
	gpio_pin_output(LCD_CS, 0);
	spi_tx(LCD_SPI, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(LCD_CS, 1);
	gpio_custom_io(LCD_GPIO_CONTROL_ENABLE, 0x2 | 0);

}

static void transmitCommandOnSPI1(int command, int subcommand) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = command;
	lcdCommand[1] = subcommand;

	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);
}

static void setPanelRegister(int reg, int value) {
	uint8_t lcdCommand[2];
	lcdCommand[0] = reg & 0x7F;
	lcdCommand[1] = value;

	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 2, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);
}


static void transmitShortCommandOnSPI1(int command) {
	uint8_t lcdCommand[1];
	lcdCommand[0] = command;

	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 1, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);
}

static int getPanelRegister(int reg) {
	uint8_t lcdCommand[1];
	uint8_t buffer[1];
	lcdCommand[0] = 0x80 | reg;

	gpio_pin_output(LCD_PANEL_CS, 0);
	spi_tx(LCD_PANEL_SPI, lcdCommand, 1, TRUE, 0);
	spi_rx(LCD_PANEL_SPI, buffer, 1, TRUE, 0);
	gpio_pin_output(LCD_PANEL_CS, 1);

	return buffer[0];
}

static void syrah_quiesce() {
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

static void hline_rgb565(Framebuffer* framebuffer, int start, int line_no, int length, int fill) {
	int i;
	volatile uint16_t* line;
	uint16_t fill565;

	fill565= ((((fill >> 16) & 0xFF) >> 3) << 11) | ((((fill >> 8) & 0xFF) >> 2) << 5) | ((fill & 0xFF) >> 3);
	line = &((uint16_t*)framebuffer->buffer)[line_no * framebuffer->lineWidth];

	int stop = start + length;
	for(i = start; i < stop; i++) {
		line[i] = fill565;
	}
}

static void vline_rgb565(Framebuffer* framebuffer, int start, int line_no, int length, int fill) {
	int i;
	volatile uint16_t* line;
	uint16_t fill565;

	fill565= ((((fill >> 16) & 0xFF) >> 3) << 11) | ((((fill >> 8) & 0xFF) >> 2) << 5) | ((fill & 0xFF) >> 3);
	line = &((uint16_t*)framebuffer->buffer)[line_no];

	int stop = start + length;
	for(i = start; i < stop; i++) {
		line[i * framebuffer->lineWidth] = fill565;
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
