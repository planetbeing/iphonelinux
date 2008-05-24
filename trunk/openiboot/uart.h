#ifndef UART_H
#define UART_H

#include "openiboot.h"

/* UART Control Registers */
#define UCON0  (*(volatile unsigned short *)0x3CC00004)
#define UCON1  (*(volatile unsigned short *)0x3CC04004)
#define UCON2	 (*(volatile unsigned short *)0x3CC08004)
#define UCON3	 (*(volatile unsigned short *)0x3CC0C004)
#define UCON4  (*(volatile unsigned short *)0x3CC10004)

int uart_setup();
int uart_set_clk(int ureg, int mode);
int uart_set_sample_rate(int ureg, int rate);
int uart_set_flow_control(int ureg, int mode);
int uart_set_mode(int ureg, int mode);

#endif
