#ifndef USB_H
#define USB_H

typedef enum USBState {
	USBStart = 0,
	USBPowered = 1,
	USBDefault = 2,
	USBAddress = 3,
	USBConfigured = 4
} USBState;

int usb_setup();
int usb_shutdown();

#endif

