#include "openiboot.h"
#include "s5l8900.h"
#include "openiboot-asmhelpers.h"

static void setup_processor();
static void setup_mmu();
static void setup_tasks();
static void setup_devices();

TaskDescriptor bootstrapTask = {
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

void OpenIBootStart() {
	setup_processor();
	mmu_setup();

	setup_tasks();
	setup_devices();

	LeaveCriticalSection();

	while(1);
}

static void setup_processor() {

	CleanAndInvalidateCPUDataCache();
	ClearCPUInstructionCache();

	WriteControlRegisterConfigData(ReadControlRegisterConfigData() & ~(0x1000));	// Disable instruction cache
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() & ~(0x4));	// Disable data cache

	GiveFullAccessCP10CP11();
	EnableVFP();

	// Map the peripheral port of size 128 MB to 0x38000000
	WritePeripheralPortMemoryRemapRegister(PeripheralPort | ARM11_PeripheralPortSize128MB);
	InvalidateCPUDataCache();
	ClearCPUInstructionCache();

	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | 0x1000);	// Enable instruction cache
	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | 0x4);		// Enable data cache

	WriteControlRegisterConfigData(ReadControlRegisterConfigData()
		& ~(0x2)								// Disable strict alignment fault checking
		| 0x400000);								// Enable unaligned data access operations

	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | 0x4);

	WriteControlRegisterConfigData(ReadControlRegisterConfigData() | 0x800); 	// Enable branch prediction

	// Enable return stack, dynamic branch prediction, static branch prediction
	WriteAuxiliaryControlRegister(ReadAuxiliaryControlRegister() | 0x7);
}

static void setup_tasks() {
	CurrentRunning = &bootstrapTask;
}

static void setup_devices() {

}
