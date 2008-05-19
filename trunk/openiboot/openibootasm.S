.include "openibootasm.h"
.include "s5l8900asm.h"

.global IncrementCriticalLock
.global DecrementCriticalLock
.global EnterCriticalSection
.global LeaveCriticalSection

.global EnableCPUIRQ
.global EnableCPUFIQ
.global DisableCPUIRQ
.global DisableCPUFIQ

.global CurrentRunning

.text

#
#	Utility functions
#

.code 16
 .thumb_func
IncrementCriticalLock:
	LDR	R3, =CurrentRunning
	LDR	R2, [R3]
	LDR	R3, [R2,#TaskDescriptor.criticalSectionNestCount]
	ADD	R3, #1
	STR	R3, [R2,#TaskDescriptor.criticalSectionNestCount]
	BX	LR


.thumb_func
DecrementCriticalLock:
	LDR	R3, =CurrentRunning
	LDR	R2, [R3]
	LDR	R3, [R2,#TaskDescriptor.criticalSectionNestCount]
	SUB	R3, #1
	STR	R3, [R2,#TaskDescriptor.criticalSectionNestCount]
	BX	LR

.thumb_func
EnterCriticalSection:
	PUSH	{LR}

	BLX	DisableCPUIRQ
	BLX	DisableCPUFIQ

	LDR	R1, =CurrentRunning
	LDR	R2, [R1]
	LDR	R0, [R2,#TaskDescriptor.criticalSectionNestCount]
	ADD	R0, #1
	STR	R0, [R2,#TaskDescriptor.criticalSectionNestCount]

	POP	{PC}

.thumb_func
LeaveCriticalSection:
	PUSH	{LR}

	LDR	R1, =CurrentRunning
	LDR	R2, [R1]
	LDR	R0, [R2,#TaskDescriptor.criticalSectionNestCount]
	SUB	R0, #1
	STR	R0, [R2,#TaskDescriptor.criticalSectionNestCount]
	BNE	LeaveCriticalSection_return				@ check the Zero flag set by the previous SUB operation
									@ remember in thumb mode, everything has set flag set

	BLX	EnableCPUFIQ
	BLX	EnableCPUIRQ

LeaveCriticalSection_return:
	POP	{PC}

.code 32
EnableCPUIRQ:
	MRS	R0, CPSR
	BIC	R1, R0,	#0x80
	MSR	CPSR_c,	R1
	AND	R0, R0,	#0x80
	BX	LR

EnableCPUFIQ:
	MRS	R0, CPSR
	BIC	R0, R0,	#0x40
	MSR	CPSR_c,	R0
	BX	LR

DisableCPUIRQ:
	MRS	R0, CPSR
	ORR	R1, R0,	#0x80
	MSR	CPSR_c,	R1
	AND	R0, R0,	#0x80
	BX	LR

DisableCPUFIQ:
	MRS	R0, CPSR
	ORR	R0, R0,	#0x40
	MSR	CPSR_c,	R0
	BX	LR

.data

CurrentRunning:
	.word	0x0

