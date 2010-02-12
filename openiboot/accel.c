#include "openiboot.h"
#include "util.h"
#include "accel.h"
#include "i2c.h"
#include "hardware/accel.h"

int accel_get_reg(int reg) {
	uint8_t registers[1];
	uint8_t out[1];

	registers[0] = reg;

	i2c_rx(ACCEL_I2C_BUS, ACCEL_GETADDR, registers, 1, out, 1);
	return out[0];
}

int accel_write_reg(int reg, int data, int verify) {
	uint8_t command[2];

	command[0] = reg;
	command[1] = data;

	i2c_tx(ACCEL_I2C_BUS, ACCEL_SETADDR, command, sizeof(command));

	if(!verify)
		return 0;

	uint8_t accelReg = reg;
	uint8_t buffer = 0;
	i2c_rx(ACCEL_I2C_BUS, ACCEL_GETADDR, &accelReg, 1, &buffer, 1);

	if(buffer == data)
		return 0;
	else
		return -1;
}

int accel_setup()
{
	int whoami = accel_get_reg(ACCEL_WHOAMI);
	if(whoami != ACCEL_WHOAMI_VALUE)
	{
		bufferPrintf("accel: incorrect whoami value\n");
		return -1;
	}

	accel_write_reg(ACCEL_CTRL_REG2, ACCEL_CTRL_REG2_BOOT, FALSE);
	accel_write_reg(ACCEL_CTRL_REG1, ACCEL_CTRL_REG1_PD | ACCEL_CTRL_REG1_XEN | ACCEL_CTRL_REG1_YEN | ACCEL_CTRL_REG1_ZEN, FALSE);
	return 0;
}

int accel_get_x()
{
	return (signed char)(accel_get_reg(ACCEL_OUTX));
}

int accel_get_y()
{
	return (signed char)(accel_get_reg(ACCEL_OUTY));
}

int accel_get_z()
{
	return (signed char)(accel_get_reg(ACCEL_OUTZ));
}


