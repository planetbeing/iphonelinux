#include "hardware/s5l8900.h"
#include "hardware/interrupt.h"
#include "hardware/edgeic.h"
#include "hardware/arm.h"

.include "openiboot.S.h"

.global _start

.global InterruptHandlerTable
.global ExceptionStack
.global ExceptionStackEnd
.global OpenIBootEnd

.text

//
//	Exception vector
//

.code 32
_start:
	B	ArmReset
	LDR	PC, =ArmUndefinedHandler
	LDR	PC, =ArmSyscall
	LDR	PC, =ArmPrefetchAbortHandler
	LDR	PC, =ArmDataAbortHandler
	LDR	PC, =ArmReservedHandler
	LDR	PC, =ArmIRQHandler
	LDR	PC, =ArmFIQHandler

//
//	Exception handlers (ArmReset: starts the boot sequence)
//

.equ ArmResetDifferenceFromEntry, (ArmReset + 0x8 - OpenIBootLoad)

.code 32
ArmReset:						// This function will setup the processor and kick off execution

	MOV	R0, PC
	LDR	R1, =ArmResetDifferenceFromEntry
	SUB	R0, R0,	R1
	LDR	R1, =_start
	CMP	R0, R1
	BEQ	ArmReset_zeroBSS

	// We need to get the MIU settings right. First we have to setup the peripheral remap register

	MOV	R1, #0x38000000				// Peripheral port at 0x38000000
	ADD	R1, #0x12				// Peripheral port size 128 MB
	MCR     p15, 0, R1, c15, c2, 4			// Write peripheral port settings

	MOV	R1, #0x38000000				// MIU register
	ADD	R1, #0x00100000
	LDR	R2, [R1]				// Get current settings
	BIC	R2, #0x7				// Mask out current MIU setting
	ORR	R2, #1					// Change setting so 0x0 is physical memory
	STR	R2, [R1]				// Set MIU settings

	LDR	R1, =_start				// OMG, we're not in the right place! Copy us to the right place
	LDR	R2, =(OpenIBootEnd - OpenIBootLoad)	// This is the size of OpenIBoot

ArmReset_relocate:
	LDR	R3, [R0],#4				// copy *R0 to *R1, increment R0, R1.
	SUBS	R2, R2,	#4
	STR	R3, [R1],#4
	BNE	ArmReset_relocate			// while the Zero flag wasn't set by the earlier SUBS operation, i.e.,
							// the counter is not 0.

	MOV	R0, #0					// reset caches, since we're about to do a jump to where there wasn't any
							// data or instructions just moments ago
	MCR	p15, 0,	R0,c7,c10			// clean entire data cache
	MCR	p15, 0,	R0,c7,c5			// invalidate entire instruction cache

	LDR	R1, =_start				// jump to the right place!
	BX	R1

ArmReset_zeroBSS:					// Zero out the BSS section
	LDR	R0, =OpenIBootEnd			// everything between text and heap
	LDR	R1, =_end
	MOV	R2, #0

ArmReset_zeroBSS_loop:
	CMP	R0, R1
	STRLT	R2, [R0],#4
	BLT	ArmReset_zeroBSS_loop

	MRS	R0, CPSR				// setup the stack for mode:
	BIC	R0, R0,	#ARM11_CPSR_MODEMASK

	ORR	R1, R0,	#ARM11_CPSR_IRQMODE
	MSR	CPSR_c,	R1				// set mode to b10010 (IRQ mode)
	LDR	SP, =ExceptionStack			// set the stack to the ExceptionStack

	ORR	R1, R0,	#ARM11_CPSR_FIQMODE
	MSR	CPSR_c,	R1				// set mode to b10001 (FIQ mode)
	LDR	SP, =ExceptionStack			// set the stack to the ExceptionStack

	ORR	R1, R0,	#ARM11_CPSR_ABORTMODE
	MSR	CPSR_c,	R1				// set mode to b10111 (abort mode)
	LDR	SP, =ExceptionStack			// set the stack to the ExceptionStack

	ORR	R1, R0,	#ARM11_CPSR_UNDEFINEDMODE	
	MSR	CPSR_c,	R1				// set mode to b11011 (Undefined)
	LDR	SP, =ExceptionStack			// set the stack to the ExceptionStack

	ORR	R1, R0,	#ARM11_CPSR_SUPERVISORMODE
	MSR	CPSR_c,	R1				// set mode to b10011 (supervisor mode)
	LDR	SP, =GeneralStack			// set the stack to the GeneralStack

	LDR	R0, =OpenIBootStart			// call the Thumb routine OpenIBootStart()
	MOV	LR, PC
	BX	R0

