#ifndef USB_H
#define USB_H

// assigned by USB Org
#define VENDOR_APPLE 0x5AC

// assigned by Apple
#define PRODUCT_IPHONE 0x1280
#define DEVICE_IPHONE 0x1103

// values we're using
#define USB_MAX_PACKETSIZE 64
#define USB_SETUP_PACKETS_AT_A_TIME 1
#define USB_SEND_BUFFER_LEN 0x80
#define USB_RECV_BUFFER_LEN 0x80

#define OPENIBOOT_INTERFACE_CLASS 0xFF
#define OPENIBOOT_INTERFACE_SUBCLASS 0xFF
#define OPENIBOOT_INTERFACE_PROTOCOL 0x51

#define USB_LANGID_ENGLISH_US 0x0409

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

typedef enum USBTransferType {
	USBControl = 0,
	USBIsochronous = 1,
	USBBulk = 2,
	USBInterrupt = 3
} USBTransferType;

typedef enum USBSynchronisationType {
	USBNoSynchronization = 0,
	USBAsynchronous = 1,
	USBAdaptive = 2,
	USBSynchronous = 3
} USBSynchronisationType;

typedef enum USBUsageType {
	USBDataEndpoint = 0,
	USBFeedbackEndpoint = 1,
	USBExplicitFeedbackEndpoint = 2
} USBUsageType;

enum USBDescrptorType {
	USBDeviceDescriptorType = 1,
	USBConfigurationDescriptorType = 2,
	USBStringDescriptorType = 3,
	USBInterfaceDescriptorType = 4,
	USBEndpointDescriptorType = 5
};

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
	volatile uint32_t control;
	volatile uint32_t field_4;
	volatile uint32_t interrupt;
	volatile uint32_t field_8;
	volatile uint32_t transferSize;
	volatile void* dmaAddress;
	volatile uint32_t field_18;
	volatile uint32_t field_1C;
} __attribute__ ((__packed__)) USBEPRegisters;

typedef struct USBDeviceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} __attribute__ ((__packed__)) USBDeviceDescriptor;

typedef struct USBConfigurationDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__ ((__packed__)) USBConfigurationDescriptor;

typedef struct USBInterfaceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __attribute__ ((__packed__)) USBInterfaceDescriptor;

typedef struct USBEndpointDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __attribute__ ((__packed__)) USBEndpointDescriptor;

typedef struct USBStringDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	char bString[];
} __attribute__ ((__packed__)) USBStringDescriptor;

typedef struct USBFirstStringDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wLANGID[];
} __attribute__ ((__packed__)) USBFirstStringDescriptor;

typedef struct USBInterface {
	USBInterfaceDescriptor descriptor;
	USBEndpointDescriptor* endpointDescriptors;
} USBInterface;

typedef struct USBConfiguration {
	USBConfigurationDescriptor descriptor;
	USBInterface* interfaces;
} USBConfiguration;

int usb_setup();
int usb_shutdown();
int usb_install_ep_handler(int endpoint, USBDirection direction, USBEndpointHandler handler, uint32_t token);

USBDeviceDescriptor* usb_get_device_descriptor();
USBConfigurationDescriptor* usb_get_configuration_descriptor(int index, uint8_t speed_id);
USBStringDescriptor* usb_get_string_descriptor(int index);

#endif

