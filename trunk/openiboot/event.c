#include "openiboot.h"
#include "timer.h"
#include "event.h"
#include "clock.h"
#include "s5l8900.h"

Event EventList = {{&EventList, &EventList}, 0, 0, NULL, 0};

static void init_event_list();
static void eventTimerHandler();

int event_setup() {
	// In our implementation, we set TicksPerSec when we setup the clock
	// so we don't have to do it here

	init_event_list();

	Timers[EventTimer].handler2 = eventTimerHandler;

	// Initialize the timer hardware for something that goes off once every 100 Hz.
	// The handler for the timer will reset it so it's periodic
	timer_init(EventTimer, TicksPerSec/100, 0, 0, FALSE, FALSE, FALSE);

	// Turn the timer on
	timer_on_off(EventTimer, ON);

	return 0;
}

static void init_event_list() {
	EventList.list.next = &EventList;
	EventList.list.prev = &EventList;
}

static void eventTimerHandler() {
	uint64_t curTime;
	Event* event;

	event = &EventList;
	curTime = timer_get_system_microtime();

	while(event->list.next != event) {
		if(curTime >= event->deadline) {
			((Event*)(event->list.next))->list.prev = event->list.prev;
			((Event*)(event->list.prev))->list.next = event->list.next;
			event->list.next = NULL;
			event->list.prev = NULL;
			event->handler(event, event->opaque);
		} else {
			break;
		}
	}
}
