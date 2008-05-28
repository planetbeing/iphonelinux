#ifndef USB_H
#define USB_H

typedef enum USBState {
	USBStart = 0,
	USBPowered = 1,
	USBDefault = 2,
	USBAddress = 3,
	USBConfigured = 4
} USBState;

typedef enum USBDirection {
	USBOut = 0,
	USBIn = 1,
	USBBiDir = 2
} USBDirection;

typedef void (*USBEndpointHandler)(uint32_t token);

typedef struct USBEndpointHandlerInfo {
	USBEndpointHandler	handler;
	uint32_t		token;
} USBEndpointHandlerInfo;

typedef struct USBEndpointBidirHandlerInfo {
	USBEndpointHandlerInfo in;
	USBEndpointHandlerInfo out;
} USBEndpointBidirHandlerInfo;

int usb_setup();
int usb_shutdown();

#endif

