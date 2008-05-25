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
		if(event->list.next == NULL) {
			break;
		}

		event = event->list.next;

		if(curTime >= event->deadline) {
			// take it off the list and dispatch it
			((Event*)(event->list.next))->list.prev = event->list.prev;
			((Event*)(event->list.prev))->list.next = event->list.next;
			event->list.next = NULL;
			event->list.prev = NULL;
			event->handler(event, event->opaque);
		} else {
			// The event queue is sorted, so we can just stop looping on the first
			// event that hasn't been triggered
			break;
		}
	}
}

int event_add(Event* newEvent, uint64_t timeout, EventHandler handler, void* opaque) {
	EnterCriticalSection();

	// If this item is already on a list, take it off
	if(newEvent->list.prev != NULL || newEvent->list.next != NULL) {
		if(newEvent->list.next != NULL) {
			((Event*)(newEvent->list.next))->list.prev = newEvent->list.prev;
		}
		if(newEvent->list.prev != NULL) {
			((Event*)(newEvent->list.prev))->list.next = newEvent->list.next;
		}
		newEvent->list.next = NULL;
		newEvent->list.prev = NULL;
	}

	newEvent->interval = timeout;
	newEvent->deadline = timer_get_system_microtime() + timeout;

	Event* insertAt = &EventList;

	while(insertAt != insertAt->list.next) {
		// Find the event that this occurs after
		if(insertAt->deadline > newEvent->deadline) {
			break;
		}
		insertAt = insertAt->list.next;
		if(insertAt == &EventList) {
			// We're after the whole list, so just insert after everyone else (after head)
			break;
		}
	}

	// Insert before insertAt
	newEvent->list.next = insertAt;
	newEvent->list.prev = insertAt->list.prev;
	((Event*)insertAt->list.prev)->list.next = newEvent;
	insertAt->list.prev = newEvent;

	LeaveCriticalSection();

	return 0;
}

int event_readd(Event* event, uint64_t new_interval) {
	EnterCriticalSection();

	// If this item is already on a list, take it off
	if(event->list.prev != NULL || event->list.next != NULL) {
		if(event->list.next != NULL) {
			((Event*)(event->list.next))->list.prev = event->list.prev;
		}
		if(event->list.prev != NULL) {
			((Event*)(event->list.prev))->list.next = event->list.next;
		}
		event->list.next = NULL;
		event->list.prev = NULL;
	}

	uint64_t interval;
	if(new_interval == 0) {
		if(event->interval == 0) {
			LeaveCriticalSection();
			return -1;
		} else {
			interval = event->interval;
		}
	} else {
		interval = new_interval;
	}

	event_add(event, interval, event->handler, event->opaque);
	LeaveCriticalSection();
}
