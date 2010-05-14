#ifndef HW_WM8958_H
#define HW_WM8958_H

#include "hardware/s5l8900.h"

#ifdef CONFIG_IPOD
#define WMCODEC_I2C 1
#endif

#ifdef CONFIG_IPHONE
#define WMCODEC_I2C 0
#endif

#ifdef CONFIG_3G
#define WMCODEC_I2C 0
#define WMCODEC_I2C_SLAVE_ADDR 0x36
#else
#define WMCODEC_I2C_SLAVE_ADDR 0x34
#endif

#define WMCODEC_INT 0x2c
#define WMCODEC_INT_GPIO 0x1604

#endif
