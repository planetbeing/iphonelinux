#include "openiboot.h"
#include "radio.h"
#include "gpio.h"
#include "timer.h"
#include "util.h"
#include "hardware/radio.h"
#include "uart.h"

// For the +XDRV stuff, it's usually device,function,arg1,arg2,arg3,...
// device 4 seems to be the vibrator, device 0 seems to be the speakers,
// 7 seems to have to do with bluetooth, and 9 is bb nvram

static int radio_nvram_read_all(char** res);
static char* radio_nvram;
static int radio_nvram_len;

int radio_setup()
{
	gpio_pin_output(RADIO_GPIO_BB_MUX_SEL, OFF);

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

	if(!radio_wait_for_ok(10))
	{
		bufferPrintf("radio: no response from baseband!\r\n");
		return -1;
	}

	bufferPrintf("radio: setting speed to 750000 baud.\r\n");

	radio_write("at+ipr=750000\r\n");

	// wait a millisecond for the command to totally clear uart
	// I wasn't able to detect this condition with uart registers (looking at FIFO count, transmitter empty)
	udelay(1000);

	uart_set_baud_rate(RADIO_UART, 750000);

	if(!radio_wait_for_ok(10))
	{
		bufferPrintf("radio: no response from baseband!\r\n");
		return -1;
	}

	bufferPrintf("radio: ready.\r\n");

	bufferPrintf("radio: reading baseband nvram... ");

	radio_nvram_len = radio_nvram_read_all(&radio_nvram);

	bufferPrintf("done\r\n");

	char* cursor = radio_nvram;
	while(cursor < (radio_nvram + radio_nvram_len))
	{
		int type = (cursor[0] << 8) | cursor[1];
		int size = ((cursor[2] << 8) | cursor[3]) * 2;
		if(size == 0)
			break;

		uint8_t* data = (uint8_t*)(cursor + 4);

		switch(type)
		{
			case 1:
				bufferPrintf("Wi-Fi TX Cal Data : <%d bytes, CRC = %08X>\r\n", size - 4, crc32(0, data, size - 4));
				break;

			case 4:
				bufferPrintf("Build name        : %s\r\n", (char*)data);
				break;

			case 2:
			case 3:
			case 5:
				if(type == 2)
					bufferPrintf("Wi-Fi MAC         : ");

				if(type == 3)
					bufferPrintf("Bluetooth MAC     : ");

				if(type == 5)
					bufferPrintf("Ethernet MAC      : ");

				bufferPrintf("%02X:%02X:%02X:%02X:%02X:%02X\r\n", data[0], data[1], data[2], data[3], data[4], data[5]);
				break;

			case 7:
				bufferPrintf("Unknown data      : %08X\r\n", *((uint32_t*)(data)));
				break;

			default:
				bufferPrintf("Unknown entry %d  : <%d bytes>\r\n", type, size - 4);
		}

		cursor += size;
	}

	speaker_setup();

	return 0;
}

void vibrator_loop(int frequency, int period, int timeOn)
{
	char buf[100];
	sprintf(buf, "at+xdrv=4,0,2,%d,%d,%d\r\n", frequency, period, timeOn);

	// write the command
	radio_write(buf);

	// clear the response
	radio_read(buf, sizeof(buf));
}

void vibrator_once(int frequency, int time)
{
	char buf[100];
	sprintf(buf, "at+xdrv=4,0,1,%d,%d,%d\r\n", frequency, time + 1, time);

	// write the command
	radio_write(buf);

	// clear the response
	radio_read(buf, sizeof(buf));
}

void vibrator_off()
{
	char buf[100];

	// write the command
	radio_write("at+xdrv=4,0,0,0,0,0\r\n");

	// clear the response
	radio_read(buf, sizeof(buf));
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
	char* curLine = buf;

	while(i < (len - 1))
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

		buf[i] = b;
		buf[i + 1] = '\0';

		if(strstr(curLine, "OK\r") != NULL)
		{
			++i;
			break;
		} else if(strstr(curLine, "ERROR\r") != NULL)
		{
			++i;
			break;
		} else if(b == '\r')
			curLine = &buf[i + 1];
		else if(b == '\n')
			curLine = &buf[i + 1];

		++i;
	}

	return i;
}

int radio_wait_for_ok(int tries)
{
	return radio_cmd("at\r\n", tries);
}

