#include "openiboot.h"
#include "uart.h"
#include "clock.h"
#include "hardware/uart.h"

UARTRegisters HWUarts[5];
UARTSettings UARTs[5];

int uart_setup() {
	// Set word size to 8bits
	SET_REG(UCON4, 0x01);
	SET_REG(UCON3, 0x01);
	SET_REG(UCON2, 0x01);
	SET_REG(UCON1, 0x01);
	SET_REG(UCON0, 0x01);
	// Set parity to even
	SET_REG(UCON4+4, 0x05);
	SET_REG(UCON3+4, 0x05);
	SET_REG(UCON2+4, 0x05);
	SET_REG(UCON1+4, 0x05);
	SET_REG(UCON0+4, 0x05);
	// Set ureg values
	UARTs[0].ureg = 0x00;
	UARTs[1].ureg = 0x01;
	UARTs[2].ureg = 0x02;
	UARTs[3].ureg = 0x03;
	UARTs[4].ureg = 0x04;
	// Set baud rate	
	UARTs[0].ureg = BAUD_115200;
	UARTs[1].ureg = BAUD_115200;
	UARTs[2].ureg = BAUD_115200;
	UARTs[3].ureg = BAUD_115200;
	UARTs[4].ureg = BAUD_115200;
	// Set clock
	uart_set_clk(0x00, 0x01);
	uart_set_clk(0x01, 0x01);
	uart_set_clk(0x02, 0x01);
	uart_set_clk(0x03, 0x01);
	uart_set_clk(0x04, 0x01);
	// Set sample rates
	uart_set_sample_rate(0x00, 0x10);
	uart_set_sample_rate(0x01, 0x10);
	uart_set_sample_rate(0x02, 0x10);
	uart_set_sample_rate(0x03, 0x10);
	uart_set_sample_rate(0x04, 0x10);
	// Set flow control 
	uart_set_flow_control(0x00, FLOW_CONTROL_OFF);
	uart_set_flow_control(0x01, FLOW_CONTROL_ON);
	uart_set_flow_control(0x02, FLOW_CONTROL_ON);
	uart_set_flow_control(0x03, FLOW_CONTROL_ON);
	uart_set_flow_control(0x04, FLOW_CONTROL_OFF);

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

	if(mode == 0) {
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
