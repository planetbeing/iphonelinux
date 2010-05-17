#include "openiboot.h"
#include "uart.h"
#include "clock.h"
#include "hardware/uart.h"
#include "timer.h"

const UARTRegisters HWUarts[] = {
	{UART + UART0 + UART_ULCON, UART + UART0 + UART_UCON, UART + UART0 + UART_UFCON, 0,
		UART + UART0 + UART_UTRSTAT, UART + UART0 + UART_UERSTAT, UART + UART0 + UART_UFSTAT,
		0, UART + UART0 + UART_UTXH, UART + UART0 + UART_URXH, UART + UART0 + UART_UBAUD,
		UART + UART0 + UART_UDIVSLOT},
	{UART + UART1 + UART_ULCON, UART + UART1 + UART_UCON, UART + UART1 + UART_UFCON, UART + UART1 + UART_UMCON,
		UART + UART1 + UART_UTRSTAT, UART + UART1 + UART_UERSTAT, UART + UART1 + UART_UFSTAT,
		UART + UART1 + UART_UMSTAT, UART + UART1 + UART_UTXH, UART + UART1 + UART_URXH, UART + UART1 + UART_UBAUD,
		UART + UART1 + UART_UDIVSLOT},
	{UART + UART2 + UART_ULCON, UART + UART2 + UART_UCON, UART + UART2 + UART_UFCON, UART + UART2 + UART_UMCON,
		UART + UART2 + UART_UTRSTAT, UART + UART2 + UART_UERSTAT, UART + UART2 + UART_UFSTAT,
		UART + UART2 + UART_UMSTAT, UART + UART2 + UART_UTXH, UART + UART2 + UART_URXH, UART + UART2 + UART_UBAUD,
		UART + UART2 + UART_UDIVSLOT},
	{UART + UART3 + UART_ULCON, UART + UART3 + UART_UCON, UART + UART3 + UART_UFCON, UART + UART3 + UART_UMCON,
		UART + UART3 + UART_UTRSTAT, UART + UART3 + UART_UERSTAT, UART + UART3 + UART_UFSTAT,
		UART + UART3 + UART_UMSTAT, UART + UART3 + UART_UTXH, UART + UART3 + UART_URXH, UART + UART3 + UART_UBAUD,
		UART + UART3 + UART_UDIVSLOT},
	{UART + UART4 + UART_ULCON, UART + UART4 + UART_UCON, UART + UART4 + UART_UFCON, UART + UART4 + UART_UMCON,
		UART + UART4 + UART_UTRSTAT, UART + UART4 + UART_UERSTAT, UART + UART4 + UART_UFSTAT,
		UART + UART4 + UART_UMSTAT, UART + UART4 + UART_UTXH, UART + UART4 + UART_URXH, UART + UART4 + UART_UBAUD,
		UART + UART4 + UART_UDIVSLOT}};

UARTSettings UARTs[5];

int UartHasInit;

int uart_setup() {
	int i;

	if(UartHasInit) {
		return 0;
	}

	clock_gate_switch(UART_CLOCKGATE, ON);

	for(i = 0; i < NUM_UARTS; i++) {
		// set all uarts to transmit 8 bit frames, one stop bit per frame, no parity, no infrared mode
		SET_REG(HWUarts[i].ULCON, UART_8BITS);

		// set all uarts to use polling for rx/tx, no breaks, no loopback, no error status interrupts,
		// no timeouts, pulse interrupts for rx/tx, peripheral clock. Basically, the defaults.
		SET_REG(HWUarts[i].UCON, (UART_UCON_MODE_IRQORPOLL << UART_UCON_RXMODE_SHIFT)
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_TXMODE_SHIFT));

		// Initialize the settings array a bit so the helper functions can be used properly
		UARTs[i].ureg = i;
		UARTs[i].baud = 115200;

		uart_set_clk(i, UART_CLOCK_EXT_UCLK0);
		uart_set_sample_rate(i, 16);
	}

	// Set flow control
	uart_set_flow_control(0, OFF);
	uart_set_flow_control(1, ON);
	uart_set_flow_control(2, ON);
	uart_set_flow_control(3, ON);

#ifdef CONFIG_3G
	uart_set_flow_control(4, ON);
#else
	uart_set_flow_control(4, OFF);
#endif

	// Reset and enable fifo
	for(i = 0; i < NUM_UARTS; i++) {
		SET_REG(HWUarts[i].UFCON, UART_FIFO_RESET_TX | UART_FIFO_RESET_RX);
		SET_REG(HWUarts[i].UFCON, UART_FIFO_ENABLE);
		UARTs[i].fifo = ON;
	}

	for(i = 0; i < NUM_UARTS; i++) {
		uart_set_mode(i, UART_POLL_MODE);
	}

	uart_set_mode(0, UART_POLL_MODE);

	UartHasInit = TRUE;

	return 0;
}


int uart_set_clk(int ureg, int clock) {
	if(ureg > 4)
		return -1; // Invalid ureg

	if(clock != UART_CLOCK_PCLK && clock != UART_CLOCK_EXT_UCLK0 && clock != UART_CLOCK_EXT_UCLK1) {
		return -1; // Invalid clock		
	}

	SET_REG(HWUarts[ureg].UCON,
		(GET_REG(HWUarts[ureg].UCON) & (~UART_CLOCK_SELECTION_MASK)) | (clock << UART_CLOCK_SELECTION_SHIFT));

	UARTs[ureg].clock = clock;
	uart_set_baud_rate(ureg, UARTs[ureg].baud);

	return 0;
}

