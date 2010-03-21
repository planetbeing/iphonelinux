#include <openiboot.h>
#include <gpio.h>
#include <camera.h>
#include <hardware/camera.h>
#include <timer.h>
#include <i2c.h>
#include <util.h>

static uint16_t camera_readw(uint16_t addr);

int camera_setup() {
	gpio_custom_io(CAMERA_GPIO_CLOCK_ENABLE, 0x02);
	gpio_pin_output(CAMERA_GPIO_POWER_ON, 1);
	
	gpio_pin_output(CAMERA_GPIO_STANDBY, 1);
	gpio_pin_output(CAMERA_GPIO_RESET, 0);
	
	udelay(1000);
	
	gpio_pin_output(CAMERA_GPIO_STANDBY, 0);
	gpio_pin_output(CAMERA_GPIO_RESET, 1);
	
	udelay(1000);

	uint16_t modelID = camera_readw(0x3000);


	if(modelID != 0x1580)
	{
		bufferPrintf("camera: unrecognized sensor model ID = 0x%x!\r\n", modelID);
		return -1;
	}

	bufferPrintf("camera: sensor model ID = 0x%x\r\n", modelID);

	return 0;
}

static uint16_t camera_readw(uint16_t addr)
{
	uint8_t registers[2];
	uint8_t buffer[2];

	registers[0] = (addr >> 8) & 0xFF;
	registers[1] = addr & 0xFF;
	buffer[0] = 0xDE;
	buffer[1] = 0xAD;

	i2c_rx(1, CAMERA_ADDR, registers, 2, buffer, 2);

	return (buffer[0] << 8) | buffer[1];
}
