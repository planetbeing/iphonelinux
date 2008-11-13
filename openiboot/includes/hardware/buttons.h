#ifndef HW_BUTTONS_H
#define HW_BUTTONS_H

#include "hardware/s5l8900.h"

// Device

#define BUTTONS_HOME_IPHONE 0x1600
#define BUTTONS_HOME_IPOD 0x1606

#ifdef CONFIG_IPOD
#define BUTTONS_HOME BUTTONS_HOME_IPOD
#else
#define BUTTONS_HOME BUTTONS_HOME_IPHONE
#endif

#define BUTTONS_HOLD 0x1605
#define BUTTONS_IIC_STATE 0x4B
#endif

