#include "openiboot.h"
#include "openiboot-asmhelpers.h"

#include "arm.h"
#include "uart.h"
#include "usb.h"
#include "mmu.h"
#include "clock.h"
#include "timer.h"
#include "event.h"
#include "miu.h"
#include "power.h"
#include "interrupt.h"
#include "gpio.h"
#include "dma.h"
#include "spi.h"
#include "i2c.h"
#include "nor.h"
#include "aes.h"
#include "lcd.h"
#include "tasks.h"
#include "images.h"
#include "syscfg.h"
#include "nvram.h"
#include "accel.h"
#include "sdio.h"
#include "wlan.h"
#include "camera.h"
#include "util.h"
#include "commands.h"
#include "framebuffer.h"
#include "menu.h"
#include "pmu.h"
#include "nand.h"
#include "ftl.h"
#include "hfs/bdev.h"
#include "hfs/fs.h"
#include "scripting.h"

#include "radio.h"
#include "wmcodec.h"
#include "wdt.h"
#include "als.h"

int received_file_size;

static int setup_devices();
static int setup_openiboot();

extern uint8_t _binary_payload_bin_start;
extern uint8_t _binary_payload_bin_end;
extern uint8_t _binary_payload_bin_size;

static void processCommand(char* command);

typedef struct CommandQueue {
	struct CommandQueue* next;
	char* command;
} CommandQueue;

CommandQueue* commandQueue = NULL;

static void startUSB();

void OpenIBootStart() {
	setup_openiboot();
	pmu_charge_settings(TRUE, FALSE, FALSE);

	framebuffer_setdisplaytext(TRUE);
	framebuffer_clear();

#ifndef SMALL
#ifndef NO_STBIMAGE
	const char* hideMenu = nvram_getvar("opib-hide-menu");
	if(hideMenu && (strcmp(hideMenu, "1") == 0 || strcmp(hideMenu, "true") == 0)) {
		bufferPrintf("Boot menu hidden. Use 'setenv opib-hide-menu false' and then 'saveenv' to unhide.\r\n");
	} else {
		framebuffer_setdisplaytext(FALSE);
		const char* sMenuTimeout = nvram_getvar("opib-menu-timeout");
		int menuTimeout = -1;
		if(sMenuTimeout)
			menuTimeout = parseNumber(sMenuTimeout);

		menu_setup(menuTimeout);
	}
#endif
#endif

	startUSB();

#ifndef CONFIG_IPOD
	camera_setup();
	radio_setup();
#endif
	sdio_setup();
	wlan_setup();
	accel_setup();
#ifndef CONFIG_IPOD
	als_setup();
#endif

	nand_setup();
#ifndef NO_HFS
	fs_setup();
#endif

	pmu_set_iboot_stage(0);
	startScripting("openiboot"); //start script mode if there is a file
	bufferPrintf("version: %s\r\n", OPENIBOOT_VERSION_STR);
	bufferPrintf("-----------------------------------------------\r\n");
	bufferPrintf("              WELCOME TO OPENIBOOT\r\n");
	bufferPrintf("-----------------------------------------------\r\n");
	DebugPrintf("                    DEBUG MODE\r\n");

	audiohw_postinit();

	// Process command queue
	while(TRUE) {
		char* command = NULL;
		CommandQueue* cur;
		EnterCriticalSection();
		if(commandQueue != NULL) {
			cur = commandQueue;
			command = cur->command;
			commandQueue = commandQueue->next;
			free(cur);
		}
		LeaveCriticalSection();

		if(command) {
			processCommand(command);
			free(command);
		}
	}
	// should not reach here

}

static uint8_t* controlSendBuffer = NULL;
static uint8_t* controlRecvBuffer = NULL;
static uint8_t* dataSendBuffer = NULL;
static uint8_t* dataRecvBuffer = NULL;
static uint8_t* commandRecvBuffer = NULL;
static uint8_t* dataRecvPtr = NULL;
static size_t left = 0;
static size_t rxLeft = 0;
static size_t lastRxLen = 0;

static uint8_t* sendFilePtr = NULL;
static uint32_t sendFileBytesLeft = 0;