int uart_set_baud_rate(int ureg, uint32_t baud) {
	if(ureg > 4)
		return -1; // Invalid ureg

	uint32_t clockFrequency = (UARTs[ureg].clock == UART_CLOCK_PCLK) ? PeripheralFrequency : FixedFrequency;
	uint32_t div_val = clockFrequency / (baud * UARTs[ureg].sample_rate) - 1;

	SET_REG(HWUarts[ureg].UBAUD, (GET_REG(HWUarts[ureg].UBAUD) & (~UART_DIVVAL_MASK)) | div_val);

	// vanilla iBoot also does a reverse calculation from div_val and solves for baud and reports
	// the "actual" baud rate, or what is after loss during integer division

	UARTs[ureg].baud = baud;

	return 0;
}

int uart_set_sample_rate(int ureg, int rate) {
	if(ureg > 4)
		return -1; // Invalid ureg

	uint32_t newSampleRate;
	switch(rate) {
		case 4:
			newSampleRate = UART_SAMPLERATE_4;
			break;
		case 8:
			newSampleRate = UART_SAMPLERATE_8;
			break;
		case 16:
			newSampleRate = UART_SAMPLERATE_16;
			break;
		default:
			return -1; // Invalid sample rate
	}

	SET_REG(HWUarts[ureg].UBAUD,
		(GET_REG(HWUarts[ureg].UBAUD) & (~UART_SAMPLERATE_MASK)) | (newSampleRate << UART_SAMPLERATE_SHIFT));

	UARTs[ureg].sample_rate = rate;
	uart_set_baud_rate(ureg, UARTs[ureg].baud);

	return 0;
}

int uart_set_flow_control(int ureg, OnOff flow_control) {
	if(ureg > 4)
		return -1; // Invalid ureg

	if(flow_control == ON) {
		if(ureg == 0)
			return -1; // uart0 does not support flow control

		SET_REG(HWUarts[ureg].UMCON, UART_UMCON_AFC_BIT);
	} else {
		if(ureg != 0) {
			SET_REG(HWUarts[ureg].UMCON, UART_UMCON_NRTS_BIT);
		}
	}

	UARTs[ureg].flow_control = flow_control;

	return 0;
}

int uart_set_mode(int ureg, uint32_t mode) {
	if(ureg > 4)
		return -1; // Invalid ureg

	UARTs[ureg].mode = mode;

	if(mode == UART_POLL_MODE) {
		// Setup some defaults, like no loopback mode
		SET_REG(HWUarts[ureg].UCON, 
			GET_REG(HWUarts[ureg].UCON) & (~UART_UCON_UNKMASK) & (~UART_UCON_UNKMASK) & (~UART_UCON_LOOPBACKMODE));

		// Use polling mode
		SET_REG(HWUarts[ureg].UCON, 
			(GET_REG(HWUarts[ureg].UCON) & (~UART_UCON_RXMODE_MASK) & (~UART_UCON_TXMODE_MASK))
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_RXMODE_SHIFT)
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_TXMODE_SHIFT));
	}

	return 0;
}

int uart_write(int ureg, const char *buffer, uint32_t length) {
	if(!UartHasInit) {
		uart_setup();
	}

	if(ureg > 4)
		return -1; // Invalid ureg

	const UARTRegisters* uart = &HWUarts[ureg];
	UARTSettings* settings = &UARTs[ureg];

	if(settings->mode != UART_POLL_MODE)
		return -1; // unhandled uart mode

	int written = 0;
	while(written < length) {
		if(settings->fifo) {
			// spin until the tx fifo buffer is no longer full
			while((GET_REG(uart->UFSTAT) & UART_UFSTAT_TXFIFO_FULL) != 0);
		} else {
			// spin while not Transmitter Empty
			while((GET_REG(uart->UTRSTAT) & UART_UTRSTAT_TRANSMITTEREMPTY) == 0);
		}

		if(settings->flow_control) {		// only need to do this when there is flow control
			// spin while not Transmitter Empty
			while((GET_REG(uart->UTRSTAT) & UART_UTRSTAT_TRANSMITTEREMPTY) == 0);

			// spin while not Clear To Send
			while((GET_REG(uart->UMSTAT) & UART_UMSTAT_CTS) == 0);
		}

		SET_REG(uart->UTXH, *buffer); 

		buffer++;
		written++;
	}

	return written;
}

int uart_read(int ureg, char *buffer, uint32_t length, uint64_t timeout) {
	if(!UartHasInit)
		return -1;

	if(ureg > 4)
		return -1; // Invalid ureg

	const UARTRegisters* uart = &HWUarts[ureg];
	UARTSettings* settings = &UARTs[ureg];

	if(settings->mode != UART_POLL_MODE)
		return -1; // unhandled uart mode

	uint64_t startTime = timer_get_system_microtime();
	int written = 0;
	uint32_t discard;

	while(written < length) {
		register int canRead = 0;
		if(settings->fifo) {
			uint32_t ufstat = GET_REG(uart->UFSTAT);
			canRead = (ufstat & UART_UFSTAT_RXFIFO_FULL) | (ufstat & UART_UFSTAT_RXCOUNT_MASK);
		} else {
			canRead = GET_REG(uart->UTRSTAT) & UART_UTRSTAT_RECEIVEDATAREADY;
		}

		if(canRead) {
			if(GET_REG(uart->UERSTAT)) {
				discard = GET_REG(uart->URXH);
			} else {
				*buffer = GET_REG(uart->URXH);
				written++;
				buffer++;
			}
		} else {
			if((timer_get_system_microtime() - startTime) >= timeout) {
				break;
			}
		}
	}

	return written;
}

