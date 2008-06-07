#include "openiboot.h"
#include "hardware/s5l8900.h"
#include "hardware/arm.h"
#include "openiboot-asmhelpers.h"
#include "uart.h"
#include "usb.h"
#include "util.h"
#include "clock.h"

static int setup_processor();
static int setup_mmu();
static int setup_tasks();
static int setup_devices();

static TaskDescriptor bootstrapTaskInit = {
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

void OpenIBootStart() {
	setup_processor();
	mmu_setup();
	setup_tasks();
	setup_devices();

	LeaveCriticalSection();

	while(TRUE) {
		printf("Hello iBoot!\r\n");
		printf("ClockFrequency: %x\r\n", ClockFrequency);
		printf("MemoryFrequency: %x\r\n", MemoryFrequency);
		printf("BusFrequency: %x\r\n", BusFrequency);
		printf("UnknownFrequency: %x\r\n", UnknownFrequency);
		printf("PeripheralFrequency: %x\r\n", PeripheralFrequency);
		printf("Unknown2Frequency: %x\r\n", Unknown2Frequency);
		printf("FixedFrequency: %x\r\n", FixedFrequency);
		printf("TimebaseFrequency: %x\r\n", TimebaseFrequency);
		printf("PLL0 Frequency: %u\r\n", PLLFrequencies[0]);
		printf("PLL1 Frequency: %u\r\n", PLLFrequencies[1]);
		printf("PLL2 Frequency: %u\r\n", PLLFrequencies[2]);
		printf("PLL3 Frequency: %u\r\n", PLLFrequencies[3]);
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

	WriteControlRegisterConfigData(ReadControlRegisterConfigData()
		& ~(ARM11_Control_STRICTALIGNMENTCHECKING)				// Disable strict alignment fault checking
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

