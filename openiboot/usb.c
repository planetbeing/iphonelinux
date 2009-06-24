#include "openiboot.h"
#include "usb.h"
#include "power.h"
#include "util.h"
#include "hardware/power.h"
#include "hardware/usb.h"
#include "timer.h"
#include "clock.h"
#include "interrupt.h"
#include "openiboot-asmhelpers.h"

static void change_state(USBState new_state);

static Boolean usb_inited;

static USBState usb_state;
static uint8_t usb_speed;
static uint8_t usb_max_packet_size;
static USBDirection endpoint_directions[USB_NUM_ENDPOINTS];
static USBEndpointBidirHandlerInfo endpoint_handlers[USB_NUM_ENDPOINTS];
static uint32_t inInterruptStatus[USB_NUM_ENDPOINTS];
static uint32_t outInterruptStatus[USB_NUM_ENDPOINTS];

USBEPRegisters* InEPRegs;
USBEPRegisters* OutEPRegs;

static uint8_t currentlySending;

static USBDeviceDescriptor deviceDescriptor;
static USBDeviceQualifierDescriptor deviceQualifierDescriptor;

static uint8_t numStringDescriptors;
static USBStringDescriptor** stringDescriptors;
static USBFirstStringDescriptor* firstStringDescriptor;

static USBConfiguration* configurations;

static uint8_t* controlSendBuffer = NULL;
static uint8_t* controlRecvBuffer = NULL;

static RingBuffer* txQueue = NULL;

static USBEnumerateHandler enumerateHandler;
static USBStartHandler startHandler;

static void usbIRQHandler(uint32_t token);

static void initializeDescriptors();

static uint8_t addConfiguration(uint8_t bConfigurationValue, uint8_t iConfiguration, uint8_t selfPowered, uint8_t remoteWakeup, uint16_t maxPower);

static void endConfiguration(USBConfiguration* configuration);

static USBInterface* addInterfaceDescriptor(USBConfiguration* configuration, uint8_t interface, uint8_t bAlternateSetting, uint8_t bInterfaceClass, uint8_t bInterfaceSubClass, uint8_t bInterfaceProtocol, uint8_t iInterface);

static uint8_t addEndpointDescriptor(USBInterface* interface, uint8_t endpoint, USBDirection direction, USBTransferType transferType, USBSynchronisationType syncType, USBUsageType usageType, uint16_t wMaxPacketSize, uint8_t bInterval);

static void releaseConfigurations();

static uint8_t addStringDescriptor(const char* descriptorString);
static void releaseStringDescriptors();
static uint16_t packetsizeFromSpeed(uint8_t speed_id);

static void sendControl(void* buffer, int bufferLen);
static void receiveControl(void* buffer, int bufferLen);

static void receive(int endpoint, USBTransferType transferType, void* buffer, int packetLength, int bufferLen);

static int resetUSB();
static void getEndpointInterruptStatuses();
static void callEndpointHandlers();
static uint32_t getConfigurationTree(int i, uint8_t speed_id, void* buffer);
static void setConfiguration(int i);

static void handleTxInterrupts(int endpoint);
static void handleRxInterrupts(int endpoint);

static int advanceTxQueue();

static RingBuffer* createRingBuffer(int size);
static int8_t ringBufferDequeue(RingBuffer* buffer);
static int8_t ringBufferEnqueue(RingBuffer* buffer, uint8_t value);

static void usbTxRx(int endpoint, USBDirection direction, USBTransferType transferType, void* buffer, int bufferLen);

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
		bufferPrintf("EP %d: %d\r\n", i, endpoint_directions[i]);
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
	while((GET_REG(USB + GRSTCTL) & ~GRSTCTL_AHBIDLE) != 0);

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
	SET_REG(USB + DIEPMSK, USB_EPINT_NONE);
	SET_REG(USB + DOEPMSK, USB_EPINT_NONE);

	interrupt_install(USB_INTERRUPT, usbIRQHandler, 0);

	usb_inited = TRUE;

	return 0;
}

