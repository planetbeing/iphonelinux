#include "openiboot.h"
#include "hardware/s5l8900.h"
#include "hardware/arm.h"
#include "openiboot-asmhelpers.h"
#include "hardware/gpio.h"
#include "hardware/power.h"

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
#include "nor.h"

#include "util.h"

static int setup_processor();
static int setup_tasks();
static int setup_devices();

const TaskDescriptor bootstrapTaskInit = {
	TaskDescriptorIdentifier1,
	{0, 0},
	{0, 0},
	TASK_RUNNING,
	1,
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{{0, 0}, 0, 0, 0, 0},
	{0, 0},
	0,
	0,
	0,
	0,
	0,
	"bootstrap",
	TaskDescriptorIdentifier2
	};

TaskDescriptor bootstrapTask;

Event testEvent;

void testEventHandler(Event* event, void* opaque) {
	bufferPrintf("Hello iBoot! Up time: %Ld seconds\r\n", timer_get_system_microtime() / 1000000);
/*	printf("ClockFrequency: %u Hz\r\n", (unsigned int) ClockFrequency);
	printf("MemoryFrequency: %u Hz\r\n", (unsigned int) MemoryFrequency);
	printf("BusFrequency: %u Hz\r\n", (unsigned int) BusFrequency);
	printf("UnknownFrequency: %u Hz\r\n", (unsigned int) UnknownFrequency);
	printf("PeripheralFrequency: %u Hz\r\n", (unsigned int) PeripheralFrequency);
	printf("Unknown2Frequency: %u Hz\r\n", (unsigned int) Unknown2Frequency);
	printf("FixedFrequency: %u Hz\r\n", (unsigned int) FixedFrequency);
	printf("TimebaseFrequency: %u Hz\r\n", (unsigned int) TimebaseFrequency);
	printf("PLL0 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[0]);
	printf("PLL1 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[1]);
	printf("PLL2 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[2]);
	printf("PLL3 Frequency: %u Hz\r\n", (unsigned int) PLLFrequencies[3]);*/

	bufferPrintf("\n\n");

	printf(getScrollback());

	event_readd(event, 0);
}

void OpenIBootStart() {
	setup_processor();
	mmu_setup();
	setup_tasks();
	setup_devices();

	LeaveCriticalSection();

	int i;

	for(i = 0; i < 5; i++) {
		bufferPrintf("Devices loaded. OpenIBoot starting in: %d\r\n", 5 - i);
		printf("Devices loaded. OpenIBoot starting in: %d\r\n", 5 - i);
		udelay(uSecPerSec);
	}

	uint16_t word;
	word = nor_read_word(0x3000);
	bufferPrintf("\r\nword at 0x3000: %x\r\n", (unsigned int) word);
	nor_erase_sector(0x3000);
	word = nor_read_word(0x3000);
	bufferPrintf("word at 0x3000: %x\r\n\r\n", (unsigned int) word);
	nor_write_word(0x3000, 0xc0de);
	word = nor_read_word(0x3000);
	bufferPrintf("word at 0x3000: %x\r\n\r\n", (unsigned int) word);

	event_add(&testEvent, uSecPerSec, &testEventHandler, NULL);

	while(TRUE) {
		printf("heartbeat\r\n");
		udelay(100000);
	}
  
	DebugReboot();
}


static int setup_processor() {

	CleanAndInvalidateCPUDataCache();
	ClearCPUInstructionCache();

	WriteControlRegisterConfigData(ReadControlRegisterConfigData() & ~(ARM11_Control_INSTRUCTIONCACHE));	// Disable instruction cache
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() & ~(ARM11_Control_DATACACHE));		// Disable data cache

	GiveFullAccessCP10CP11();
	EnableVFP();

	// Map the peripheral port of size 128 MB to 0x38000000
	WritePeripheralPortMemoryRemapRegister(PeripheralPort | ARM11_PeripheralPortSize128MB);
	InvalidateCPUDataCache();
	ClearCPUInstructionCache();

	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | ARM11_Control_INSTRUCTIONCACHE);	// Enable instruction cache
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | ARM11_Control_DATACACHE);		// Enable data cache

	WriteControlRegisterConfigData((ReadControlRegisterConfigData()
		& ~(ARM11_Control_STRICTALIGNMENTCHECKING))				// Disable strict alignment fault checking
		| ARM11_Control_UNALIGNEDDATAACCESS);					// Enable unaligned data access operations


	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | ARM11_Control_BRANCHPREDICTION); 	// Enable branch prediction

	// Enable return stack, dynamic branch prediction, static branch prediction
	WriteAuxiliaryControlRegister(ReadAuxiliaryControlRegister()
		| ARM11_AuxControl_RETURNSTACK
		| ARM11_AuxControl_DYNAMICBRANCHPREDICTION
		| ARM11_AuxControl_STATICBRANCHPREDICTION);

	return 0;
}

static int setup_tasks() {
	memcpy(&bootstrapTask, &bootstrapTaskInit, sizeof(TaskDescriptor));
	CurrentRunning = &bootstrapTask;
	return 0;
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

	dma_setup();
	usb_setup();
	usb_install_ep_handler(4, USBOut, controlReceived, 0);
	usb_install_ep_handler(2, USBOut, dataReceived, 0);
	usb_install_ep_handler(3, USBIn, controlSent, 0);
	usb_install_ep_handler(1, USBIn, dataSent, 0);
	usb_start(enumerateHandler, startHandler);

	nor_setup();

	return 0;
}

