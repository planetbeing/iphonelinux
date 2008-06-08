#include "openiboot.h"
#include "hardware/s5l8900.h"
#include "hardware/arm.h"
#include "openiboot-asmhelpers.h"

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
	printf("Hello iBoot! Up time: %Ld seconds\r\n", timer_get_system_microtime() / 1000000);
	printf("ClockFrequency: %u Hz\r\n", ClockFrequency);
	printf("MemoryFrequency: %u Hz\r\n", MemoryFrequency);
	printf("BusFrequency: %u Hz\r\n", BusFrequency);
	printf("UnknownFrequency: %u Hz\r\n", UnknownFrequency);
	printf("PeripheralFrequency: %u Hz\r\n", PeripheralFrequency);
	printf("Unknown2Frequency: %u Hz\r\n", Unknown2Frequency);
	printf("FixedFrequency: %u Hz\r\n", FixedFrequency);
	printf("TimebaseFrequency: %u Hz\r\n", TimebaseFrequency);
	printf("PLL0 Frequency: %u Hz\r\n", PLLFrequencies[0]);
	printf("PLL1 Frequency: %u Hz\r\n", PLLFrequencies[1]);
	printf("PLL2 Frequency: %u Hz\r\n", PLLFrequencies[2]);
	printf("PLL3 Frequency: %u Hz\r\n", PLLFrequencies[3]);
	printf("\n\n");

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
		printf("Devices loaded. OpenIBoot starting in: %d\r\n", 5 - i);
		udelay(uSecPerSec);
	}

	event_add(&testEvent, uSecPerSec, &testEventHandler, NULL);

	while(TRUE);
  
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

	return 0;
}

