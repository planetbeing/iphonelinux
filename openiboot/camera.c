#include <openiboot.h>
#include <gpio.h>
#include <camera.h>
#include <hardware/camera.h>
#include <timer.h>
#include <i2c.h>
#include <util.h>


void camera_setup() {
	uint8_t registers[2];
	uint8_t buffer[2];

    	bufferPrintf("setup camera....\n");

	gpio_custom_io(GPIO_function_clock_enable, 0x02);
	gpio_pin_output(GPIO_function_power_on, 1);
	
	gpio_pin_output(GPIO_function_standby, 1);
	gpio_pin_output(GPIO_function_reset, 0);
	
	udelay(1000);
	
	gpio_pin_output(GPIO_function_standby, 0);
	gpio_pin_output(GPIO_function_reset, 1);
	
	udelay(1000);

	

	registers[0] = 0x30;
	registers[1] = 0x00;
	buffer[0] = 0xDE;
	buffer[1] = 0xAD;

	i2c_rx(1, camera_i2c_slave_read, registers, 2, buffer, 2);
	bufferPrintf("camera sensor model ID = 0x%x\r\n", (buffer[0] << 8) | buffer[1]);
	
}