static int USB_BYTES_AT_A_TIME = 0;

static void addToCommandQueue(const char* command) {
	EnterCriticalSection();

	if(dataRecvBuffer != commandRecvBuffer) {
		// in file mode, but we just received the whole thing
		dataRecvBuffer = commandRecvBuffer;
		bufferPrintf("file received (%d bytes).\r\n", lastRxLen);
		received_file_size = lastRxLen;
		LeaveCriticalSection();
		return;
	}

	CommandQueue* toAdd = malloc(sizeof(CommandQueue));
	toAdd->next = NULL;
	toAdd->command = strdup(command);

	CommandQueue* prev = NULL;
	CommandQueue* cur = commandQueue;
	while(cur != NULL) {
		prev = cur;
		cur = cur->next;
	}

	if(prev == NULL) {
		commandQueue = toAdd;
	} else {
		prev->next = toAdd;
	}
	LeaveCriticalSection();
}

static void processCommand(char* command) {
	int argc;
	char** argv = tokenize(command, &argc);

	if(strcmp(argv[0], "sendfile") == 0) {
		if(argc >= 2) {
			// enter file mode
			EnterCriticalSection();
			if(dataRecvBuffer == commandRecvBuffer) {
				dataRecvBuffer = (uint8_t*) parseNumber(argv[1]);
			}
			LeaveCriticalSection();
			free(argv);
			return;
		}
	}

	if(strcmp(argv[0], "getfile") == 0) {
		if(argc >= 3) {
			// enter file mode
			EnterCriticalSection();
			if(sendFileBytesLeft == 0) {
				sendFilePtr = (uint8_t*) parseNumber(argv[1]);
				sendFileBytesLeft = parseNumber(argv[2]);
			}
			LeaveCriticalSection();
			free(argv);
			return;
		}
	}

	OPIBCommand* curCommand = CommandList;

	int success = FALSE;
	while(curCommand->name != NULL) {
		if(strcmp(argv[0], curCommand->name) == 0) {
			curCommand->routine(argc, argv);
			success = TRUE;
			break;
		}
		curCommand++;
	}

	if(!success) {
		bufferPrintf("unknown command: %s\r\n", command);
	}

	free(argv);
}

