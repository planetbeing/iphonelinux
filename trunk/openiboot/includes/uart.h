#ifndef UART_H
#define UART_H

#include "openiboot.h"

#define BAUD_115200 0x1C200
#define FLOW_CONTROL_ON	 0x01
#define FLOW_CONTROL_OFF 0x00

typedef struct uart_settings {
	uint32_t ureg;
	uint32_t baud;
	uint32_t sample_rate;
	uint32_t flow_control;
	uint32_t mode;
	uint32_t divider;
} uart_settings;

int uart_setup();
int uart_set_clk(int ureg, int mode);
int uart_set_sample_rate(int ureg, int rate);
int uart_set_flow_control(int ureg, int mode);
int uart_set_mode(int ureg, int mode);

#endif
