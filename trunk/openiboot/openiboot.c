#include "openiboot.h"

static void setup_processor();
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
	setup_tasks();
	setup_devices();

	LeaveCriticalSection();

	while(1);
}

static void setup_processor() {
	DisableCPUIRQ();
	/*CleanAndInvalidateCpuDataCache();
	ClearCPUInstructionCache();*/
}

static void setup_tasks() {
	CurrentRunning = &bootstrapTask;
}

static void setup_devices() {

}