static void controlReceived(uint32_t token) {
	OpenIBootCmd* cmd = (OpenIBootCmd*)controlRecvBuffer;
	OpenIBootCmd* reply = (OpenIBootCmd*)controlSendBuffer;

	if(cmd->command == OPENIBOOTCMD_DUMPBUFFER) {
		int length;

		if(sendFileBytesLeft > 0) {
			length = sendFileBytesLeft;
		} else {
			length = getScrollbackLen(); // getScrollbackLen();// USB_BYTES_AT_A_TIME;
		}

		reply->command = OPENIBOOTCMD_DUMPBUFFER_LEN;
		reply->dataLen = length;
		usb_send_interrupt(3, controlSendBuffer, sizeof(OpenIBootCmd));
		//uartPrintf("got dumpbuffer cmd, returning length: %d\r\n", length);
	} else if(cmd->command == OPENIBOOTCMD_DUMPBUFFER_GOAHEAD) {
		left = cmd->dataLen;

		//uartPrintf("got dumpbuffer goahead, writing length: %d\r\n", (int)left);

		size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: left;
		if(sendFileBytesLeft > 0) {
			usb_send_bulk(1, sendFilePtr, toRead);
			sendFilePtr += toRead;
			sendFileBytesLeft -= toRead;
			if(sendFileBytesLeft == 0) {
				bufferPrintf("file sent.\r\n");
			}
		} else {
			bufferFlush((char*) dataSendBuffer, toRead);
			usb_send_bulk(1, dataSendBuffer, toRead);
		}
		left -= toRead;
	} else if(cmd->command == OPENIBOOTCMD_SENDCOMMAND) {
		dataRecvPtr = dataRecvBuffer;
		rxLeft = cmd->dataLen;
		lastRxLen = rxLeft;

		//uartPrintf("got sendcommand, receiving length: %d\r\n", (int)rxLeft);

		reply->command = OPENIBOOTCMD_SENDCOMMAND_GOAHEAD;
		reply->dataLen = cmd->dataLen;
		usb_send_interrupt(3, controlSendBuffer, sizeof(OpenIBootCmd));

		size_t toRead = (rxLeft > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: rxLeft;
		usb_receive_bulk(2, dataRecvPtr, toRead);
		rxLeft -= toRead;
		dataRecvPtr += toRead;
	}

	usb_receive_interrupt(4, controlRecvBuffer, sizeof(OpenIBootCmd));
}

static void dataReceived(uint32_t token) {
	//uartPrintf("receiving remainder: %d\r\n", (int)rxLeft);
	if(rxLeft > 0) {
		size_t toRead = (rxLeft > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: rxLeft;
		usb_receive_bulk(2, dataRecvPtr, toRead);
		rxLeft -= toRead;
		dataRecvPtr += toRead;
	} else {
		*dataRecvPtr = '\0';
		addToCommandQueue((char*)dataRecvBuffer);
	}
}

static void dataSent(uint32_t token) {
	//uartPrintf("sending remainder: %d\r\n", (int)left);
	if(left > 0) {
		size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: left;
		if(sendFileBytesLeft > 0) {
			usb_send_bulk(1, sendFilePtr, toRead);
			sendFilePtr += toRead;
			sendFileBytesLeft -= toRead;
			if(sendFileBytesLeft == 0) {
				bufferPrintf("file sent.\r\n");
			}
		} else {
			bufferFlush((char*) dataSendBuffer, toRead);
			usb_send_bulk(1, dataSendBuffer, toRead);
		}
		left -= toRead;
	}
}

static void controlSent(uint32_t token) {
	//uartPrintf("control sent\r\n");
}

static void enumerateHandler(USBInterface* interface) {
	usb_add_endpoint(interface, 1, USBIn, USBBulk);
	usb_add_endpoint(interface, 2, USBOut, USBBulk);
	usb_add_endpoint(interface, 3, USBIn, USBInterrupt);
	usb_add_endpoint(interface, 4, USBOut, USBInterrupt);

	if(!controlSendBuffer)
		controlSendBuffer = memalign(DMA_ALIGN, 512);

	if(!controlRecvBuffer)
		controlRecvBuffer = memalign(DMA_ALIGN, 512);

	if(!dataSendBuffer)
		dataSendBuffer = memalign(DMA_ALIGN, 512);

	if(!dataRecvBuffer)
		dataRecvBuffer = commandRecvBuffer = memalign(DMA_ALIGN, 512);
}

static void startHandler() {
	if(usb_get_speed() == USBHighSpeed) {
		USB_BYTES_AT_A_TIME = 512;
	} else {
		USB_BYTES_AT_A_TIME = 0x80;
	}

	usb_receive_interrupt(4, controlRecvBuffer, sizeof(OpenIBootCmd));
}

static void startUSB()
{
	usb_setup();
	usb_install_ep_handler(4, USBOut, controlReceived, 0);
	usb_install_ep_handler(2, USBOut, dataReceived, 0);
	usb_install_ep_handler(3, USBIn, controlSent, 0);
	usb_install_ep_handler(1, USBIn, dataSent, 0);
	usb_start(enumerateHandler, startHandler);
}

static int setup_devices() {
	// Basic prerequisites for everything else
	miu_setup();
	power_setup();
	clock_setup();

	// Need interrupts for everything afterwards
	interrupt_setup();

	gpio_setup();

	// For scheduling/sleeping niceties
	timer_setup();
	event_setup();
	wdt_setup();

	// Other devices
	usb_shutdown();
	uart_setup();
	i2c_setup();

	dma_setup();

	spi_setup();

	return 0;
}

static int setup_openiboot() {
	arm_setup();
	mmu_setup();
	tasks_setup();
	setup_devices();

	LeaveCriticalSection();

	clock_set_sdiv(0);

	aes_setup();

	nor_setup();
	syscfg_setup();
	images_setup();
	nvram_setup();

	lcd_setup();
	framebuffer_setup();

	audiohw_init();

	return 0;
}

