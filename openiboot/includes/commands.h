#ifndef COMMANDS_H
#define COMMANDS_H

#include "openiboot.h"

typedef void (*OPIBCommandRoutine)(int argc, char** argv);

typedef struct OPIBCommand {
	char* name;
	char* description;
	OPIBCommandRoutine routine;
} OPIBCommand;

extern OPIBCommand CommandList[];

#endif