int usb_start(USBEnumerateHandler hEnumerate, USBStartHandler hStart) {
	enumerateHandler = hEnumerate;
	startHandler = hStart;
	currentlySending = 0xFF;

	if(txQueue == NULL)
		txQueue = createRingBuffer(TX_QUEUE_LEN);

	initializeDescriptors();

	if(controlSendBuffer == NULL)
		controlSendBuffer = memalign(DMA_ALIGN, CONTROL_SEND_BUFFER_LEN);

	if(controlRecvBuffer == NULL)
		controlRecvBuffer = memalign(DMA_ALIGN, CONTROL_RECV_BUFFER_LEN);

	SET_REG(USB + GAHBCFG, GAHBCFG_DMAEN | GAHBCFG_BSTLEN_INCR8 | GAHBCFG_MASKINT);
	SET_REG(USB + GUSBCFG, GUSBCFG_PHYIF16BIT | GUSBCFG_SRPENABLE | GUSBCFG_HNPENABLE | ((5 & GUSBCFG_TURNAROUND_MASK) << GUSBCFG_TURNAROUND_SHIFT));
	SET_REG(USB + DCFG, DCFG_HISPEED); // some random setting. See specs
	SET_REG(USB + DCFG, GET_REG(USB + DCFG) & ~(DCFG_DEVICEADDRMSK));
	InEPRegs[0].control = USB_EPCON_ACTIVE;
	OutEPRegs[0].control = USB_EPCON_ACTIVE;

	SET_REG(USB + GRXFSIZ, RX_FIFO_DEPTH);
	SET_REG(USB + GNPTXFSIZ, (TX_FIFO_DEPTH << GNPTXFSIZ_DEPTH_SHIFT) | TX_FIFO_STARTADDR);

	int i;
	for(i = 0; i < USB_NUM_ENDPOINTS; i++) {
		InEPRegs[i].interrupt = USB_EPINT_INEPNakEff | USB_EPINT_INTknEPMis | USB_EPINT_INTknTXFEmp
			| USB_EPINT_TimeOUT | USB_EPINT_AHBErr | USB_EPINT_EPDisbld | USB_EPINT_XferCompl;
		OutEPRegs[i].interrupt = USB_EPINT_OUTTknEPDis
			| USB_EPINT_SetUp | USB_EPINT_AHBErr | USB_EPINT_EPDisbld | USB_EPINT_XferCompl;
		
	}

	SET_REG(USB + GINTMSK, GINTMSK_OTG | GINTMSK_SUSPEND | GINTMSK_RESET | GINTMSK_INEP | GINTMSK_OEP | GINTMSK_DISCONNECT);
	SET_REG(USB + DAINTMSK, DAINTMSK_ALL);

	SET_REG(USB + DOEPMSK, USB_EPINT_XferCompl | USB_EPINT_SetUp | USB_EPINT_Back2BackSetup);
	SET_REG(USB + DIEPMSK, USB_EPINT_XferCompl | USB_EPINT_AHBErr | USB_EPINT_TimeOUT);

	InEPRegs[0].interrupt = USB_EPINT_ALL;
	OutEPRegs[0].interrupt = USB_EPINT_ALL;

	SET_REG(USB + DCTL, DCTL_PROGRAMDONE + DCTL_CGOUTNAK + DCTL_CGNPINNAK);
	udelay(USB_PROGRAMDONE_DELAYUS);
	SET_REG(USB + GOTGCTL, GET_REG(USB + GOTGCTL) | GOTGCTL_SESSIONREQUEST);

	receiveControl(controlRecvBuffer, sizeof(USBSetupPacket));

	change_state(USBPowered);

	interrupt_enable(USB_INTERRUPT);

	return 0;
}

static void receiveControl(void* buffer, int bufferLen) {
	CleanAndInvalidateCPUDataCache();
	receive(USB_CONTROLEP, USBControl, buffer, USB_MAX_PACKETSIZE, bufferLen);
}

static void usbTxRx(int endpoint, USBDirection direction, USBTransferType transferType, void* buffer, int bufferLen) {
	int packetLength;

	if(transferType == USBControl || transferType == USBInterrupt) {
		packetLength = USB_MAX_PACKETSIZE;
	} else {
		packetLength = packetsizeFromSpeed(usb_speed);
	}

	CleanAndInvalidateCPUDataCache();

	if(direction == USBOut) {
		receive(endpoint, transferType, buffer, packetLength, bufferLen);
		return;
	}

	if(GNPTXFSTS_GET_TXQSPCAVAIL(GET_REG(USB + GNPTXFSTS)) == 0) {
		// no space available
		return;
	}

	// enable interrupts for this endpoint
	SET_REG(USB + DAINTMSK, GET_REG(USB + DAINTMSK) | ((1 << endpoint) << DAINTMSK_IN_SHIFT));
	
	InEPRegs[endpoint].dmaAddress = buffer;

	if(endpoint == USB_CONTROLEP) {
		// we'll only send one packet at a time on CONTROLEP
		InEPRegs[endpoint].transferSize = ((1 & DEPTSIZ_PKTCNT_MASK) << DEPTSIZ_PKTCNT_SHIFT) | (bufferLen & DEPTSIZ0_XFERSIZ_MASK);
		InEPRegs[endpoint].control = USB_EPCON_CLEARNAK;
		return;
	}

	int packetCount = bufferLen / packetLength;
	if((bufferLen % packetLength) != 0)
		++packetCount;


	InEPRegs[endpoint].transferSize = ((packetCount & DEPTSIZ_PKTCNT_MASK) << DEPTSIZ_PKTCNT_SHIFT)
		| (bufferLen & DEPTSIZ_XFERSIZ_MASK) | ((USB_MULTICOUNT & DIEPTSIZ_MC_MASK) << DIEPTSIZ_MC_SHIFT);


	InEPRegs[endpoint].control = USB_EPCON_CLEARNAK | USB_EPCON_ACTIVE | ((transferType & USB_EPCON_TYPE_MASK) << USB_EPCON_TYPE_SHIFT) | (packetLength & USB_EPCON_MPS_MASK);
	
}

static void receive(int endpoint, USBTransferType transferType, void* buffer, int packetLength, int bufferLen) {
	if(endpoint == USB_CONTROLEP) {
		OutEPRegs[endpoint].transferSize = ((USB_SETUP_PACKETS_AT_A_TIME & DOEPTSIZ0_SUPCNT_MASK) << DOEPTSIZ0_SUPCNT_SHIFT)
			| ((USB_SETUP_PACKETS_AT_A_TIME & DOEPTSIZ0_PKTCNT_MASK) << DEPTSIZ_PKTCNT_SHIFT) | (bufferLen & DEPTSIZ0_XFERSIZ_MASK);
	} else {
		// divide our buffer into packets. Apple uses fancy bitwise arithmetic while we call huge libgcc integer arithmetic functions
		// for the sake of code readability. Will this matter?
		int packetCount = bufferLen / packetLength;
		if((bufferLen % packetLength) != 0)
			++packetCount;

		OutEPRegs[endpoint].transferSize = ((packetCount & DEPTSIZ_PKTCNT_MASK) << DEPTSIZ_PKTCNT_SHIFT) | (bufferLen & DEPTSIZ_XFERSIZ_MASK);
	}

	SET_REG(USB + DAINTMSK, GET_REG(USB + DAINTMSK) | (1 << (DAINTMSK_OUT_SHIFT + endpoint)));

	OutEPRegs[endpoint].dmaAddress = buffer;

	// start the transfer
	OutEPRegs[endpoint].control = USB_EPCON_ENABLE | USB_EPCON_CLEARNAK | USB_EPCON_ACTIVE
		| ((transferType & USB_EPCON_TYPE_MASK) << USB_EPCON_TYPE_SHIFT) | (packetLength & USB_EPCON_MPS_MASK);
}

