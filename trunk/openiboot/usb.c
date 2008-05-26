#include "openiboot.h"
#include "usb.h"
#include "power.h"
#include "hardware/power.h"
#include "hardware/usb.h"

int usb_setup() {
	return 0;
}

int usb_shutdown() {
	power_ctrl(POWER_USB, ON);
	clock_gate_switch(USB_OTGCLOCKGATE, ON);
	clock_gate_switch(USB_PHYCLOCKGATE, ON);

	power_ctrl(POWER_USB, OFF);
	return 0;
}

