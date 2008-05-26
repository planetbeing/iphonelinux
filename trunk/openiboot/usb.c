#include "openiboot.h"
#include "usb.h"
#include "power.h"
#include "util.h"
#include "hardware/power.h"
#include "hardware/usb.h"

int usb_setup() {
	return 0;
}

int usb_shutdown() {
	power_ctrl(POWER_USB, ON);
	clock_gate_switch(USB_OTGCLOCKGATE, ON);
	clock_gate_switch(USB_PHYCLOCKGATE, ON);

	SET_REG(USB + USB_ONOFF, GET_REG(USB + USB_ONOFF) | USB_ONOFF_OFF); // reset link
	SET_REG(USB_PHY + OPHYPWR, OPHYPWR_FORCESUSPEND | OPHYPWR_PLLPOWERDOWN
		| OPHYPWR_XOPOWERDOWN | OPHYPWR_ANALOGPOWERDOWN | OPHYPWR_UNKNOWNPOWERDOWN); // power down phy

	SET_REG(USB_PHY + ORSTCON, ORSTCON_PHYSWRESET | ORSTCON_LINKSWRESET | ORSTCON_PHYLINKSWRESET); // reset phy/link

	udelay(1000);	// wait a millisecond for the changes to stick

	clock_gate_switch(USB_OTGCLOCKGATE, OFF);
	clock_gate_switch(USB_PHYCLOCKGATE, OFF);
	power_ctrl(POWER_USB, OFF);
	return 0;
}

