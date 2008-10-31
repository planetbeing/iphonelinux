#include "openiboot.h"
#include "lcd.h"
#include "util.h"
#include "framebuffer.h"
#include "buttons.h"
#include "timer.h"
#include "images/console.h"
#include "images/iphoneos.h"
#include "images.h"
#include "actions.h"

static uint32_t FBWidth;
static uint32_t FBHeight;

static uint32_t* imgiPhoneOS;
static uint32_t* imgConsole;

typedef enum MenuSelection {
	MenuSelectioniPhoneOS,
	MenuSelectionConsole
} MenuSelection;

static MenuSelection Selection;

static uint32_t* loadRGBImage(char* image_data, int width, int height) {
	uint32_t* image = (uint32_t*) malloc(width * height * 4);
	uint8_t pixel[3];

	int i;
	for(i = 0; i < (width * height); i++) {
		HEADER_PIXEL(image_data, pixel);
		image[i] = pixel[0] << 16 | pixel[1] << 8 | pixel[2];
	}

	return image;
}

static void drawSelectionBox() {
	if(Selection == MenuSelectioniPhoneOS) {
		framebuffer_draw_rect(COLOR_WHITE, 20, 140, console_width + 20, console_height + 20);
		framebuffer_draw_rect(COLOR_BLACK, 20, 140 + iphoneos_height + 40, console_width + 20, console_height + 20);
	}

	if(Selection == MenuSelectionConsole) {
		framebuffer_draw_rect(COLOR_BLACK, 20, 140, console_width + 20, console_height + 20);
		framebuffer_draw_rect(COLOR_WHITE, 20, 140 + iphoneos_height + 40, console_width + 20, console_height + 20);
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

	imgiPhoneOS = loadRGBImage(iphoneos_data, iphoneos_width, iphoneos_height);
	imgConsole = loadRGBImage(console_data, console_width, console_height);

	bufferPrintf("menu: images loaded\r\n");

	framebuffer_draw_image(imgiPhoneOS, 30, 150, iphoneos_width, iphoneos_height);
	framebuffer_draw_image(imgConsole, 30, 150 + iphoneos_height + 40, console_width, console_height);

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

