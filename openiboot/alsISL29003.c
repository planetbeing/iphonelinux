#include "openiboot.h"
#include "als.h"
#include "hardware/als.h"
#include "i2c.h"
#include "timer.h"
#include "multitouch.h"
#include "util.h"
#include "gpio.h"

#define COMMAND 0x0
#define CONTROL 0x1
#define INTTHRESHHIGH 0x2
#define INTTHRESHLOW 0x3
#define SENSORLOW 0x4
#define SENSORHIGH 0x5
#define TIMERLOW 0x6
#define TIMERHIGH 0x7

static void als_writeb(uint8_t addr, uint8_t b);
static uint8_t als_readb(uint8_t addr);
static uint16_t als_readw(uint8_t addr);
static void als_clearint();
static void als_int(uint32_t token);

static int use_channel;

int als_setup()
{
	multitouch_on();

	// disable ADC-core
	als_writeb(COMMAND, 0<<7);
	udelay(1000);
	// powerdown chip
	als_writeb(COMMAND, 1<<6);
	udelay(1000);
	// power up chip
	als_writeb(COMMAND, 0<<6);
	udelay(1000);
	als_setchannel(0);

	if(als_readb(COMMAND) != (1 << 7 | 0 << 5 | (use_channel == 0 ? 0 : 1) << 2))
	{
		bufferPrint("als: error initializing\r\n");
		return -1;
	}

	gpio_register_interrupt(ALS_INT, 1, 0, 0, als_int, 0);

	bufferPrintf("als: initialized\r\n");

	return 0;
}

void als_setchannel(int channel)
{
	use_channel = channel;
	als_writeb(COMMAND, 1 << 7 | 0 << 5 | (channel == 0 ? 0 : 1) << 2);
	udelay(1000);
}

void als_enable_interrupt()
{
	uint16_t sensordata = als_data();

	als_setlowthreshold(sensordata >> 8);
	als_sethighthreshold(sensordata >> 8);

	// Gain : 0-62272 lux, trigger if out of range for every consecutive integration cycles
	als_writeb(CONTROL, (3 << 2) | 0);
	als_clearint();

	gpio_interrupt_enable(ALS_INT);
}

void als_disable_interrupt()
{
	gpio_interrupt_disable(ALS_INT);
	als_clearint();
}

static void als_int(uint32_t token)
{
	// this is needed because there's no way to avoid repeated interrupts at the boundaries (0 and 0xFFFF)
	static uint16_t lastData0 = 0xFFFF;

	uint16_t sensordata = als_data();

	als_setlowthreshold(sensordata >> 8);
	als_sethighthreshold(sensordata >> 8);

	if(lastData0 != sensordata)
		bufferPrintf("als: data = %d\r\n", sensordata);

	lastData0 = sensordata;

	als_clearint();
}

void als_setlowthreshold(uint16_t value)
{
	als_writeb(INTTHRESHHIGH, value);
}

void als_sethighthreshold(uint16_t value)
{
	als_writeb(INTTHRESHLOW, value);
}

uint16_t als_data()
{
	return als_readw(SENSORLOW);
}

static void als_writeb(uint8_t addr, uint8_t b)
{
	uint8_t buf[2];
	buf[0] = addr | (1 << 6);
	buf[1] = b;
	i2c_tx(ALS_I2C, ALS_ADDR, buf, sizeof(buf));
}

static uint8_t als_readb(uint8_t addr)
{
	uint8_t registers[1];
	uint8_t ret[1];

	registers[0] = addr | (1 << 6);

	ret[0] = 0;

	i2c_rx(ALS_I2C, ALS_ADDR, registers, 1, ret, sizeof(ret)); 

	return ret[0];
}

static uint16_t als_readw(uint8_t addr)
{
	uint8_t registers;
	uint16_t ret;

	registers = addr | (1 << 6);
	ret = 0;

	i2c_rx(ALS_I2C, ALS_ADDR, &registers, 1, &ret, sizeof(ret));

	return ret;
}

static void als_clearint()
{
	als_setchannel(use_channel);
}