static int resetUSB() {
	SET_REG(USB + DCFG, GET_REG(USB + DCFG) & ~DCFG_DEVICEADDRMSK);

	int endpoint;
	for(endpoint = 0; endpoint < USB_NUM_ENDPOINTS; endpoint++) {
		OutEPRegs[endpoint].control = OutEPRegs[endpoint].control | USB_EPCON_SETNAK;
	}

	// enable interrupts for endpoint 0
	SET_REG(USB + DAINTMSK, GET_REG(USB + DAINTMSK) | ((1 << USB_CONTROLEP) << DAINTMSK_OUT_SHIFT) | ((1 << USB_CONTROLEP) << DAINTMSK_IN_SHIFT));

	SET_REG(USB + DOEPMSK, USB_EPINT_XferCompl | USB_EPINT_SetUp | USB_EPINT_Back2BackSetup);
	SET_REG(USB + DIEPMSK, USB_EPINT_XferCompl | USB_EPINT_AHBErr | USB_EPINT_TimeOUT);

	receiveControl(controlRecvBuffer, sizeof(USBSetupPacket));

	return 0;
}

static void getEndpointInterruptStatuses() {
	// To not mess up the interrupt controller, we can only read the interrupt status once per interrupt, so we need to cache them here
	int endpoint;
	for(endpoint = 0; endpoint < USB_NUM_ENDPOINTS; endpoint++) {
		if(endpoint_directions[endpoint] == USBIn || endpoint_directions[endpoint] == USBBiDir) {
			inInterruptStatus[endpoint] = InEPRegs[endpoint].interrupt;
		}
		if(endpoint_directions[endpoint] == USBOut || endpoint_directions[endpoint] == USBBiDir) {
			outInterruptStatus[endpoint] = OutEPRegs[endpoint].interrupt;
		}
	}
}

static int isSetupPhaseDone() {
	uint32_t status = GET_REG(USB + DAINTMSK);
	int isDone = FALSE;

	if((status & ((1 << USB_CONTROLEP) << DAINTMSK_OUT_SHIFT)) == ((1 << USB_CONTROLEP) << DAINTMSK_OUT_SHIFT)) {
		if((outInterruptStatus[USB_CONTROLEP] & USB_EPINT_SetUp) == USB_EPINT_SetUp) {
			isDone = TRUE;
		}
	}

	// clear interrupt
	OutEPRegs[USB_CONTROLEP].interrupt = USB_EPINT_SetUp;

	return isDone;
}

static void callEndpointHandlers() {
	uint32_t status = GET_REG(USB + DAINTMSK);

	int endpoint;
	for(endpoint = 0; endpoint < USB_NUM_ENDPOINTS; endpoint++) {
		if((status & ((1 << endpoint) << DAINTMSK_OUT_SHIFT)) == ((1 << endpoint) << DAINTMSK_OUT_SHIFT)) {
			if((outInterruptStatus[endpoint] & USB_EPINT_XferCompl) == USB_EPINT_XferCompl) {
				if(endpoint_handlers[endpoint].out.handler != NULL) {
					endpoint_handlers[endpoint].out.handler(endpoint_handlers[endpoint].out.token);
				}
			}
		}

		if((status & ((1 << endpoint) << DAINTMSK_IN_SHIFT)) == ((1 << endpoint) << DAINTMSK_IN_SHIFT)) {
			if((inInterruptStatus[endpoint] & USB_EPINT_XferCompl) == USB_EPINT_XferCompl) {
				if(endpoint_handlers[endpoint].in.handler != NULL) {
					endpoint_handlers[endpoint].in.handler(endpoint_handlers[endpoint].in.token);
				}
			}
		}

		if(endpoint_directions[endpoint] == USBOut || endpoint_directions[endpoint] == USBBiDir) {
			handleRxInterrupts(endpoint);
		}

		if(endpoint_directions[endpoint] == USBIn || endpoint_directions[endpoint] == USBBiDir) {
			handleTxInterrupts(endpoint);
		}
	}
}

static void handleTxInterrupts(int endpoint) {
	if(!inInterruptStatus[endpoint]) {
		return;
	}

	//uartPrintf("\tendpoint %d has an TX interrupt\r\n", endpoint);

	// clear pending interrupts
	if((inInterruptStatus[endpoint] & USB_EPINT_INEPNakEff) == USB_EPINT_INEPNakEff) {
		InEPRegs[endpoint].interrupt = USB_EPINT_INEPNakEff;
		//uartPrintf("\t\tUSB_EPINT_INEPNakEff\r\n");
	}

	if((inInterruptStatus[endpoint] & USB_EPINT_INTknEPMis) == USB_EPINT_INTknEPMis) {
		InEPRegs[endpoint].interrupt = USB_EPINT_INTknEPMis;
		//uartPrintf("\t\tUSB_EPINT_INTknEPMis\r\n");

		// clear the corresponding core interrupt
		SET_REG(USB + GINTSTS, GET_REG(USB + GINTSTS) | GINTMSK_EPMIS);
	}

	if((inInterruptStatus[endpoint] & USB_EPINT_INTknTXFEmp) == USB_EPINT_INTknTXFEmp) {
		InEPRegs[endpoint].interrupt = USB_EPINT_INTknTXFEmp;
		//uartPrintf("\t\tUSB_EPINT_INTknTXFEmp\r\n");
	}

	if((inInterruptStatus[endpoint] & USB_EPINT_TimeOUT) == USB_EPINT_TimeOUT) {
		InEPRegs[endpoint].interrupt = USB_EPINT_TimeOUT;
		//uartPrintf("\t\tUSB_EPINT_TimeOUT\r\n");
		currentlySending = 0xFF;
		//ringBufferDequeue(txQueue);
	}

	if((inInterruptStatus[endpoint] & USB_EPINT_AHBErr) == USB_EPINT_AHBErr) {
		InEPRegs[endpoint].interrupt = USB_EPINT_AHBErr;
		//uartPrintf("\t\tUSB_EPINT_AHBErr\r\n");
	}

	if((inInterruptStatus[endpoint] & USB_EPINT_EPDisbld) == USB_EPINT_EPDisbld) {
		InEPRegs[endpoint].interrupt = USB_EPINT_EPDisbld;
		//uartPrintf("\t\tUSB_EPINT_EPDisbldr\n");
	}

	if((inInterruptStatus[endpoint] & USB_EPINT_XferCompl) == USB_EPINT_XferCompl) {
		InEPRegs[endpoint].interrupt = USB_EPINT_XferCompl;
		//uartPrintf("\t\tUSB_EPINT_XferCompl\n");
		currentlySending = 0xFF;
		advanceTxQueue();
	}
	
}

