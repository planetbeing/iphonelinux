#include "openiboot.h"
#include "uart.h"
#include "s5l8900.h"

int uart_setup() {

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
