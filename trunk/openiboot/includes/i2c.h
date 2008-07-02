#ifndef I2C_H
#define I2C_H

#include "openiboot.h"
#include "clock.h"

typedef enum I2CError {
	I2CNoError = 0
} I2CError;

typedef enum I2CState {
	I2CDone = 0,
	I2CStart = 1
} I2CState;


typedef struct I2CInfo {
	uint32_t field_0;
	uint32_t frequency;
	I2CError error_code;
	int is_write;
	int cursor;
	I2CState state;
	int operation_result;
	int address;
	uint32_t current_iiccon;
	uint32_t current_iicstat;
	int num_regs;
	int* registers;
	int bufferLen;
	void* buffer;
	uint32_t iic_scl_gpio;
	uint32_t iic_sda_gpio;
	uint32_t register_IICCON;
	uint32_t register_IICSTAT;
	uint32_t register_IICADD;
	uint32_t register_IICDS;
	uint32_t register_IICLC;
	uint32_t register_14;
	uint32_t register_18;
	uint32_t register_1C;
	uint32_t register_20;
} I2CInfo;

int i2c_setup();
I2CError i2c_rx(int bus, int iicaddr, int* registers, int num_regs, void* buffer, int len);
I2CError i2c_tx(int bus, int iicaddr, void* buffer, int len);

#endif
