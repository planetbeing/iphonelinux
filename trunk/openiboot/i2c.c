#include "openiboot.h"
#include "i2c.h"
#include "hardware/i2c.h"
#include "util.h"
#include "clock.h"
#include "gpio.h"
#include "timer.h"

static const I2CInfo I2CInit[] = {
			{0, 0, I2CNoError, 0, 0, I2CDone, 0, 0, 0, 0, 0, NULL, 0, NULL, I2C0_SCL_GPIO, I2C0_SDA_GPIO, I2C0 + IICCON, I2C0 + IICSTAT, I2C0 + IICADD, I2C0 + IICDS, I2C0 + IICLC,
				I2C0 + IICREG14, I2C0 + IICREG18, I2C0 + IICREG1C, I2C0 + IICREG20},
			{0, 0, I2CNoError, 0, 0, I2CDone, 0, 0, 0, 0, 0, NULL, 0, NULL, I2C1_SCL_GPIO, I2C1_SDA_GPIO, I2C1 + IICCON, I2C1 + IICSTAT, I2C1 + IICADD, I2C1 + IICDS, I2C1 + IICLC,
				I2C1 + IICREG14, I2C1 + IICREG18, I2C1 + IICREG1C, I2C1 + IICREG20}
		};

static I2CInfo I2C[2];

static void init_i2c(I2CInfo* i2c, FrequencyBase freqBase);
static I2CError i2c_readwrite(I2CInfo* i2c);
static void do_i2c(I2CInfo* i2c);

int i2c_setup() {
	memcpy(I2C, I2CInit, sizeof(I2CInfo) * 2);

	clock_gate_switch(I2C0_CLOCKGATE, ON);
	clock_gate_switch(I2C1_CLOCKGATE, ON);

	init_i2c(&I2C[0], FrequencyBasePeripheral);
	init_i2c(&I2C[1], FrequencyBasePeripheral);

	return 0;
}

I2CError i2c_rx(int bus, int iicaddr, int* registers, int num_regs, void* buffer, int len) {
	I2C[bus].address = iicaddr;
	I2C[bus].is_write = FALSE;
	I2C[bus].registers = registers;
	I2C[bus].num_regs = num_regs;
	I2C[bus].bufferLen = len;
	I2C[bus].buffer = buffer;
	return i2c_readwrite(&I2C[bus]);
}

I2CError i2c_tx(int bus, int iicaddr, void* buffer, int len) {
	I2C[bus].address = iicaddr;
	I2C[bus].is_write = TRUE;
	I2C[bus].registers = NULL;
	I2C[bus].num_regs = 0;
	I2C[bus].bufferLen = len;
	I2C[bus].buffer = buffer;
	return i2c_readwrite(&I2C[bus]);
}

static void init_i2c(I2CInfo* i2c, FrequencyBase freqBase) {
	i2c->frequency = 256000000000ULL / clock_get_frequency(freqBase);
	int divisorRequired = 640000/i2c->frequency;
	int prescaler;
	if(divisorRequired < 512) {
		// round up
		i2c->current_iiccon = IICCON_INIT | IICCON_TXCLKSRC_FPCLK16;
		prescaler = ((divisorRequired + 0x1F) >> 5) - 1;
	} else {
		i2c->current_iiccon = IICCON_INIT | IICCON_TXCLKSRC_FPCLK512;
		prescaler = ((divisorRequired + 0x1FF) >> 9) - 1;
	}

	if(prescaler == 0)
		prescaler = 1;

	i2c->current_iiccon |= prescaler;

	gpio_custom_io(i2c->iic_sda_gpio, 0xE); // pull sda low?

	int i;
	for(i = 0; i < 19; i++) {
		gpio_custom_io(i2c->iic_scl_gpio, ((i % 2) == 1) ? 0xE : 0x0);
		udelay(5);
	}

	gpio_custom_io(i2c->iic_scl_gpio, 0x2);	// generate stop condition?
	gpio_custom_io(i2c->iic_sda_gpio, 0x2);
}

static I2CError i2c_readwrite(I2CInfo* i2c) {
	SET_REG(i2c->register_IICCON, i2c->current_iiccon);

	i2c->current_iiccon |= IICCON_ACKGEN;
	i2c->state = I2CStart;
	i2c->operation_result = 0;
	i2c->error_code = I2CNoError;

	SET_REG(i2c->register_20, IICCON_INIT);

	do_i2c(i2c);

	if(i2c->error_code != I2CNoError)
		return i2c->error_code;

	while(TRUE) {
		int hardware_status;
		do {
			hardware_status = GET_REG(i2c->register_20);
			SET_REG(i2c->register_20, hardware_status);
			i2c->operation_result &= ~hardware_status;
		} while(hardware_status != 0);

		if(i2c->state == I2CDone) {
			return i2c->error_code;
		}

		do_i2c(i2c);
	}

	return 0;
}

static void do_i2c(I2CInfo* i2c) {
	while(i2c->operation_result) {
		switch(i2c->state) {

		}
	}
}

