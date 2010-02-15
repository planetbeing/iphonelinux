#include "openiboot.h"
#include "wdt.h"
#include "interrupt.h"
#include "hardware/s5l8900.h"
#include "util.h"
#include "event.h"
#include "clock.h"

#define PRESCALE 15
#define CLOCK_CS 4
#define CLOCK_DIV 2048

static int count;
static int period;

static Event WDTEvent;

static void wdt_handler(Event* event, void* opaque)
{
	SET_REG(WDT_CTRL, WDT_CLR | WDT_ENABLE | ((15 & WDT_PRE_MASK) << WDT_PRE_SHIFT) | ((4 & WDT_CS_MASK) << WDT_CS_SHIFT));
	event_readd(event, period);
	++count;
}

int wdt_setup()
{
	period = 1000000 * 2048 / PeripheralFrequency * (PRESCALE + 1) * CLOCK_DIV / 4;
	count = 0;
	event_add(&WDTEvent, period, wdt_handler, NULL);
	SET_REG(WDT_CTRL, WDT_CLR | WDT_ENABLE | ((PRESCALE & WDT_PRE_MASK) << WDT_PRE_SHIFT) | ((CLOCK_CS & WDT_CS_MASK) << WDT_CS_SHIFT));
	return 0;
}

int wdt_counter()
{
	return count;
}
