#include "openiboot.h"
#include "radio.h"
#include "gpio.h"
#include "timer.h"
#include "util.h"
#include "hardware/radio.h"
#include "uart.h"

int radio_setup()
{
	gpio_pulldown_configure(RADIO_BB_PULLDOWN, GPIOPDDown);

	gpio_pin_output(RADIO_GPIO_BB_ON, OFF);
	udelay(100000);
	gpio_pin_output(RADIO_GPIO_RADIO_ON, ON);
	udelay(100000);
	gpio_pin_output(RADIO_GPIO_BB_RESET, ON);
	udelay(100000);
	gpio_pin_output(RADIO_GPIO_BB_RESET, OFF);
	udelay(100000);

	gpio_pin_use_as_input(RADIO_GPIO_BB_DETECT);
	if(gpio_pin_state(RADIO_GPIO_BB_DETECT) != 0)
	{
		bufferPrintf("radio: comm board not present, powered on, or at+xdrv=10,2 had been issued.\r\n");
		return -1;
	}

	bufferPrintf("radio: comm board detected.\r\n");
	return 0;
}

int radio_write(const char* str)
{
	int len = strlen(str);
	return uart_write(RADIO_UART, str, len);
}

int radio_read(char* buf, int len)
{
	char b;
	int i = 0;

	while(i < len)
	{
		uint64_t startTime = timer_get_system_microtime();
	       	while(uart_read(RADIO_UART, &b, 1, 0) == 0)
		{
			if(has_elapsed(startTime,  500000)) {
				return i;
			}
		}

		if(b == 0)
			continue;

		buf[i++] = b;
	}

	return i;
}
