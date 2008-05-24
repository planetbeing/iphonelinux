#include "openiboot.h"
#include "uart.h"
#include "s5l8900.h"

int uart_setup() {
	
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
