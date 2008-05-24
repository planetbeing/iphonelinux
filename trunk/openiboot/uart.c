#include "openiboot.h"
#include "uart.h"
#include "s5l8900.h"

int uart_setup() {
	
	uart_settings u_set[5];

	// Set word size to 8bits
	SET_REG(UCON4, 0x01)
	SET_REG(UCON3, 0x01)
	SET_REG(UCON2, 0x01)
	SET_REG(UCON1, 0x01)
	SET_REG(UCON0, 0x01)
	// Set parity to even
  SET_REG(UCON4+4, 0x05)
  SET_REG(UCON3+4, 0x05)
  SET_REG(UCON2+4, 0x05)
  SET_REG(UCON1+4, 0x05)
  SET_REG(UCON0+4, 0x05)
	// Set ureg values
	u_set[0].ureg = 0x00;
	u_set[1].ureg = 0x01;
	u_set[2].ureg = 0x02;
	u_set[3].ureg = 0x03;
	u_set[4].ureg = 0x04;
	// Set baud rate	
	u_set[0].ureg = BAUD_115200;
	u_set[1].ureg = BAUD_115200;
	u_set[2].ureg = BAUD_115200;
	u_set[3].ureg = BAUD_115200;
	u_set[4].ureg = BAUD_115200;
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


int uart_set_clk(int ureg, int mode) {

	return 0;

}
int uart_set_sample_rate(int ureg, int rate) {

	return 0;
}
int uart_set_flow_control(int ureg, int mode) {
	
	return 0;
}
int uart_set_mode(int ureg, int mode) {

	return 0;
}
int uart_write(int ureg, char *buffer) {

	if(ureg > 4)
		return -1; // Invalid ureg

}
