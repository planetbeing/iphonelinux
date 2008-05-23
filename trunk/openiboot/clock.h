#ifndef MIU_H
#define MIU_H

#include "openiboot.h"

typedef enum Clock0ConfigCode {
	Clock0ConfigCode0 = 0,
	Clock0ConfigCode1 = 1,
	Clock0ConfigCode2 = 2,
	Clock0ConfigCode3 = 3
} Clock0ConfigCode;

int clock_set_bottom_bits_38100000(Clock0ConfigCode code);

#endif