static void handleRxInterrupts(int endpoint) {
	if(!outInterruptStatus[endpoint]) {
		return;
	}

	//uartPrintf("\tendpoint %d has an RX interrupt\r\n", endpoint);
		
	if((outInterruptStatus[endpoint] & USB_EPINT_Back2BackSetup) == USB_EPINT_Back2BackSetup) {
		OutEPRegs[endpoint].interrupt = USB_EPINT_Back2BackSetup;
	}

	if((outInterruptStatus[endpoint] & USB_EPINT_OUTTknEPDis) == USB_EPINT_OUTTknEPDis) {
		OutEPRegs[endpoint].interrupt = USB_EPINT_OUTTknEPDis;
	}

	if((outInterruptStatus[endpoint] & USB_EPINT_AHBErr) == USB_EPINT_AHBErr) {
		OutEPRegs[endpoint].interrupt = USB_EPINT_AHBErr;
	}

	if((outInterruptStatus[endpoint] & USB_EPINT_EPDisbld) == USB_EPINT_EPDisbld) {
		OutEPRegs[endpoint].interrupt = USB_EPINT_EPDisbld;
	}

	if((outInterruptStatus[endpoint] & USB_EPINT_XferCompl) == USB_EPINT_XferCompl) {
		OutEPRegs[endpoint].interrupt = USB_EPINT_XferCompl;
		if(endpoint == 0) {
			receiveControl(controlRecvBuffer, sizeof(USBSetupPacket));
		}
	}
}

static void sendControl(void* buffer, int bufferLen) {
	usbTxRx(USB_CONTROLEP, USBIn, USBControl, buffer, bufferLen);
	ringBufferEnqueue(txQueue, USB_CONTROLEP);
	advanceTxQueue();
}

static void stallControl(void) {
	InEPRegs[USB_CONTROLEP].control = USB_EPCON_STALL;
}

void usb_send_bulk(uint8_t endpoint, void* buffer, int bufferLen) {
	usbTxRx(endpoint, USBIn, USBBulk, buffer, bufferLen);
	ringBufferEnqueue(txQueue, endpoint);
	advanceTxQueue();
}

void usb_send_interrupt(uint8_t endpoint, void* buffer, int bufferLen) {
	usbTxRx(endpoint, USBIn, USBInterrupt, buffer, bufferLen);
	ringBufferEnqueue(txQueue, endpoint);
	advanceTxQueue();
}


void usb_receive_bulk(uint8_t endpoint, void* buffer, int bufferLen) {
	usbTxRx(endpoint, USBOut, USBBulk, buffer, bufferLen);
}

void usb_receive_interrupt(uint8_t endpoint, void* buffer, int bufferLen) {
	usbTxRx(endpoint, USBOut, USBInterrupt, buffer, bufferLen);
}


static int advanceTxQueue() {
	EnterCriticalSection();
	if(currentlySending != 0xFF) {
		LeaveCriticalSection();
		return -1;
	}

	int8_t nextEP = ringBufferDequeue(txQueue);
	if(nextEP < 0) {
		LeaveCriticalSection();
		return -1;
	}

	currentlySending = nextEP;
	LeaveCriticalSection();

	/*int endpoint;
	for(endpoint = 0; endpoint < USB_NUM_ENDPOINTS; endpoint++) {
		if(endpoint_directions[endpoint] == USBIn || endpoint_directions[endpoint] == USBBiDir) {
			InEPRegs[endpoint].control = (InEPRegs[endpoint].control & ~(DCTL_NEXTEP_MASK << DCTL_NEXTEP_SHIFT)) | ((nextEP & DCTL_NEXTEP_MASK) << DCTL_NEXTEP_SHIFT);
		}
	}*/

	InEPRegs[0].control = (InEPRegs[0].control & ~(DCTL_NEXTEP_MASK << DCTL_NEXTEP_SHIFT)) | ((nextEP & DCTL_NEXTEP_MASK) << DCTL_NEXTEP_SHIFT);
	InEPRegs[1].control = (InEPRegs[1].control & ~(DCTL_NEXTEP_MASK << DCTL_NEXTEP_SHIFT)) | ((nextEP & DCTL_NEXTEP_MASK) << DCTL_NEXTEP_SHIFT);
	InEPRegs[3].control = (InEPRegs[3].control & ~(DCTL_NEXTEP_MASK << DCTL_NEXTEP_SHIFT)) | ((nextEP & DCTL_NEXTEP_MASK) << DCTL_NEXTEP_SHIFT);
	InEPRegs[5].control = (InEPRegs[5].control & ~(DCTL_NEXTEP_MASK << DCTL_NEXTEP_SHIFT)) | ((nextEP & DCTL_NEXTEP_MASK) << DCTL_NEXTEP_SHIFT);

	// clear all the interrupts
	InEPRegs[nextEP].interrupt = USB_EPINT_ALL;

	// we're ready to transmit!
	uint32_t controlStatus = InEPRegs[nextEP].control;
	InEPRegs[nextEP].control = controlStatus | USB_EPCON_ENABLE | USB_EPCON_CLEARNAK;

	return 0;
}

