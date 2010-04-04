#ifndef WDT_H
#define WDT_H

#include "openiboot.h"

int wdt_setup();
int wdt_counter();
void wdt_enable();
void wdt_disable();

#endif
