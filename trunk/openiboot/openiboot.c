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

#include "util.h"

static int setup_devices();

Event testEvent;

void testEventHandler(Event* event, void* opaque) {
	bufferPrintf("Hello iBoot! Up time: %Ld seconds\r\n", timer_get_system_microtime() / 1000000);
	bufferPrintf("ClockFrequency: %u Hz\r\n", (unsigned int) ClockFrequency);
	bufferPrintf("MemoryFrequency: %u Hz\r\n", (unsigned int) MemoryFrequency);
	bufferPrintf("BusFrequency: %u Hz\r\n", (unsigned int) BusFrequency);
	bufferPrintf("UnknownFrequency: %u Hz\r\n", (unsigned int) UnknownFrequency);
	bufferPrintf("PeripheralFrequency: %u Hz\r\n", (unsigned int) PeripheralFrequency);
	bufferPrintf("DisplayFrequency: %u Hz\r\n", (unsigned int) DisplayFrequency);
	bufferPrintf("FixedFrequency: %u Hz\r\n", (unsigned int) FixedFrequency);
	bufferPrintf("TimebaseFrequency: %u Hz\r\n", (unsigned int) TimebaseFrequency);
	bufferPrintf("PLL0 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[0]);
	bufferPrintf("PLL1 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[1]);
	bufferPrintf("PLL2 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[2]);
	bufferPrintf("PLL3 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[3]);

	bufferPrintf("\n\n");

	printf(getScrollback());

	event_readd(event, 0);
}

void OpenIBootStart() {
	int i;

	arm_setup();
	mmu_setup();
	tasks_setup();
	setup_devices();

	LeaveCriticalSection();

	lcd_setup();

	while(1) {
		lcd_fill(0xFF0000);
		udelay(1000000);
		lcd_fill(0x00FF00);
		udelay(1000000);
		lcd_fill(0x0000FF);
		udelay(1000000);
	}

	uint8_t test[0x40];

	uint8_t userkey[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0xE, 0x0F};

	memcpy(test, "This is a test!", sizeof("This is a test!"));
	for(i = 0; i < sizeof("This is a test!"); i++) {
		bufferPrintf("%x ", (unsigned int) test[i]);
	}
	bufferPrintf("\r\n");
	aes_encrypt(test, sizeof("This is a test!"), AESCustom, userkey, NULL);
	for(i = 0; i < sizeof("This is a test!"); i++) {
		bufferPrintf("%x ", (unsigned int) test[i]);
	}
	bufferPrintf("\r\n");
	aes_decrypt(test, sizeof("This is a test!"), AESCustom, userkey, NULL);
	for(i = 0; i < sizeof("This is a test!"); i++) {
		bufferPrintf("%x ", (unsigned int) test[i]);
	}
	bufferPrintf("\r\n");


	for(i = 0; i < 5; i++) {
		bufferPrintf("Devices loaded. OpenIBoot starting in: %d\r\n", 5 - i);
		printf("Devices loaded. OpenIBoot starting in: %d\r\n", 5 - i);
		udelay(uSecPerSec);
	}

/*
	uint16_t word;
	word = nor_read_word(0x3000);
	bufferPrintf("\r\nword at 0x3000: %x\r\n", (unsigned int) word);
	nor_erase_sector(0x3000);
	word = nor_read_word(0x3000);
	bufferPrintf("word at 0x3000: %x\r\n\r\n", (unsigned int) word);
	nor_write_word(0x3000, 0xc0de);
	word = nor_read_word(0x3000);
	bufferPrintf("word at 0x3000: %x\r\n\r\n", (unsigned int) word);
*/
	event_add(&testEvent, uSecPerSec, &testEventHandler, NULL);

	while(TRUE) {
		printf("heartbeat\r\n");
		udelay(100000);
	}
  
	DebugReboot();
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

