#include "openiboot.h"
#include "buttons.h"
#include "hardware/buttons.h"
#include "pmu.h"
#include "gpio.h"

int buttons_is_pushed(int which) {
	if(gpio_pin_state(which) && pmu_get_reg(BUTTONS_IIC_STATE))
		return TRUE;
	else
		return FALSE;
}