ArmReset_loopForever:					// shouldn't reach here
	B	ArmReset_loopForever


DebugReboot:						// Write to the WDT register without memory access
							// This ought to cause a reboot same as the normal
							// Reboot function
	MOV	R1, #0x3E000000
	ADD	R1, #0x00300000
	MOV	R0, #0x100000
	STR	R0, [R1]
	B	DebugReboot

//
//	Exception handlers (IRQ handlers)
//

.code 32
ArmIRQHandler:
	SUB	LR, LR,	#4
	STMFD	SP!, {R0-R3,R12,LR}
	BLX	ThumbIRQHandler
	LDMFD	SP!, {R0-R3,R12,PC}^
	B	ArmReset


ArmFIQHandler:
	SUB	LR, LR,	#4
	STMFD	SP!, {R0-R3,R12,LR}
	BLX	ThumbFIQHandler
	LDMFD	SP!, {R0-R3,R12,PC}^


.code 16
ThumbIRQHandler:
	PUSH	{LR}
	BL	IncrementCriticalLock
	BL	HandleIRQ
	BL	DecrementCriticalLock
	POP	{PC}


ThumbFIQHandler:
	PUSH	{LR}
	BL	IncrementCriticalLock
	BL	HandleFIQ
	BL	DecrementCriticalLock
	POP	{PC}


HandleIRQ:
	PUSH	{R4,R5,R6,LR}
	LDR	R3, =(VIC0 + VICIRQSTATUS)	// Check if the interrupt was on VIC0
	LDR	R3, [R3]
	CMP	R3, #0
	BEQ	HandleIRQ_checkVIC1
	LDR	R5, =(VIC0 + VICADDRESS)
	B	HandleIRQ_lookupHandler

HandleIRQ_checkVIC1:
	LDR	R3, =(VIC1 + VICIRQSTATUS)	// Check if the interrupt was on VIC1, if it wasn't on VIC0
	LDR	R3, [R3]
	CMP	R3, #0
	BEQ	HandleIRQ_return
	LDR	R5, =(VIC1 + VICADDRESS)