static void usbIRQHandler(uint32_t token) {

	// we need to mask because GINTSTS is set for a particular interrupt even if it's masked in GINTMSK (GINTMSK just prevents an interrupt being generated)
	uint32_t status = GET_REG(USB + GINTSTS) & GET_REG(USB + GINTMSK);
	int process = FALSE;

	//uartPrintf("<begin interrupt: %x>\r\n", status);

	if(status) {
		process = TRUE;
	}

	while(process) {
		if((status & GINTMSK_OTG) == GINTMSK_OTG) {
			// acknowledge OTG interrupt (these bits are all R_SS_WC which means Write Clear, a write of 1 clears the bits)
			SET_REG(USB + GOTGINT, GET_REG(USB + GOTGINT));

			// acknowledge interrupt (this bit is actually RO, but should've been cleared when we cleared GOTGINT. Still, iBoot pokes it as if it was WC, so we will too)
			SET_REG(USB + GINTSTS, GINTMSK_OTG);

			process = TRUE;
		} else {
			// we only care about OTG
			process = FALSE;
		}

		if((status & GINTMSK_RESET) == GINTMSK_RESET) {
			if(usb_state < USBError) {
				bufferPrintf("usb: reset detected\r\n");
				change_state(USBPowered);
			}

			int retval = resetUSB();

			SET_REG(USB + GINTSTS, GINTMSK_RESET);

			if(retval) {
				bufferPrintf("usb: listening for further usb events\r\n");
				return;	
			}

			process = TRUE;
		}

		if(((status & GINTMSK_INEP) == GINTMSK_INEP) || ((status & GINTMSK_OEP) == GINTMSK_OEP)) {
			// aha, got something on one of the endpoints. Now the real fun begins

			// first, let's get the interrupt status of individual endpoints
			getEndpointInterruptStatuses();

			if(isSetupPhaseDone()) {
				// recall our earlier receiveControl calls. We now should have 8 bytes of goodness in controlRecvBuffer.
				USBSetupPacket* setupPacket = (USBSetupPacket*) controlRecvBuffer;

				uint16_t length;
				uint32_t totalLength;
				USBStringDescriptor* strDesc;
				if(USBSetupPacketRequestTypeType(setupPacket->bmRequestType) != USBSetupPacketVendor) {
					switch(setupPacket->bRequest) {
						case USB_GET_DESCRIPTOR:
							length = setupPacket->wLength;
							// descriptor type is high, descriptor index is low
							int stall = FALSE;
							switch(setupPacket->wValue >> 8) {
								case USBDeviceDescriptorType:
									if(length > sizeof(USBDeviceDescriptor))
										length = sizeof(USBDeviceDescriptor);

									memcpy(controlSendBuffer, usb_get_device_descriptor(), length);
									break;
								case USBConfigurationDescriptorType:
									// hopefully SET_ADDRESS was received beforehand to set the speed
									totalLength = getConfigurationTree(setupPacket->wValue & 0xFF, usb_speed, controlSendBuffer);
									if(length > totalLength)
										length = totalLength;
									break;
								case USBStringDescriptorType:
									strDesc = usb_get_string_descriptor(setupPacket->wValue & 0xFF);
									if(length > strDesc->bLength)
										length = strDesc->bLength;
									memcpy(controlSendBuffer, strDesc, length);
									break;
								case USBDeviceQualifierDescriptorType:
									if(length > sizeof(USBDeviceQualifierDescriptor))
										length = sizeof(USBDeviceQualifierDescriptor);

									memcpy(controlSendBuffer, usb_get_device_qualifier_descriptor(), length);
									break;
								default:
									bufferPrintf("Unknown descriptor request: %d\r\n", setupPacket->wValue >> 8);
									stall = TRUE;
							}

							if(usb_state < USBError) {
								if(stall)
									stallControl();
								else
									sendControl(controlSendBuffer, length);
							}

							break;

						case USB_SET_ADDRESS:
							usb_speed = DSTS_GET_SPEED(GET_REG(USB + DSTS));
							usb_max_packet_size = packetsizeFromSpeed(usb_speed);
							SET_REG(USB + DCFG, (GET_REG(USB + DCFG) & ~DCFG_DEVICEADDRMSK)
								| ((setupPacket->wValue & DCFG_DEVICEADDR_UNSHIFTED_MASK) << DCFG_DEVICEADDR_SHIFT));

							// send an acknowledgement
							sendControl(controlSendBuffer, 0);

							if(usb_state < USBError) {
								change_state(USBAddress);
							}
							break;

						case USB_SET_INTERFACE:
							// send an acknowledgement
							sendControl(controlSendBuffer, 0);
							break;

						case USB_GET_STATUS:
							// FIXME: iBoot doesn't really care about this status
							*((uint16_t*) controlSendBuffer) = 0;
							sendControl(controlSendBuffer, sizeof(uint16_t));
							break;

						case USB_GET_CONFIGURATION:
							// FIXME: iBoot just puts out a debug message on console for this request.
							break;

						case USB_SET_CONFIGURATION:
							setConfiguration(0);
							// send an acknowledgment
							sendControl(controlSendBuffer, 0);

							if(usb_state < USBError) {
								change_state(USBConfigured);
								startHandler();
							}
							break;
						default:
							if(usb_state < USBError) {
								change_state(USBUnknownRequest);
							}
					}

					// get the next SETUP packet
					receiveControl(controlRecvBuffer, sizeof(USBSetupPacket));
				}
			} else {
				//uartPrintf("\t<begin callEndpointHandlers>\r\n");
				callEndpointHandlers();
				//uartPrintf("\t<end callEndpointHandlers>\r\n");
			}

			process = TRUE;
		}

		if((status & GINTMSK_SOF) == GINTMSK_SOF) {
			SET_REG(USB + GINTSTS, GINTMSK_SOF);
			process = TRUE;
		}

		if((status & GINTMSK_SUSPEND) == GINTMSK_SUSPEND) {
			SET_REG(USB + GINTSTS, GINTMSK_SUSPEND);
			process = TRUE;
		}

		status = GET_REG(USB + GINTSTS) & GET_REG(USB + GINTMSK);
	}

	//uartPrintf("<end interrupt>\r\n");

}

