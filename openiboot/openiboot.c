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
#include "nvram.h"

#include "util.h"
#include "commands.h"
#include "framebuffer.h"
#include "menu.h"
#include "pmu.h"
#include "nand.h"
#include "ftl.h"

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

void OpenIBootStart() {
	setup_openiboot();
	pmu_charge_settings(TRUE, FALSE, FALSE);

	const char* hideMenu = nvram_getvar("opib-hide-menu");
	if(hideMenu && (strcmp(hideMenu, "1") == 0 || strcmp(hideMenu, "true") == 0)) {
		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();
		bufferPrintf("Boot menu hidden. Use 'setenv opib-hide-menu false' and then 'saveenv' to unhide.\r\n");
	} else {
		menu_setup();
	}

	nand_setup();
	ftl_setup();

	bufferPrintf("-----------------------------------------------\r\n");
	bufferPrintf("              WELCOME TO OPENIBOOT\r\n");
	bufferPrintf("-----------------------------------------------\r\n");

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

static uint8_t* sendFilePtr = NULL;
static uint32_t sendFileBytesLeft = 0;

#define USB_BYTES_AT_A_TIME 0x80

static void addToCommandQueue(const char* command) {
	EnterCriticalSection();

	if(dataRecvBuffer != commandRecvBuffer) {
		// in file mode, but we just received the whole thing
		dataRecvBuffer = commandRecvBuffer;
		bufferPrintf("file received.\r\n");
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
			length = getScrollbackLen(); // getScrollbackLen();// 0x80;
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
		controlSendBuffer = memalign(DMA_ALIGN, 0x80);

	if(!controlRecvBuffer)
		controlRecvBuffer = memalign(DMA_ALIGN, 0x80);

	if(!dataSendBuffer)
		dataSendBuffer = memalign(DMA_ALIGN, 0x80);

	if(!dataRecvBuffer)
		dataRecvBuffer = commandRecvBuffer = memalign(DMA_ALIGN, 0x80);
}

static void startHandler() {
	usb_receive_interrupt(4, controlRecvBuffer, sizeof(OpenIBootCmd));
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

	// Other devices
	usb_shutdown();
	uart_setup();
	i2c_setup();

	dma_setup();
	usb_setup();
	usb_install_ep_handler(4, USBOut, controlReceived, 0);
	usb_install_ep_handler(2, USBOut, dataReceived, 0);
	usb_install_ep_handler(3, USBIn, controlSent, 0);
	usb_install_ep_handler(1, USBIn, dataSent, 0);
	usb_start(enumerateHandler, startHandler);

	spi_setup();

//	bufferPrintf("Before:\r\n");
//	buffer_dump_memory(0x3e400000, 0x320);
//	gpio_reset();
/*	bufferPrintf("After:\r\n");
	buffer_dump_memory2(0x3e400000, 0x320, 8);
	power_ctrl(0x1000, ON);
	clock_gate_switch(0x1E, ON);
	int i = 0;
//	uint32_t position = 0x20000000 + (0x40 * 645);
	uint32_t position = 0x2000A000 + (0x40 * 290);
	position = 0x2000e600;
	for(i = 0; i < 10; i++) {
		bufferDump(position, 0x40);
		position += 0x40;
	}
	clock_gate_switch(0x1E, OFF);
	power_ctrl(0x1000, OFF);*/

	return 0;
}

static int setup_openiboot() {
	arm_setup();
	mmu_setup();
	tasks_setup();
	setup_devices();

	LeaveCriticalSection();

	clock_set_sdiv(0);
	lcd_setup();

	framebuffer_setup();

	aes_setup();

	nor_setup();
	images_setup();
	nvram_setup();

	return 0;
}

