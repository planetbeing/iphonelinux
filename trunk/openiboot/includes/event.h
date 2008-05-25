#ifndef EVENT_H
#define EVENT_H

#include "openiboot.h"

extern Event EventList;

int event_setup();
int event_add(Event* newEvent, uint64_t timeout, EventHandler handler, void* opaque);
int event_readd(Event* event, uint64_t new_interval);

#endif
