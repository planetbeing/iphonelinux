#ifndef HW_MULTITOUCH_H
#define HW_MULTITOUCH _H

#include "hardware/spi.h"

#ifdef CONFIG_IPHONE
#define MT_GPIO_POWER 0x804
#define MT_ATN_INTERRUPT 0xa3
#else
#define MT_GPIO_POWER 0x701
#define MT_ATN_INTERRUPT 0x9b
#endif

#ifdef CONFIG_3G
#define MT_SPI 1
#define MT_SPI_CS GPIO_SPI1_CS0
#else
#define MT_SPI 2
#define MT_SPI_CS GPIO_SPI2_CS0
#endif

#define MT_INFO_FAMILYID 0xD1
#define MT_INFO_SENSORINFO 0xD3
#define MT_INFO_SENSORREGIONDESC 0xD0
#define MT_INFO_SENSORREGIONPARAM 0xA1
#define MT_INFO_SENSORDIM 0xD9

#endif
