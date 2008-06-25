#include "tasks.h"
#include "util.h"

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

int tasks_setup() {
	memcpy(&bootstrapTask, &bootstrapTaskInit, sizeof(TaskDescriptor));
	CurrentRunning = &bootstrapTask;
	return 0;
}

