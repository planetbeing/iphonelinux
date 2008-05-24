#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "openiboot.h"

int interrupt_setup();
int interrupt_install(int irq_no, InterruptServiceRoutine handler, uint32_t token);

#endif
