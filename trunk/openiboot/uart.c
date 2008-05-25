#include "openiboot.h"
#include "uart.h"
#include "clock.h"
#include "hardware/uart.h"

UARTRegisters HWUarts[5];
UARTSettings UARTs[5];

int uart_setup() {
	int i;

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
	uart_set_flow_control(4, OFF);

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

	return 0;
}


int uart_set_clk(int ureg, int clock) {
	if(ureg > 4)
		return -1; // Invalid ureg

	if(clock != UART_CLOCK_PCLK && clock != UART_CLOCK_EXT_UCLK0 && clock != UART_CLOCK_EXT_UCLK1) {
		return -1; // Invalid clock		
	}

	SET_REG(HWUarts[ureg].UCON,
		GET_REG(HWUarts[ureg].UCON) & (~UART_CLOCK_SELECTION_MASK) | (clock << UART_CLOCK_SELECTION_SHIFT));

	UARTs[ureg].clock = clock;
	uart_set_baud_rate(ureg, UARTs[ureg].baud);

	return 0;
}

int uart_set_baud_rate(int ureg, uint32_t baud) {
	if(ureg > 4)
		return -1; // Invalid ureg

	uint32_t clockFrequency = (UARTs[ureg].clock == 0) ? PeripheralFrequency : FixedFrequency;
	uint32_t div_val = clockFrequency / (baud * UARTs[ureg].sample_rate) - 1;

	SET_REG(HWUarts[ureg].UBAUD, GET_REG(HWUarts[ureg].UBAUD) & (~UART_DIVVAL_MASK) | div_val);

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
		GET_REG(HWUarts[ureg].UBAUD) & (~UART_SAMPLERATE_MASK) | (newSampleRate << UART_SAMPLERATE_SHIFT));

	UARTs[ureg].clock = rate;
	uart_set_baud_rate(ureg, UARTs[ureg].baud);

	return 0;
}

int uart_set_flow_control(int ureg, OnOff flow_control) {
	if(ureg > 4)
		return -1; // Invalid ureg

	if(ureg == 0)
		return -1; // uart0 does not support flow control

	if(flow_control == ON) {
		SET_REG(HWUarts[ureg].UMCON, UART_UMCON_AFC_BIT);
	} else {
		SET_REG(HWUarts[ureg].UMCON, UART_UMCON_NRTS_BIT);
	}

	UARTs[ureg].flow_control = flow_control;
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
			GET_REG(HWUarts[ureg].UCON) & (~UART_UCON_RXMODE_MASK) & (~UART_UCON_TXMODE_MASK)
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_RXMODE_SHIFT)
			| (UART_UCON_MODE_IRQORPOLL << UART_UCON_TXMODE_SHIFT));
	}

	return 0;
}

int uart_write(int ureg, char *buffer) {

	if(ureg > 4)
		return -1; // Invalid ureg

}
