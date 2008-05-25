#ifndef UART_H
#define UART_H

#include "openiboot.h"

typedef struct UARTSettings {
	uint32_t ureg;
	uint32_t baud;
	uint32_t sample_rate;
	OnOff flow_control;
	uint32_t mode;
	uint32_t clock;
} UARTSettings;

typedef struct UARTRegisters {
	uint32_t ULCON;
	uint32_t UCON;
	uint32_t UFCON;
	uint32_t UMCON;
	uint32_t UTRSTAT;
	uint32_t UERSTAT;
	uint32_t UFSTAT;
	uint32_t UMSTAT;
	uint32_t UTXH2;
	uint32_t URXH2;
	uint32_t UBAUD;
	uint32_t UINTP;
} UARTRegisters;

int uart_setup();
int uart_set_clk(int ureg, int clock);
int uart_set_sample_rate(int ureg, int rate);
int uart_set_flow_control(int ureg, OnOff flow_control);
int uart_set_mode(int ureg, uint32_t mode);
int uart_set_baud_rate(int ureg, uint32_t baud);

#endif
