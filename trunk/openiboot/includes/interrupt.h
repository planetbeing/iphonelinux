#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "openiboot.h"
#include "hardware/interrupt.h"

typedef void (*InterruptServiceRoutine)(uint32_t token);

typedef struct InterruptHandler {
        InterruptServiceRoutine handler;
        uint32_t token;
        uint32_t useEdgeIC;
} InterruptHandler;

extern InterruptHandler InterruptHandlerTable[VIC_MaxInterrupt];

int interrupt_setup();
int interrupt_install(int irq_no, InterruptServiceRoutine handler, uint32_t token);
int interrupt_enable(int irq_no);
int interrupt_disable(int irq_no);

#endif
