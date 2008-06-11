#include "openiboot.h"
#include "usb.h"
#include "power.h"
#include "util.h"
#include "hardware/power.h"
#include "hardware/usb.h"
#include "timer.h"
#include "clock.h"
#include "interrupt.h"

static void change_state(USBState new_state);

static Boolean usb_inited;

static USBState usb_state;
static USBDirection endpoint_directions[USB_NUM_ENDPOINTS];
static USBEndpointBidirHandlerInfo endpoint_handlers[USB_NUM_ENDPOINTS];

volatile USBEPRegisters* InEPRegs;
volatile USBEPRegisters* OutEPRegs;

static void usbIRQHandler(uint32_t token) {

}

int usb_setup() {
	int i;

	if(usb_inited) {
		return 0;
	}

	InEPRegs = (USBEPRegisters*)(USB + USB_INREGS);
	OutEPRegs = (USBEPRegisters*)(USB + USB_OUTREGS);

	change_state(USBStart);

	// Power on hardware
	power_ctrl(POWER_USB, ON);
	udelay(USB_START_DELAYUS);

	// Initialize our data structures
	for(i = 0; i < USB_NUM_ENDPOINTS; i++) {
		switch(USB_EP_DIRECTION(i)) {
			case USB_ENDPOINT_DIRECTIONS_BIDIR:
				endpoint_directions[i] = USBBiDir;
				break;
			case USB_ENDPOINT_DIRECTIONS_IN:
				endpoint_directions[i] = USBIn;
				break;
			case USB_ENDPOINT_DIRECTIONS_OUT:
				endpoint_directions[i] = USBOut;
				break;
		}
	}

	memset(endpoint_handlers, 0, sizeof(endpoint_handlers));

	// Set up the hardware
	clock_gate_switch(USB_OTGCLOCKGATE, ON);
	clock_gate_switch(USB_PHYCLOCKGATE, ON);
	clock_gate_switch(EDRAM_CLOCKGATE, ON);

	// Generate a soft disconnect on host
	SET_REG(USB + DCTL, GET_REG(USB + DCTL) | DCTL_SFTDISCONNECT);
	udelay(USB_SFTDISCONNECT_DELAYUS);

	// power on OTG
	SET_REG(USB + USB_ONOFF, GET_REG(USB + USB_ONOFF) & (~USB_ONOFF_OFF));
	udelay(USB_ONOFFSTART_DELAYUS);

	// power on PHY
	SET_REG(USB_PHY + OPHYPWR, OPHYPWR_POWERON);
	udelay(USB_PHYPWRPOWERON_DELAYUS);

	// select clock
	SET_REG(USB_PHY + OPHYCLK, (GET_REG(USB_PHY + OPHYCLK) & OPHYCLK_CLKSEL_MASK) | OPHYCLK_CLKSEL_48MHZ);

	// reset phy
	SET_REG(USB_PHY + ORSTCON, GET_REG(USB_PHY + ORSTCON) | ORSTCON_PHYSWRESET);
	udelay(USB_RESET2_DELAYUS);
	SET_REG(USB_PHY + ORSTCON, GET_REG(USB_PHY + ORSTCON) & (~ORSTCON_PHYSWRESET));
	udelay(USB_RESET_DELAYUS);

	SET_REG(USB + GRSTCTL, GRSTCTL_CORESOFTRESET);

	// wait until reset takes
	while((GET_REG(USB + GRSTCTL) & GRSTCTL_CORESOFTRESET) == GRSTCTL_CORESOFTRESET);

	// wait until reset completes
	while(GET_REG(USB + GRSTCTL) >= 0);

	udelay(USB_RESETWAITFINISH_DELAYUS);

	// allow host to reconnect
	SET_REG(USB + DCTL, GET_REG(USB + DCTL) & (~DCTL_SFTDISCONNECT));
	udelay(USB_SFTCONNECT_DELAYUS);

	// flag all interrupts as positive, maybe to disable them

	// Set 7th EP? This is what iBoot does
	InEPRegs[USB_NUM_ENDPOINTS].interrupt = USB_EPINT_INEPNakEff | USB_EPINT_INTknEPMis | USB_EPINT_INTknTXFEmp
		| USB_EPINT_TimeOUT | USB_EPINT_AHBErr | USB_EPINT_EPDisbld | USB_EPINT_XferCompl;
	OutEPRegs[USB_NUM_ENDPOINTS].interrupt = USB_EPINT_OUTTknEPDis
		| USB_EPINT_SetUp | USB_EPINT_AHBErr | USB_EPINT_EPDisbld | USB_EPINT_XferCompl;

	for(i = 0; i < USB_NUM_ENDPOINTS; i++) {
		InEPRegs[i].interrupt = USB_EPINT_INEPNakEff | USB_EPINT_INTknEPMis | USB_EPINT_INTknTXFEmp
			| USB_EPINT_TimeOUT | USB_EPINT_AHBErr | USB_EPINT_EPDisbld | USB_EPINT_XferCompl;
		OutEPRegs[i].interrupt = USB_EPINT_OUTTknEPDis
			| USB_EPINT_SetUp | USB_EPINT_AHBErr | USB_EPINT_EPDisbld | USB_EPINT_XferCompl;
	}

	// disable all interrupts until endpoint descriptors and configuration structures have been setup
	SET_REG(USB + GINTMSK, GINTMSK_NONE);
	SET_REG(USB + DIEPMSK, DIEPMSK_NONE);
	SET_REG(USB + DOEPMSK, DOEPMSK_NONE);

	interrupt_install(USB_INTERRUPT, usbIRQHandler, 0);
	interrupt_enable(USB_INTERRUPT);

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

	udelay(USB_RESET_DELAYUS);	// wait a millisecond for the changes to stick

	clock_gate_switch(USB_OTGCLOCKGATE, OFF);
	clock_gate_switch(USB_PHYCLOCKGATE, OFF);
	power_ctrl(POWER_USB, OFF);
	return 0;
}

static void change_state(USBState new_state) {
	usb_state = new_state;
	if(usb_state == USBConfigured) {
		// TODO: set to host powered
	}
}

