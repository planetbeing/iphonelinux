#include "openiboot.h"
#include "alsISL29003.h"
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
static void als_writew(uint8_t addr, uint16_t w);
static uint8_t als_readb(uint8_t addr);
static uint16_t als_readw(uint8_t addr);
static void als_clearint();
static void als_int(uint32_t token);

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
    // enable ADC-core, internal timing and mode3 
	als_writeb(COMMAND, 1<<7 | 0<<5 | 2<<2 );
	udelay(1000);

    if(als_readb(COMMAND) != 0x88)
    {
        bufferPrint("als: error initializing\r\n");
        return -1;
    }
	
	gpio_register_interrupt(ALS_INT, 1, 0, 0, als_int, 0);

	bufferPrintf("als: initialized\r\n");

	return 0;
}

void als_enable_interrupt()
{
	uint16_t sensordata = als_sensordata();

	if(sensordata > 0)
		als_setlowthreshold(sensordata - 1);
	else
		als_setlowthreshold(sensordata);

	if(sensordata < 0xFFFF)
		als_sethighthreshold(sensordata + 1);
	else
		als_sethighthreshold(0xFFFF);

	// Gain : 0-62272 lux, trigger if out of range for every consecutive integration cycles
	als_writeb(CONTROL, (3 << 2) | 0);
	//als_clearint();

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

	uint16_t timerdata = als_timerdata();
	uint16_t sensordata = als_sensordata();
	if(sensordata > 0)
		als_setlowthreshold(sensordata - 1);
	else
		als_setlowthreshold(sensordata);

	if(sensordata < 0xFFFF)
		als_sethighthreshold(sensordata + 1);
	else
		als_sethighthreshold(0xFFFF);

	if(lastData0 != sensordata)
		bufferPrintf("als: sensordata = %d, timerdata = %d\r\n", sensordata, timerdata);

	lastData0 = sensordata;

	als_clearint();
}

void als_setlowthreshold(uint16_t value)
{
	als_writew(INTTHRESHHIGH, value);
}

void als_sethighthreshold(uint16_t value)
{
	als_writew(INTTHRESHHIGH, value);
}

uint16_t als_sensordata()
{
	return als_readw(SENSORLOW);
}

// NOTE : timer data is only available in External Timing Mode (i.e. bit 5 in COMMAND register is set)
uint16_t als_timerdata()
{
	return als_readw(TIMERLOW);
}

static void als_writeb(uint8_t addr, uint8_t b)
{
	uint8_t buf[2];
	buf[0] = addr;
	buf[1] = b;
	i2c_tx(ALS_I2C, ALS_ADDR, buf, sizeof(buf));
}

static uint8_t als_readb(uint8_t addr)
{
	uint8_t registers[1];
	uint8_t ret[1];

	registers[0] = addr;

	ret[0] = 0;

	i2c_rx(ALS_I2C, ALS_ADDR, registers, 1, ret, sizeof(ret)); 

	return ret[0];
}

static uint16_t als_readw(uint8_t addr)
{
	uint8_t registers;
	uint16_t ret;

	registers = addr;
	ret = 0;

	i2c_rx(ALS_I2C, ALS_ADDR, &registers, 1, &ret, sizeof(ret));

	return ret;
}

static void als_writew(uint8_t addr, uint16_t w)
{
	uint8_t buf[4];
	buf[0] = addr;
	buf[1] = 2;
	buf[2] = w & 0xFF;
	buf[3] = (w >> 8) & 0xFF;
	i2c_tx(ALS_I2C, ALS_ADDR, buf, sizeof(buf));
}

static void als_clearint()
{
//	als_writeb(CONTROL, 0);
}