int radio_cmd(const char* cmd, int tries)
{
	int i;
	for(i = 0; i < tries; ++i)
	{
		char buf[200];
		int n;

		radio_write(cmd);

		n = radio_read(buf, sizeof(buf) - 1);

		if(n == 0)
			continue;

		if(strstr(buf, "\nOK\r") != NULL)
			break;
		else if(strstr(buf, "\rOK\r") != NULL)
			break;
	}

	if(i == tries)
		return FALSE;
	else
		return TRUE;
}

static int radio_nvram_read_idx(int idx, char** res)
{
	char cmd[20];
	char* curBuf;
	char* resultStart;
	int curBufSize;
	int curPos;
	int c;
	int searchLen;

	sprintf(cmd, "at+xdrv=9,1,%d\r\n", idx);

	radio_write(cmd);

	curPos = 0;
	curBufSize = 100;

	curBuf = malloc(curBufSize);

	curPos = radio_read(curBuf, curBufSize);
	while(curPos == (curBufSize - 1))
	{
		curBufSize += 100;
		curBuf = realloc(curBuf, curBufSize);
		c = radio_read(curBuf + curPos, curBufSize - curPos);
		curPos += c;
	}

	sprintf(cmd, "+XDRV: 9,1,0,%d,", idx);
	searchLen = strlen(cmd);

	resultStart = curBuf;

	while((resultStart - curBuf) <= (curPos - searchLen) && memcmp(resultStart, cmd, searchLen) != 0)
		++resultStart;

	if(memcmp(resultStart, cmd, searchLen) != 0)
	{
		free(curBuf);
		return 0;
	}

	resultStart += searchLen;

	if(memcmp(resultStart, "NULL", sizeof("NULL")) == 0)
	{
		free(curBuf);
		return 0;
	}

	c = 0;
	while(*resultStart != '\r' && *resultStart != '\n' && *resultStart != '\0')
	{
		cmd[0] = resultStart[0];
		cmd[1] = resultStart[1];
		cmd[2] = '\0';
		curBuf[c++] = strtoul(cmd, NULL, 16);
		resultStart += 2;
	}

	*res = curBuf;

	return c;
}

static int radio_nvram_read_all(char** res)
{
	int ret;
	int idx;
	int len;

	*res = NULL;
	len = 0;
	idx = 0;
	while(TRUE)
	{
		char* line;
		ret = radio_nvram_read_idx(idx, &line);
		if(ret == 0)
			return len;

		*res = realloc(*res, len + ret);
		memcpy(*res + len, line, ret);
		free(line);
		len += ret;
		++idx;
	}
}

int speaker_setup()
{
	bufferPrintf("radio: enabling internal speaker\r\n");

	// something set at the very beginning
	radio_cmd("at+xdrv=0,41,25\r\n", 10);

	// mute everything?
	radio_cmd("at+xdrv=0,1,0,0\r\n", 10);
	radio_cmd("at+xdrv=0,1,0,1\r\n", 10);
	radio_cmd("at+xdrv=0,1,0,2\r\n", 10);
	radio_cmd("at+xdrv=0,1,0,6\r\n", 10);

	// I really don't know
	radio_cmd("at+xdrv=0,24,1,1\r\n", 10);
	radio_cmd("at+xdrv=0,0,2,2\r\n", 10);

	loudspeaker_vol(100);
	speaker_vol(68);

	// clock
	// In general, lower is slower and higher is faster, but at some point it loops around.
	// This may mean the value is a bitset, e.g., at+xdrv=0,2,2,29 will set it to half speed
	radio_cmd("at+xdrv=0,2,2,10\r\n", 10);

	// channels?
	radio_cmd("at+xdrv=0,9,2\r\n", 10);

	// enable i2s?
	radio_cmd("at+xdrv=0,20,1\r\n", 10);

	// unmute?
	radio_cmd("at+xdrv=0,3,0\r\n", 10);

	bufferPrintf("radio: internal speaker enabled\r\n");
	return 0;
}

void loudspeaker_vol(int vol)
{
	char buf[100];
	sprintf(buf, "at+xdrv=0,1,%d,2\r\n", vol);
	radio_cmd(buf, 10);
}

void speaker_vol(int vol)
{
	char buf[100];
	sprintf(buf, "at+xdrv=0,1,%d,0\r\n", vol);
	radio_cmd(buf, 10);
}