HandleIRQ_lookupHandler:
	LDR	R2, [R5]			// Read VICADDRESS. Note that this indicates to the VIC hardware the interrupt is being serviced
	CMP	R2, #VIC_MaxInterrupt
	BHI	HandleIRQ_callHandler		// Directly call what's in the VICADDRESS if it is an actual ISR address

	LDR	R1, =InterruptHandlerTable
	LSL	R0, R2,	#1			// Index into the InterruptHandlerTable
	ADD	R3, R0,	R2
	LSL	R6, R3,	#2
	ADD	R6, R6,	R1

	LDR	R3, [R6,#InterruptHandler.useEdgeIC]
	CMP	R3, #0
	BEQ	HandleIRQ_prepareHandler	// Just call the handler if we aren't using the edge IC

	CMP	R2, #VIC_InterruptSeparator	// Figure out which edge IC register to write to
						// We never should get here, because we don't use the edge IC. This is included for completeness
	BHI	HandleIRQ_edgeICHighInterrupt
	LDR	R4, =(EDGEIC + EDGEICLOWSTATUS)
	MOV	R3, #1
	LSL	R3, R2				// write 1 << irq_no
	B	HandleIRQ_edgeICReset
	
HandleIRQ_edgeICHighInterrupt:
	LDR	R4, =(EDGEIC + EDGEICHIGHSTATUS)
	SUB	R2, #VIC_InterruptSeparator
	MOV	R3, #1
	LSL	R3, R2				// write 1 << (irq_no - 0x20)
	
HandleIRQ_edgeICReset:
	STR	R3, [R4]

HandleIRQ_prepareHandler:
	LDR	R2, [R6,#InterruptHandler.handler]
	CMP	R2, #0
	BEQ	HandleIRQ_doneWithInterrupt
	LDR	R0, [R6,#InterruptHandler.token]

HandleIRQ_callHandler:
	BLX	R2

HandleIRQ_doneWithInterrupt:
	MOV	R3, #1
	STR	R3, [R5]			// This write to VICADDRESS indicates to the VIC hardware that the interrupt has been serviced

HandleIRQ_return:
	POP	{R4,R5,R6,PC}


HandleFIQ:					// This function shouldn't actually be called, since we never use FIQs. This handler is included
						// for completeness

	PUSH	{R4,LR}
	LDR	R0, =(VIC0 + VICIRQSTATUS)
	LDR	R0, [R0]
	CMP	R0, #0
	BEQ	HandleFIQ_checkVIC1
	MOV	R4, #0
	B	HandleFIQ_callHandler

HandleFIQ_checkVIC1:
	LDR	R0, =(VIC1 + VICIRQSTATUS)
	LDR	R0, [R0]
	CMP	R0, #0
	BEQ	HandleFIQ_return
	MOV	R4, #VIC_InterruptSeparator

HandleFIQ_callHandler:
	BL	__ctzsi2			// Get correct bit in the status flag
	LDR	R2, =InterruptHandlerTable
	ADD	R0, R0,	R4			// Index into the InterruptHandlerTable
	LSL	R3, R0,	#1
	ADD	R0, R3,	R0
	LSL	R0, R0,	#2
	ADD	R0, R0,	R2

	LDR	R3, [R0,#InterruptHandler.handler]
	CMP	R3, #0				// Don't call if handler address is NULL
	BEQ	HandleFIQ_return
	LDR	R0, [R0,#InterruptHandler.token]
	BLX	R3				// call the handler

HandleFIQ_return:
	POP	{R4,PC}


//
//	Exception handlers (unexpected: something's wrong!)
//

.code 32
ArmUndefinedHandler:
	STMFD	SP!, {R0-R12,LR}
	MOV	R0, SP
	BLX	ThumbUndefinedHandler
	LDMFD	SP!, {R0-R12,PC}^


ArmSyscall:
	STMFD	SP!, {R0-R12,LR}
	MRS	R0, SPSR
	STMFD	SP!, {R0}
	MOV	R0, SP
	BLX	ThumbSyscall
	LDMFD	SP!, {R0}
	MSR	SPSR_cf, R0
	LDMFD	SP!, {R0-R12,PC}^


ArmPrefetchAbortHandler:
	SUB	LR, LR,	#4
	STMFD	SP!, {R0-R12,LR}
	MRS	R0, SPSR
	STMFD	SP!, {R0}
	MOV	R0, SP
	BLX	ThumbPrefetchAbortHandler
	LDMFD	SP!, {R0}
	MSR	SPSR_cf, R0
	LDMFD	SP!, {R0-R12,PC}^


ArmDataAbortHandler:
	SUB	LR, LR,	#8
	STMFD	SP!, {R0-R12,LR}
	MRS	R0, SPSR
	STMFD	SP!, {R0}
	MOV	R0, SP
	BLX	ThumbDataAbortHandler
	LDMFD	SP!, {R0}
	MSR	SPSR_cf, R0
	LDMFD	SP!, {R0-R12,PC}^


ArmReservedHandler:
	STMFD	SP!, {R0-R12,LR}
	MOV	R0, SP
	BLX	ThumbReservedHandler
	LDMFD	SP!, {R0-R12,PC}^

.code 16
ThumbUndefinedHandler:
	BL	panic
	BX	LR


ThumbSyscall:
	BL	panic
	BX	LR


ThumbPrefetchAbortHandler:
	BL	panic
	BX	LR


ThumbDataAbortHandler:
	BL	panic
	BX	LR


ThumbReservedHandler:
	BL	panic
	BX	LR


panic:
	B	panic

.data

InterruptHandlerTable:
	.rept	0x40
	.word	0x0	// address
	.word	0x0	// token
	.word	0x0	// useEdgeIC
	.endr

ExceptionStackEnd:
	.rept	0x800
	.byte	0x0
	.endr
ExceptionStack:

#ifdef SMALL
GeneralStackEnd:
	.rept	0x800
	.byte	0x0
	.endr
GeneralStack:
#endif

.bss

OpenIBootEnd:

