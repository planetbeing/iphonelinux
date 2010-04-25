#include "openiboot.h"
#include "als.h"
#include "hardware/als.h"
#include "i2c.h"
#include "timer.h"
#include "multitouch.h"
#include "util.h"
#include "gpio.h"

#define CONTROL 0x0
#define TIMING 0x1
#define THRESHLOWLOW 0x2
#define THRESHLOWHIGH 0x3
#define THRESHHIGHLOW 0x4
#define THRESHHIGHHIGH 0x5
#define INTERRUPT 0x6
#define CRC 0x8
#define ID 0xA
#define DATA0LOW 0xC
#define DATA0HIGH 0xD
#define DATA1LOW 0xE
#define DATA1HIGH 0xF

static void als_writeb(uint8_t addr, uint8_t b);
static void als_writew(uint8_t addr, uint16_t w);
static uint8_t als_readb(uint8_t addr);
static uint16_t als_readw(uint8_t addr);
static void als_clearint();
static void als_int(uint32_t token);
static uint16_t als_data0();
static uint16_t als_data1();

static int use_channel;

int als_setup()
{
	multitouch_on();

	als_writeb(CONTROL, 0x0);
	udelay(1000);
	als_writeb(CONTROL, 0x3);

	if(als_readb(CONTROL) != 0x3)
	{
		bufferPrintf("als: error initializing\r\n");
		return -1;
	}

	als_setchannel(0);

	gpio_register_interrupt(ALS_INT, 1, 0, 0, als_int, 0);

	bufferPrintf("als: initialized\r\n");

	return 0;
}

void als_setchannel(int channel)
{
	use_channel = channel;
}

void als_enable_interrupt()
{
	uint16_t data0 = als_data0();

	if(data0 > 0)
		als_setlowthreshold(data0 - 1);
	else
		als_setlowthreshold(data0);

	if(data0 < 0xFFFF)
		als_sethighthreshold(data0 + 1);
	else
		als_sethighthreshold(0xFFFF);

	// level interrupt, any value out of range.
	als_writeb(INTERRUPT, (1 << 4) | 1);
	als_clearint();

	gpio_interrupt_enable(ALS_INT);
}

void als_disable_interrupt()
{
	gpio_interrupt_disable(ALS_INT);
	als_writeb(INTERRUPT, 0);
	als_clearint();
}

static void als_int(uint32_t token)
{
	// this is needed because there's no way to avoid repeated interrupts at the boundaries (0 and 0xFFFF)
	static uint16_t lastData0 = 0xFFFF;

	uint16_t data1 = als_data1();
	uint16_t data0 = als_data0();
	if(data0 > 0)
		als_setlowthreshold(data0 - 1);
	else
		als_setlowthreshold(data0);

	if(data0 < 0xFFFF)
		als_sethighthreshold(data0 + 1);
	else
		als_sethighthreshold(0xFFFF);

	if(lastData0 != data0)
		bufferPrintf("als: data = %d\r\n", (use_channel == 0 ? data0 : data1));

	lastData0 = data0;

	als_clearint();
}

void als_setlowthreshold(uint16_t value)
{
	als_writew(THRESHLOWLOW, value);
}

void als_sethighthreshold(uint16_t value)
{
	als_writew(THRESHHIGHLOW, value);
}

static uint16_t als_data0()
{
	return als_readw(DATA0LOW);
}

static uint16_t als_data1()
{
	return als_readw(DATA1LOW);
}

uint16_t als_data()
{
	return use_channel == 0 ? als_data0() : als_data1();
}

static void als_writeb(uint8_t addr, uint8_t b)
{
	uint8_t buf[2];
	buf[0] = 0x80 | addr;
	buf[1] = b;
	i2c_tx(ALS_I2C, ALS_ADDR, buf, sizeof(buf));
}

static uint8_t als_readb(uint8_t addr)
{
	uint8_t registers[1];
	uint8_t ret[1];

	registers[0] = 0x80 | addr;
	ret[0] = 0;

	i2c_rx(ALS_I2C, ALS_ADDR, registers, 1, ret, sizeof(ret)); 

	return ret[0];
}

static uint16_t als_readw(uint8_t addr)
{
	uint8_t registers;
	uint16_t ret;

	registers = 0x80 | 0x10 | addr;
	ret = 0;

	i2c_rx(ALS_I2C, ALS_ADDR, &registers, 1, &ret, sizeof(ret));

	return ret;
}

static void als_writew(uint8_t addr, uint16_t w)
{
	uint8_t buf[4];
	buf[0] = 0x80 | 0x10 | addr;
	buf[1] = 2;
	buf[2] = w & 0xFF;
	buf[3] = (w >> 8) & 0xFF;
	i2c_tx(ALS_I2C, ALS_ADDR, buf, sizeof(buf));
}

static void als_clearint()
{
	uint8_t buf = 0x80 | 0x40;
	i2c_tx(ALS_I2C, ALS_ADDR, &buf, sizeof(buf)); 
}
