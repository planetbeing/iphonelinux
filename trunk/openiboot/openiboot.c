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

void OpenIBootStart() {
	setup_openiboot();

	void* iboot;

	if(images_get(fourcc("opib")) != NULL)
		images_erase(images_get(fourcc("opib")));

	images_duplicate(images_get(fourcc("ibot")), fourcc("opib"), -1);

	images_read(images_get(fourcc("ibot")), &iboot);

	EnterCriticalSection();

	CallArm((uint32_t) iboot);

	// should not reach here

}

static uint8_t* controlSendBuffer = NULL;
static uint8_t* controlRecvBuffer = NULL;
static uint8_t* dataSendBuffer = NULL;
static uint8_t* dataRecvBuffer = NULL;
static size_t left = 0;

#define USB_BYTES_AT_A_TIME 0x80

static void controlReceived(uint32_t token) {
	OpenIBootCmd* cmd = (OpenIBootCmd*)controlRecvBuffer;
	OpenIBootCmd* reply = (OpenIBootCmd*)controlSendBuffer;

	if(cmd->command == OPENIBOOTCMD_DUMPBUFFER) {
		int length = getScrollbackLen();
		reply->command = OPENIBOOTCMD_DUMPBUFFER_LEN;
		reply->dataLen = length;
		usb_send_interrupt(3, controlSendBuffer, sizeof(OpenIBootCmd));
	} else if(cmd->command == OPENIBOOTCMD_DUMPBUFFER_GOAHEAD) {
		left = cmd->dataLen;

		size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: left;
		bufferFlush((char*) dataSendBuffer, toRead);
		usb_send_bulk(1, dataSendBuffer, toRead);
		left -= toRead;
	}

	usb_receive_interrupt(4, controlRecvBuffer, sizeof(OpenIBootCmd));
}

static void dataReceived(uint32_t token) {

}

static void dataSent(uint32_t token) {
	size_t toRead = (left > USB_BYTES_AT_A_TIME) ? USB_BYTES_AT_A_TIME: left;
	bufferFlush((char*) dataSendBuffer, toRead);
	usb_send_bulk(1, dataSendBuffer, toRead);
	left -= toRead;
}

static void controlSent(uint32_t token) {

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
	nor_setup();

	return 0;
}

static int setup_openiboot() {
	arm_setup();
	mmu_setup();
	tasks_setup();
	setup_devices();

	LeaveCriticalSection();

	lcd_setup();

	aes_setup();
	images_setup();

	return 0;
}

