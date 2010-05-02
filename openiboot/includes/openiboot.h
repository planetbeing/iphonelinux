#ifndef OPENIBOOT_H
#define OPENIBOOT_H

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef signed long long int64_t;
typedef signed int int32_t;
typedef signed short int16_t;
typedef signed char int8_t;
typedef long unsigned int size_t;
typedef signed int intptr_t;

#ifdef DEBUG
#define OPENIBOOT_VERSION_DEBUG " (DEBUG)"
#else
#define OPENIBOOT_VERSION_DEBUG ""
#endif

#ifdef CONFIG_IPHONE
#define OPENIBOOT_VERSION_CONFIG " for iPhone 2G"
#endif
#ifdef CONFIG_IPOD
#define OPENIBOOT_VERSION_CONFIG " for iPod touch 1G"
#endif
#ifdef CONFIG_3G
#define OPENIBOOT_VERSION_CONFIG " for iPhone 3G"
#endif

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(s) #s
#define OPENIBOOT_VERSION_STR "openiboot " XSTRINGIFY(OPENIBOOT_VERSION) " commit " XSTRINGIFY(OPENIBOOT_VERSION_BUILD) OPENIBOOT_VERSION_DEBUG OPENIBOOT_VERSION_CONFIG

extern void* _start;
extern void* OpenIBootEnd;
extern int received_file_size;

typedef enum Boolean {
	FALSE = 0,
	TRUE = 1
} Boolean;

typedef enum OnOff {
	OFF = 0,
	ON = 1
} OnOff;

#ifndef NULL
#define NULL 0
#endif
#define uSecPerSec 1000000

typedef struct Event Event;

typedef void (*TaskRoutineFunction)(void* opaque);
typedef void (*EventHandler)(Event* event, void* opaque);

typedef struct LinkedList {
	void* prev;
	void* next;
} __attribute__ ((packed)) LinkedList;

typedef struct TaskRegisterState {
	uint32_t	r4;
	uint32_t	r5;
	uint32_t	r6;
	uint32_t	r7;
	uint32_t	r8;
	uint32_t	r9;
	uint32_t	r10;
	uint32_t	r11;
	uint32_t	sp;
	uint32_t	lr;
} __attribute__ ((packed)) TaskRegisterState;

typedef enum TaskState {
	TASK_READY = 1,
	TASK_RUNNING = 2,
	TASK_SLEEPING = 4,
	TASK_STOPPED = 5
} TaskState;

#define TaskDescriptorIdentifier1 0x7461736b
#define TaskDescriptorIdentifier2 0x74736b32

struct Event {
	LinkedList	list;
	uint64_t	deadline;
	uint64_t	interval;
	EventHandler	handler;
	void*		opaque;
};

typedef struct TaskDescriptor {
	uint32_t		identifier1;
	LinkedList		taskList;
	LinkedList		runqueueList;
	TaskState		state;
	uint32_t		criticalSectionNestCount;
	TaskRegisterState	savedRegisters;
	Event			sleepEvent;
	LinkedList		linked_list_3;
	uint32_t		exitState;
	TaskRoutineFunction	taskRoutine;
	void*			unknown_passed_value;
	void*			storage;
	uint32_t		storageSize;
	char			taskName[16];
	uint32_t		identifier2;
} __attribute__ ((packed)) TaskDescriptor;

extern TaskDescriptor* CurrentRunning;

/*
 *	Macros
 */

#define GET_REG(x) (*((volatile uint32_t*)(x)))
#define SET_REG(x, y) (*((volatile uint32_t*)(x)) = (y))
#define GET_REG32(x) GET_REG(x)
#define SET_REG32(x, y) SET_REG(x, y)
#define GET_REG16(x) (*((volatile uint16_t*)(x)))
#define SET_REG16(x, y) (*((volatile uint16_t*)(x)) = (y))
#define GET_REG8(x) (*((volatile uint8_t*)(x)))
#define SET_REG8(x, y) (*((volatile uint8_t*)(x)) = (y))
#define GET_BITS(x, start, length) ((((uint32_t)(x)) << (32 - ((start) + (length)))) >> (32 - (length)))

#endif