static void create_descriptors() {
	if(configurations == NULL) {
		deviceDescriptor.bLength = sizeof(USBDeviceDescriptor);
		deviceDescriptor.bDescriptorType = USBDeviceDescriptorType;
		deviceDescriptor.bcdUSB = USB_2_0;
		deviceDescriptor.bDeviceClass = 0;
		deviceDescriptor.bDeviceSubClass = 0;
		deviceDescriptor.bDeviceProtocol = 0;
		deviceDescriptor.bMaxPacketSize = USB_MAX_PACKETSIZE;
		deviceDescriptor.idVendor = 0x525;
		deviceDescriptor.idProduct = PRODUCT_IPHONE;
		deviceDescriptor.bcdDevice = DEVICE_IPHONE;
		deviceDescriptor.iManufacturer = addStringDescriptor("Apple Inc.");
		deviceDescriptor.iProduct = addStringDescriptor("Apple Mobile Device (OpenIBoot Mode)");
		deviceDescriptor.iSerialNumber = addStringDescriptor("");
		deviceDescriptor.bNumConfigurations = 0;

		deviceQualifierDescriptor.bDescriptorType = USBDeviceDescriptorType;
		deviceQualifierDescriptor.bcdUSB = USB_2_0;
		deviceQualifierDescriptor.bDeviceClass = 0;
		deviceQualifierDescriptor.bDeviceSubClass = 0;
		deviceQualifierDescriptor.bDeviceProtocol = 0;
		deviceDescriptor.bMaxPacketSize = USB_MAX_PACKETSIZE;
		deviceDescriptor.bNumConfigurations = 0;

		addConfiguration(1, addStringDescriptor("OpenIBoot Mode Configuration"), 0, 0, 500);
	}
}

USBDeviceDescriptor* usb_get_device_descriptor() {
	create_descriptors();
	return &deviceDescriptor;
}

USBDeviceQualifierDescriptor* usb_get_device_qualifier_descriptor() {
	create_descriptors();
	return &deviceQualifierDescriptor;
}

static void setConfiguration(int i) {
	int8_t j;
	for(j = 0; j < configurations[i].descriptor.bNumInterfaces; j++) {
		int8_t k;
		for(k = 0; k < configurations[i].interfaces[j].descriptor.bNumEndpoints; k++) {
			int endpoint = configurations[i].interfaces[j].endpointDescriptors[k].bEndpointAddress & 0x3;
			if((configurations[i].interfaces[j].endpointDescriptors[k].bEndpointAddress & (0x1 << 7)) == (0x1 << 7)) {
				InEPRegs[endpoint].control = InEPRegs[endpoint].control | DCTL_SETD0PID;
			} else {
				OutEPRegs[endpoint].control = OutEPRegs[endpoint].control | DCTL_SETD0PID;
			}
		}
	}
}

static uint32_t getConfigurationTree(int i, uint8_t speed_id, void* buffer) {
	uint8_t *buf = (uint8_t*) buffer;
	uint32_t pos = 0;

	if(configurations == NULL) {
		return 0;
	}

	memcpy(buf + pos, usb_get_configuration_descriptor(i, speed_id), sizeof(USBConfigurationDescriptor));
	pos += sizeof(USBConfigurationDescriptor);

	int8_t j;
	for(j = 0; j < configurations[i].descriptor.bNumInterfaces; j++) {
		memcpy(buf + pos, &configurations[i].interfaces[j].descriptor, sizeof(USBInterfaceDescriptor));
		pos += sizeof(USBInterfaceDescriptor);
		int8_t k;
		for(k = 0; k < configurations[i].interfaces[j].descriptor.bNumEndpoints; k++) {
			memcpy(buf + pos, &configurations[i].interfaces[j].endpointDescriptors[k], sizeof(USBEndpointDescriptor));
			pos += sizeof(USBEndpointDescriptor);
		}
	}

	return pos;
}

USBConfigurationDescriptor* usb_get_configuration_descriptor(int index, uint8_t speed_id) {
	if(index == 0 && configurations[0].interfaces == NULL) {
		USBInterface* interface = addInterfaceDescriptor(&configurations[0], 0, 0,
			OPENIBOOT_INTERFACE_CLASS, OPENIBOOT_INTERFACE_SUBCLASS, OPENIBOOT_INTERFACE_PROTOCOL, addStringDescriptor("IF0"));

		enumerateHandler(interface);
		endConfiguration(&configurations[0]);
	}

	return &configurations[index].descriptor;
}

