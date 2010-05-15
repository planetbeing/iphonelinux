#include "openiboot.h"
#include "piezo.h"
#include "timer.h"
#include "clock.h"
#include "hardware/timer.h"

void piezo_buzz(int hertz, unsigned int microseconds)
{
	// The second option represents the duty cycle of the PWM that drives the buzzer. In this case it is always 50%.
	timer_init(PiezoTimer, TicksPerSec / hertz / 2, TicksPerSec / hertz, 0, 0, FALSE, FALSE, FALSE, FALSE);
	timer_on_off(PiezoTimer, ON);
	udelay(microseconds);
	timer_on_off(PiezoTimer, OFF);
}
