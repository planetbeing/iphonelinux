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
#define CONTROL_SEND_BUFFER_LEN 0x80
#define CONTROL_RECV_BUFFER_LEN 0x80
#define TX_QUEUE_LEN 0x80

// one packet at a time
#define USB_MULTICOUNT 1

#define OPENIBOOT_INTERFACE_CLASS 0xFF
#define OPENIBOOT_INTERFACE_SUBCLASS 0xFF
#define OPENIBOOT_INTERFACE_PROTOCOL 0x51

#define USB_LANGID_ENGLISH_US 0x0409

#define USBError 0xEEE

typedef enum USBState {
	USBStart = 0,
	USBPowered = 1,
	USBDefault = 2,
	USBAddress = 3,
	USBConfigured = 4,

	// Values higher than USBError(0xEEE) are error conditions
	USBUnknownDescriptorRequest = 0xEEE,
	USBUnknownRequest = 0xEEF
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

enum USBDescriptorType {
	USBDeviceDescriptorType = 1,
	USBConfigurationDescriptorType = 2,
	USBStringDescriptorType = 3,
	USBInterfaceDescriptorType = 4,
	USBEndpointDescriptorType = 5,
	USBDeviceQualifierDescriptorType = 6
};

typedef enum USBSpeed {
	USBHighSpeed = 0,
	USBFullSpeed = 1,
	USBLowSpeed = 2
} USBSpeed;

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
} USBEPRegisters;

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

typedef struct USBDeviceQualifierDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize;
	uint8_t bNumConfigurations;
	uint8_t bReserved;
} __attribute__ ((__packed__)) USBDeviceQualifierDescriptor;

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

typedef struct USBSetupPacket {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__ ((__packed__)) USBSetupPacket;

typedef struct RingBuffer {
	int8_t* writePtr;
	int8_t* readPtr;
	uint32_t count;
	uint32_t size;
	int8_t* bufferStart;
	int8_t* bufferEnd;
} RingBuffer;

typedef void (*USBStartHandler)(void);
typedef void (*USBEnumerateHandler)(USBInterface* interface);

#define OPENIBOOTCMD_DUMPBUFFER 0
#define OPENIBOOTCMD_DUMPBUFFER_LEN 1
#define OPENIBOOTCMD_DUMPBUFFER_GOAHEAD 2
#define OPENIBOOTCMD_SENDCOMMAND 3
#define OPENIBOOTCMD_SENDCOMMAND_GOAHEAD 4

typedef struct OpenIBootCmd {
	uint32_t command;
	uint32_t dataLen;
}  __attribute__ ((__packed__)) OpenIBootCmd;

#define USBSetupPacketRequestTypeDirection(x) GET_BITS(x, 7, 1)
#define USBSetupPacketRequestTypeType(x) GET_BITS(x, 5, 2)
#define USBSetupPacketRequestTypeRecpient(x) GET_BITS(x, 0, 5)

#define USBSetupPacketHostToDevice 0
#define USBSetupPacketDeviceToHost 1
#define USBSetupPacketStandard 0
#define USBSetupPacketClass 1
#define USBSetupPacketVendor 2
#define USBSetupPacketRecpientDevice 0
#define USBSetupPacketRecpientInterface 1
#define USBSetupPacketRecpientEndpoint 2
#define USBSetupPacketRecpientOther 3

#define USB_CLEAR_FEATURE 1
#define USB_GET_CONFIGURATION 8
#define USB_GET_DESCRIPTOR 6
#define USB_GET_INTERFACE 10
#define USB_GET_STATUS 0
#define USB_SET_ADDRESS 5
#define USB_SET_CONFIGURATION 9
#define USB_SET_DESCRIPTOR 7
#define USB_SET_FEATURE 3
#define USB_SET_INTERFACE 11
#define USB_SYNCH_FRAME 12

int usb_setup();
int usb_start(USBEnumerateHandler hEnumerate, USBStartHandler hStart);
int usb_shutdown();
int usb_install_ep_handler(int endpoint, USBDirection direction, USBEndpointHandler handler, uint32_t token);
void usb_add_endpoint(USBInterface* interface, int endpoint, USBDirection direction, USBTransferType transferType);
void usb_send_interrupt(uint8_t endpoint, void* buffer, int bufferLen);
void usb_send_bulk(uint8_t endpoint, void* buffer, int bufferLen);
void usb_receive_bulk(uint8_t endpoint, void* buffer, int bufferLen);
void usb_receive_interrupt(uint8_t endpoint, void* buffer, int bufferLen);
USBSpeed usb_get_speed();

USBDeviceDescriptor* usb_get_device_descriptor();
USBDeviceQualifierDescriptor* usb_get_device_qualifier_descriptor();
USBConfigurationDescriptor* usb_get_configuration_descriptor(int index, uint8_t speed_id);
USBStringDescriptor* usb_get_string_descriptor(int index);

#endif

