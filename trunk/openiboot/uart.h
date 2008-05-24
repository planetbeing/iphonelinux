#ifndef UART_H
#define UART_H

#include "openiboot.h"

int uart_setup();
int uart_set_clk(int ureg, int mode);
int uart_set_sample_rate(int ureg, int rate);
int uart_set_flow_control(int ureg, int mode);
int uart_set_mode(int ureg, int mode);

#endif