void usb_add_endpoint(USBInterface* interface, int endpoint, USBDirection direction, USBTransferType transferType) {
	if(transferType == USBInterrupt) {
		if(usb_speed == USB_HIGHSPEED)
			addEndpointDescriptor(interface, endpoint, direction, transferType, USBNoSynchronization, USBDataEndpoint, packetsizeFromSpeed(usb_speed), 9);
		else
			addEndpointDescriptor(interface, endpoint, direction, transferType, USBNoSynchronization, USBDataEndpoint, packetsizeFromSpeed(usb_speed), 32);
	} else {
		addEndpointDescriptor(interface, endpoint, direction, transferType, USBNoSynchronization, USBDataEndpoint, packetsizeFromSpeed(usb_speed), 0);
	}
}

static void initializeDescriptors() {
	numStringDescriptors = 0;
	stringDescriptors = NULL;
	configurations = NULL;
	firstStringDescriptor = NULL;
	firstStringDescriptor = (USBFirstStringDescriptor*) malloc(sizeof(USBFirstStringDescriptor) + (sizeof(uint16_t) * 1));
	firstStringDescriptor->bLength = sizeof(USBFirstStringDescriptor) + (sizeof(uint16_t) * 1);
	firstStringDescriptor->bDescriptorType = USBStringDescriptorType;
	firstStringDescriptor->wLANGID[0] = USB_LANGID_ENGLISH_US;
}

static void releaseConfigurations() {
	if(configurations == NULL) {
		return;
	}

	int8_t i;
	for(i = 0; i < deviceDescriptor.bNumConfigurations; i++) {
		int8_t j;
		for(j = 0; j < configurations[i].descriptor.bNumInterfaces; j++) {
			free(configurations[i].interfaces[j].endpointDescriptors);
		}
		free(configurations[i].interfaces);
	}

	free(configurations);
	deviceDescriptor.bNumConfigurations = 0;
	configurations = NULL;
}

static uint8_t addConfiguration(uint8_t bConfigurationValue, uint8_t iConfiguration, uint8_t selfPowered, uint8_t remoteWakeup, uint16_t maxPower) {
	uint8_t newIndex = deviceDescriptor.bNumConfigurations;
	deviceDescriptor.bNumConfigurations++;
	deviceQualifierDescriptor.bNumConfigurations++;

	configurations = (USBConfiguration*) realloc(configurations, sizeof(USBConfiguration) * deviceDescriptor.bNumConfigurations);
	configurations[newIndex].descriptor.bLength = sizeof(USBConfigurationDescriptor);
	configurations[newIndex].descriptor.bDescriptorType = USBConfigurationDescriptorType;
	configurations[newIndex].descriptor.wTotalLength = 0;
	configurations[newIndex].descriptor.bNumInterfaces = 0;
	configurations[newIndex].descriptor.bConfigurationValue = bConfigurationValue;
	configurations[newIndex].descriptor.iConfiguration = iConfiguration;
	configurations[newIndex].descriptor.bmAttributes = ((0x1) << 7) | ((selfPowered & 0x1) << 6) | ((remoteWakeup & 0x1) << 5);
	configurations[newIndex].descriptor.bMaxPower = maxPower / 2;
	configurations[newIndex].interfaces = NULL;

	return newIndex;
}

static void endConfiguration(USBConfiguration* configuration) {
	configuration->descriptor.wTotalLength = sizeof(USBConfigurationDescriptor);

	int i;
	for(i = 0; i < configurations->descriptor.bNumInterfaces; i++) {
		configuration->descriptor.wTotalLength += sizeof(USBInterfaceDescriptor) + (configuration->interfaces[i].descriptor.bNumEndpoints * sizeof(USBEndpointDescriptor));
	}
}

static USBInterface* addInterfaceDescriptor(USBConfiguration* configuration, uint8_t bInterfaceNumber, uint8_t bAlternateSetting, uint8_t bInterfaceClass, uint8_t bInterfaceSubClass, uint8_t bInterfaceProtocol, uint8_t iInterface) {
	uint8_t newIndex = configuration->descriptor.bNumInterfaces;
	configuration->descriptor.bNumInterfaces++;

	configuration->interfaces = (USBInterface*) realloc(configuration->interfaces, sizeof(USBInterface) * configuration->descriptor.bNumInterfaces);
	configuration->interfaces[newIndex].descriptor.bLength = sizeof(USBInterfaceDescriptor);
	configuration->interfaces[newIndex].descriptor.bDescriptorType = USBInterfaceDescriptorType;
	configuration->interfaces[newIndex].descriptor.bInterfaceNumber = bInterfaceNumber;
	configuration->interfaces[newIndex].descriptor.bAlternateSetting = bAlternateSetting;
	configuration->interfaces[newIndex].descriptor.bInterfaceClass = bInterfaceClass;
	configuration->interfaces[newIndex].descriptor.bInterfaceSubClass = bInterfaceSubClass;
	configuration->interfaces[newIndex].descriptor.bInterfaceProtocol = bInterfaceProtocol;
	configuration->interfaces[newIndex].descriptor.iInterface = iInterface;
	configuration->interfaces[newIndex].descriptor.bNumEndpoints = 0;
	configuration->interfaces[newIndex].endpointDescriptors = NULL;

	return &configuration->interfaces[newIndex];
}

