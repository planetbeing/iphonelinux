#ifndef HW_WM8958_H
#define HW_WM8958_H

#include "hardware/s5l8900.h"

#ifdef CONFIG_IPOD
#define WMCODEC_I2C 1
#else
#define WMCODEC_I2C 0
#endif

#endif
