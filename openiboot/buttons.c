#include "openiboot.h"
#include "buttons.h"
#include "hardware/buttons.h"
#include "pmu.h"
#include "gpio.h"

int buttons_is_home_pushed() {
	if(gpio_pin_state(BUTTONS_HOME) && pmu_get_reg(BUTTONS_IIC_STATE))
		return TRUE;
	else
		return FALSE;
}

int buttons_is_hold_pushed() {
	if(gpio_pin_state(BUTTONS_HOLD) && pmu_get_reg(BUTTONS_IIC_STATE))
		return TRUE;
	else
		return FALSE;
}

