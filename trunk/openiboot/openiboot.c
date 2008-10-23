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

#include "util.h"

static int setup_devices();
static int setup_openiboot();

extern uint8_t _binary_payload_bin_start;
extern uint8_t _binary_payload_bin_end;
extern uint8_t _binary_payload_bin_size;


void OpenIBootStart() {
	setup_openiboot();

	while(TRUE) {
	}

	void* iboot;
	void* ibootLoc = (void*) 0x09000000;

	images_setup();
	images_list();

	int verify = images_verify(images_get(fourcc("opib")));

	if(verify != 0) {
		bufferPrintf("Image verification failed!\r\n");
	} else {
		bufferPrintf("Image verification successful!\r\n");

		unsigned int len = images_read(images_get(fourcc("opib")), &iboot);

		memcpy(ibootLoc, iboot, len);

		bufferPrintf("jumping to iboot at: %x\r\n", ibootLoc);

		EnterCriticalSection();
		CallArm((uint32_t) ibootLoc);
	}

	while(TRUE);

	// should not reach here

}

void processCommand(const char* command) {
	if(strcmp(command, "reboot") == 0) {
		Reboot();
	} else {
		bufferPrintf("unknown command: %s\r\n", command);
	}
}

static uint8_t* controlSendBuffer = NULL;
static uint8_t* controlRecvBuffer = NULL;
static uint8_t* dataSendBuffer = NULL;
static uint8_t* dataRecvBuffer = NULL;
static uint8_t* dataRecvPtr = NULL;
static size_t left = 0;
static size_t rxLeft = 0;

#define USB_BYTES_AT_A_TIME 0x80

static void controlReceived(uint32_t token) {
	OpenIBootCmd* cmd = (OpenIBootCmd*)controlRecvBuffer;
	OpenIBootCmd* reply = (OpenIBootCmd*)controlSendBuffer;

	if(cmd->command == OPENIBOOTCMD_DUMPBUFFER) {
		int length = getScrollbackLen(); // getScrollbackLen();// 0x80;
		reply->command = OPENIBOOTCMD_DUMPBUFFER_LEN;
		reply->dataLen = length;
		usb_send_interrupt(3, controlSendBuffer, sizeof(OpenIBootCmd));
		//uartPrintf("got dumpbuffer cmd, returning length: %d\r\n", length);
	} else if(cmd->command == OPENIBOOTCMD_DUMPBUFFER_GOAHEAD) {
		left = cmd->dataLen;

		//uartPrintf("got dumpbuffer goahead, writing length: %d\r\n", (int)left);

		size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: left;
		bufferFlush((char*) dataSendBuffer, toRead);
		usb_send_bulk(1, dataSendBuffer, toRead);
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
		processCommand((char*)dataRecvBuffer);
	}	
}

static void dataSent(uint32_t token) {
	//uartPrintf("sending remainder: %d\r\n", (int)left);
	if(left > 0) {
		size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: left;
		bufferFlush((char*) dataSendBuffer, toRead);
		usb_send_bulk(1, dataSendBuffer, toRead);
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
		dataRecvBuffer = memalign(DMA_ALIGN, 0x80);
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

	//lcd_setup();

	aes_setup();
	//images_setup();

	return 0;
}

