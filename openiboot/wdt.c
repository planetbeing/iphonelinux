#include "openiboot.h"
#include "openiboot-asmhelpers.h"
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

static volatile int wdt_enabled;
static volatile int wdt_disabled;
static int wdt_init = FALSE;

static void wdt_handler(Event* event, void* opaque)
{
	EnterCriticalSection();
	if(wdt_enabled)
	{
		SET_REG(WDT_CTRL, WDT_CLR | WDT_ENABLE | ((PRESCALE & WDT_PRE_MASK) << WDT_PRE_SHIFT) | ((CLOCK_CS & WDT_CS_MASK) << WDT_CS_SHIFT));
		event_readd(event, period);
		++count;
	}
	else
	{
		// shouldn't be necessary
		wdt_disable();
		wdt_disabled = TRUE;
	}
	LeaveCriticalSection();
}

int wdt_setup()
{
	wdt_init = TRUE;
	wdt_disabled = TRUE;
	wdt_enabled = FALSE;
	wdt_enable();
	return 0;
}

void wdt_enable()
{
	if(!wdt_init)
		return;
	
	EnterCriticalSection();
	if(wdt_disabled)
	{
		period = 1000000 * 2048 / PeripheralFrequency * (PRESCALE + 1) * CLOCK_DIV / 4;
		count = 0;
		event_add(&WDTEvent, period, wdt_handler, NULL);
		SET_REG(WDT_CTRL, WDT_CLR | WDT_ENABLE | ((PRESCALE & WDT_PRE_MASK) << WDT_PRE_SHIFT) | ((CLOCK_CS & WDT_CS_MASK) << WDT_CS_SHIFT));
	}
	
	wdt_enabled = TRUE;
	LeaveCriticalSection();
}

void wdt_disable()
{
	if(!wdt_init)
		return;
	
	EnterCriticalSection();
	if(wdt_enabled)
	{	
		SET_REG(WDT_CTRL, WDT_CLR | WDT_DIS);
		wdt_enabled = FALSE;
	}
	LeaveCriticalSection();
}

int wdt_counter()
{
	return count;
}
