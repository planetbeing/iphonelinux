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

typedef struct USBEPRegisters {
	uint32_t control;
	uint32_t field_4;
	uint32_t interrupt;
	uint32_t field_8;
	uint32_t transferSize;
	uint32_t dmaAddress;
	uint32_t field_18;
	uint32_t field_1C;
} __attribute__ ((__packed__)) USBEPRegisters;

int usb_setup();
int usb_shutdown();

#endif

