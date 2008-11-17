#include "openiboot.h"
#include "lcd.h"
#include "util.h"
#include "framebuffer.h"
#include "buttons.h"
#include "timer.h"
#include "images/ConsolePNG.h"
#include "images/iPhoneOSPNG.h"
#include "images/HeaderPNG.h"
#include "images/SelectionPNG.h"
#include "images.h"
#include "actions.h"

static uint32_t FBWidth;
static uint32_t FBHeight;


static uint32_t* imgiPhoneOS;
static int imgiPhoneOSWidth;
static int imgiPhoneOSHeight;
static int imgiPhoneOSX;
static int imgiPhoneOSY;

static uint32_t* imgConsole;
static int imgConsoleWidth;
static int imgConsoleHeight;
static int imgConsoleX;
static int imgConsoleY;

static uint32_t* imgiPhoneOSSelected;
static uint32_t* imgConsoleSelected;
static int imgSelectionWidth;
static int imgSelectionHeight;

static uint32_t* imgHeader;
static int imgHeaderWidth;
static int imgHeaderHeight;
static int imgHeaderX;
static int imgHeaderY;

typedef enum MenuSelection {
	MenuSelectioniPhoneOS,
	MenuSelectionConsole
} MenuSelection;

static MenuSelection Selection;

static void drawSelectionBox() {
	if(Selection == MenuSelectioniPhoneOS) {
		framebuffer_draw_image(imgiPhoneOSSelected, imgiPhoneOSX, imgiPhoneOSY, imgSelectionWidth, imgSelectionHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgSelectionWidth, imgSelectionHeight);
	}

	if(Selection == MenuSelectionConsole) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgSelectionWidth, imgSelectionHeight);
		framebuffer_draw_image(imgConsoleSelected, imgConsoleX, imgConsoleY, imgSelectionWidth, imgSelectionHeight);
	}
}

static void toggle() {
	if(Selection == MenuSelectioniPhoneOS) {
		Selection = MenuSelectionConsole;
	} else if(Selection == MenuSelectionConsole) {
		Selection = MenuSelectioniPhoneOS;
	}
	drawSelectionBox();
}
int menu_setup() {
	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;	

	uint32_t* rawImgiPhoneOS = framebuffer_load_image(dataiPhoneOSPNG, dataiPhoneOSPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);
	uint32_t* rawImgConsole = framebuffer_load_image(dataConsolePNG, dataConsolePNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	uint32_t* rawImgHeader = framebuffer_load_image(dataHeaderPNG, dataHeaderPNG_size, &imgHeaderWidth, &imgHeaderHeight, TRUE);
	uint32_t* imgSelection = framebuffer_load_image(dataSelectionPNG, dataSelectionPNG_size, &imgSelectionWidth, &imgSelectionHeight, TRUE);

	bufferPrintf("menu: images loaded\r\n");

	imgHeader = malloc(sizeof(uint32_t) * imgHeaderWidth * imgHeaderHeight);
	memset(imgHeader, 0, sizeof(uint32_t) * imgHeaderWidth * imgHeaderHeight);
	framebuffer_blend_image(imgHeader, imgHeaderWidth, imgHeaderHeight, rawImgHeader, imgHeaderWidth, imgHeaderHeight, 0, 0);
	free(rawImgHeader);

	imgiPhoneOSSelected = malloc(sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	memset(imgiPhoneOSSelected, 0, sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	framebuffer_blend_image(imgiPhoneOSSelected, imgSelectionWidth, imgSelectionHeight, imgSelection, imgSelectionWidth, imgSelectionHeight, 0, 0);
	imgConsoleSelected = malloc(sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	memcpy(imgConsoleSelected, imgiPhoneOSSelected, sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	free(imgSelection);

	imgiPhoneOS = malloc(sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	memset(imgiPhoneOS, 0, sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	imgConsole = malloc(sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);
	memset(imgConsole, 0, sizeof(uint32_t) * imgSelectionWidth * imgSelectionHeight);

	imgHeaderX = (FBWidth - imgHeaderWidth) / 2;
	imgHeaderY = 30;

	imgiPhoneOSX = (FBWidth - imgSelectionWidth) / 2;
	imgiPhoneOSY = 106;

	imgConsoleX = (FBWidth - imgSelectionWidth) / 2;
	imgConsoleY = 246;

	int imgiPhoneOSXWithinSelection = (imgSelectionWidth - imgiPhoneOSWidth) / 2;
	int imgiPhoneOSYWithinSelection = (imgSelectionHeight - imgiPhoneOSHeight) / 2;
	int imgConsoleXWithinSelection = (imgSelectionWidth - imgConsoleWidth) / 2;
	int imgConsoleYWithinSelection = (imgSelectionHeight - imgConsoleHeight) / 2;

	framebuffer_blend_image(imgiPhoneOS, imgSelectionWidth, imgSelectionHeight, rawImgiPhoneOS, imgiPhoneOSWidth, imgiPhoneOSHeight, imgiPhoneOSXWithinSelection, imgiPhoneOSYWithinSelection);
	framebuffer_blend_image(imgiPhoneOSSelected, imgSelectionWidth, imgSelectionHeight, rawImgiPhoneOS, imgiPhoneOSWidth, imgiPhoneOSHeight, imgiPhoneOSXWithinSelection, imgiPhoneOSYWithinSelection);

	framebuffer_blend_image(imgConsole, imgSelectionWidth, imgSelectionHeight, rawImgConsole, imgConsoleWidth, imgConsoleHeight, imgConsoleXWithinSelection, imgConsoleYWithinSelection);
	framebuffer_blend_image(imgConsoleSelected, imgSelectionWidth, imgSelectionHeight, rawImgConsole, imgConsoleWidth, imgConsoleHeight, imgConsoleXWithinSelection, imgConsoleYWithinSelection);

	free(rawImgiPhoneOS);
	free(rawImgConsole);

	bufferPrintf("menu: images prepared\r\n");
	framebuffer_draw_image(imgHeader, imgHeaderX, imgHeaderY, imgHeaderWidth, imgHeaderHeight);
	framebuffer_draw_rect_hgradient(0, 43, 0, 360, FBWidth, (FBHeight - 20) - 360);
	framebuffer_draw_rect(0x222222, 0, FBHeight - 20, FBWidth, 20);

	Selection = MenuSelectioniPhoneOS;

	drawSelectionBox();

	while(TRUE) {
		if(buttons_is_hold_pushed()) {
			toggle();
			udelay(200000);
		}
		if(buttons_is_home_pushed()) {
			break;
		}
		udelay(10000);
	}

	if(Selection == MenuSelectioniPhoneOS) {
		Image* image = images_get(fourcc("ibox"));
		void* imageData;
		images_read(image, &imageData);
		chainload((uint32_t)imageData);
	}

	if(Selection == MenuSelectionConsole) {
		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();
	}

	return 0;
}