static uint8_t addEndpointDescriptor(USBInterface* interface, uint8_t endpoint, USBDirection direction, USBTransferType transferType, USBSynchronisationType syncType, USBUsageType usageType, uint16_t wMaxPacketSize, uint8_t bInterval) {
	if(direction > 2)
		return -1;

	uint8_t newIndex = interface->descriptor.bNumEndpoints;
	interface->descriptor.bNumEndpoints++;

	interface->endpointDescriptors = (USBEndpointDescriptor*) realloc(interface->endpointDescriptors, sizeof(USBEndpointDescriptor) * interface->descriptor.bNumEndpoints);
	interface->endpointDescriptors[newIndex].bLength = sizeof(USBEndpointDescriptor);
	interface->endpointDescriptors[newIndex].bDescriptorType = USBEndpointDescriptorType;
	interface->endpointDescriptors[newIndex].bEndpointAddress = (endpoint & 0xF) | ((direction & 0x1) << 7);	// see USB specs for the bitfield spec
	interface->endpointDescriptors[newIndex].bmAttributes = (transferType & 0x3) | ((syncType & 0x3) << 2) | ((usageType & 0x3) << 4);
	interface->endpointDescriptors[newIndex].wMaxPacketSize = wMaxPacketSize;
	interface->endpointDescriptors[newIndex].bInterval = bInterval;

	return newIndex;
}

static uint8_t addStringDescriptor(const char* descriptorString) {
	uint8_t newIndex = numStringDescriptors;
	numStringDescriptors++;

	stringDescriptors = (USBStringDescriptor**) realloc(stringDescriptors, sizeof(USBStringDescriptor*) * numStringDescriptors);

	int sLen = strlen(descriptorString);
	stringDescriptors[newIndex] = (USBStringDescriptor*) malloc(sizeof(USBStringDescriptor) + sLen * 2);
	stringDescriptors[newIndex]->bLength = sizeof(USBStringDescriptor) + sLen * 2;
	stringDescriptors[newIndex]->bDescriptorType = USBStringDescriptorType;
	uint16_t* string = (uint16_t*) stringDescriptors[newIndex]->bString;
	int i;
	for(i = 0; i < sLen; i++) {
		string[i] = descriptorString[i];
	}

	return (newIndex + 1);
}

USBStringDescriptor* usb_get_string_descriptor(int index) {
	if(index == 0) {
		return (USBStringDescriptor*) firstStringDescriptor;
	} else {
		return stringDescriptors[index - 1];
	}
}

static void releaseStringDescriptors() {
	int8_t i;

	if(stringDescriptors == NULL) {
		return;
	}

	for(i = 0; i < numStringDescriptors; i++) {
		free(stringDescriptors[i]);
	}

	free(stringDescriptors);

	numStringDescriptors = 0;
	stringDescriptors = NULL;
}

static uint16_t packetsizeFromSpeed(uint8_t speed_id) {
	switch(speed_id) {
		case USB_HIGHSPEED:
			return 512;
		case USB_FULLSPEED:
		case USB_FULLSPEED_48_MHZ:
			return 64;
		case USB_LOWSPEED:
			return 32;
		default:
			return -1;
	}
}

int usb_install_ep_handler(int endpoint, USBDirection direction, USBEndpointHandler handler, uint32_t token) {
	if(endpoint >= USB_NUM_ENDPOINTS) {
		return -1;
	}

	if(endpoint_directions[endpoint] != direction && endpoint_directions[endpoint] != USBBiDir) {
		return -1; // that endpoint can't handle this kind of direction
	}

	if(direction == USBIn) {
		endpoint_handlers[endpoint].in.handler = handler;
		endpoint_handlers[endpoint].in.token = token;
	} else if(direction == USBOut) {
		endpoint_handlers[endpoint].out.handler = handler;
		endpoint_handlers[endpoint].out.token = token;
	} else {
		return -1; // can only register IN or OUt directions
	}

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

	releaseConfigurations();
	releaseStringDescriptors();

	return 0;
}

static void change_state(USBState new_state) {
	bufferPrintf("USB state change: %d -> %d\r\n", usb_state, new_state);
	usb_state = new_state;
	if(usb_state == USBConfigured) {
		// TODO: set to host powered
	}
}


static RingBuffer* createRingBuffer(int size) {
	RingBuffer* buffer;
	buffer = (RingBuffer*) malloc(sizeof(RingBuffer));
	buffer->bufferStart = (int8_t*) memalign(DMA_ALIGN, size);
	buffer->bufferEnd = buffer->bufferStart + size;
	buffer->size = size;
	buffer->count = 0;
	buffer->readPtr = buffer->bufferStart;
	buffer->writePtr = buffer->bufferStart;

	return buffer;
}

static int8_t ringBufferDequeue(RingBuffer* buffer) {
	EnterCriticalSection();
	if(buffer->count == 0) {
		LeaveCriticalSection();
		return -1;
	}

	int8_t value = *buffer->readPtr;
	buffer->readPtr++;
	buffer->count--;

	if(buffer->readPtr == buffer->bufferEnd) {
		buffer->readPtr = buffer->bufferStart;
	}

	//uartPrintf("queue(dequeue): %d %x %x %x %x\r\n", buffer->count, buffer->readPtr, buffer->writePtr, buffer->bufferStart, buffer->bufferEnd);
	LeaveCriticalSection();
	return value;
}

static int8_t ringBufferEnqueue(RingBuffer* buffer, uint8_t value) {
	EnterCriticalSection();
	if(buffer->count == buffer->size) {
		LeaveCriticalSection();
		return -1;
	}

	*buffer->writePtr = value;
	buffer->writePtr++;
	buffer->count++;

	if(buffer->writePtr == buffer->bufferEnd) {
		buffer->writePtr = buffer->bufferStart;
	}

	//uartPrintf("queue(enqueue): %d %x %x %x %x\r\n", buffer->count, buffer->readPtr, buffer->writePtr, buffer->bufferStart, buffer->bufferEnd);
	LeaveCriticalSection();

	return value;
}

USBSpeed usb_get_speed() {
	switch(usb_speed) {
		case USB_HIGHSPEED:
			return USBHighSpeed;
		case USB_FULLSPEED:
		case USB_FULLSPEED_48_MHZ:
			return USBFullSpeed;
		case USB_LOWSPEED:
			return USBLowSpeed;
	}

	return USBLowSpeed;
}
