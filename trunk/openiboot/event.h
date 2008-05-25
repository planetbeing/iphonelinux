#ifndef EVENT_H
#define EVENT_H

#include "openiboot.h"

extern Event EventList;

int event_setup();
void event_add(Event* newEvent, uint64_t timeout, EventHandler handler, void* opaque);

#endif
