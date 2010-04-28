#ifndef SMALL
#ifndef NO_STBIMAGE

#include "openiboot.h"
#include "lcd.h"
#include "util.h"
#include "framebuffer.h"
#include "buttons.h"
#include "timer.h"
#include "images/ConsolePNG.h"
#include "images/iPhoneOSPNG.h"
#include "images/ConsoleSelectedPNG.h"
#include "images/iPhoneOSSelectedPNG.h"
#include "images/HeaderPNG.h"
#include "images.h"
#include "actions.h"
#include "stb_image.h"
#include "pmu.h"
#include "nand.h"
#include "radio.h"
#include "hfs/fs.h"
#include "ftl.h"
#include "scripting.h"

int globalFtlHasBeenRestored = 0; /* global variable to tell wether a ftl_restore has been done*/

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
		framebuffer_draw_image(imgiPhoneOSSelected, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
	}

	if(Selection == MenuSelectionConsole) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsoleSelected, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
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

int menu_setup(int timeout) {
	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;	

	imgiPhoneOS = framebuffer_load_image(dataiPhoneOSPNG, dataiPhoneOSPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);
	imgiPhoneOSSelected = framebuffer_load_image(dataiPhoneOSSelectedPNG, dataiPhoneOSSelectedPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);
	imgConsole = framebuffer_load_image(dataConsolePNG, dataConsolePNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	imgConsoleSelected = framebuffer_load_image(dataConsoleSelectedPNG, dataConsoleSelectedPNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	imgHeader = framebuffer_load_image(dataHeaderPNG, dataHeaderPNG_size, &imgHeaderWidth, &imgHeaderHeight, TRUE);

	bufferPrintf("menu: images loaded\r\n");

	imgiPhoneOSX = (FBWidth - imgiPhoneOSWidth) / 2;
	imgiPhoneOSY = 106;

	imgConsoleX = (FBWidth - imgConsoleWidth) / 2;
	imgConsoleY = 246;

	imgHeaderX = (FBWidth - imgHeaderWidth) / 2;
	imgHeaderY = 30;

	framebuffer_draw_image(imgHeader, imgHeaderX, imgHeaderY, imgHeaderWidth, imgHeaderHeight);
	framebuffer_draw_rect_hgradient(0, 90, 0, 378, FBWidth, (FBHeight - 12) - 378);
	framebuffer_draw_rect_hgradient(0x22, 0x22, 0, FBHeight - 12, FBWidth, 12);

	framebuffer_setloc(0, 47);
	framebuffer_setcolors(COLOR_WHITE, 0x222222);
	framebuffer_print_force(OPENIBOOT_VERSION_STR);
	framebuffer_setcolors(COLOR_WHITE, COLOR_BLACK);
	framebuffer_setloc(0, 0);

	Selection = MenuSelectioniPhoneOS;

	drawSelectionBox();

	pmu_set_iboot_stage(0);

	uint64_t startTime = timer_get_system_microtime();
	while(TRUE) {
		if(buttons_is_pushed(BUTTONS_HOLD)) {
			toggle();
			startTime = timer_get_system_microtime();
			udelay(200000);
		}
#ifndef CONFIG_IPOD
		if(!buttons_is_pushed(BUTTONS_VOLUP)) {
			Selection = MenuSelectioniPhoneOS;

			drawSelectionBox();
			startTime = timer_get_system_microtime();
			udelay(200000);
		}
		if(!buttons_is_pushed(BUTTONS_VOLDOWN)) {
			Selection = MenuSelectionConsole;

			drawSelectionBox();
			startTime = timer_get_system_microtime();
			udelay(200000);
		}
#endif
		if(buttons_is_pushed(BUTTONS_HOME)) {
			break;
		}
		if(timeout > 0 && has_elapsed(startTime, (uint64_t)timeout * 1000)) {
			bufferPrintf("menu: timed out, selecting current item\r\n");
			break;
		}
		udelay(10000);
	}

	if(Selection == MenuSelectioniPhoneOS) {
		Image* image = images_get(fourcc("ibox"));
		if(image == NULL)
			image = images_get(fourcc("ibot"));
		void* imageData;
		images_read(image, &imageData);
		chainload((uint32_t)imageData);
	}

	if(Selection == MenuSelectionConsole) {
#ifndef NO_HFS
		startTime = timer_get_system_microtime();
		while(TRUE) {
			if(!buttons_is_pushed(BUTTONS_HOME))
				break;

			if(has_elapsed(startTime, (uint64_t)2000 * 1000)) {
				framebuffer_setdisplaytext(TRUE);
				framebuffer_clear();
				radio_setup();
				nand_setup();
				fs_setup();
				if(globalFtlHasBeenRestored) /* if ftl has been restored, sync it, so kernel doesn't have to do a ftl_restore again */
				{
					if(ftl_sync())
					{
						bufferPrintf("ftl synced successfully");
					}
					else
					{
						bufferPrintf("error syncing ftl");
					}
				}

				pmu_set_iboot_stage(0);
				startScripting("linux"); //start script mode if there is a script file
				boot_linux_from_files();
			}

			udelay(10000);
		}
#endif

		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();
	}

	return 0;
}

#endif
#endif
