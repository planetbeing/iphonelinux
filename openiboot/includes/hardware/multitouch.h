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

/*
 * The kernel specifies speeds at ratios that would make the normal speed 12X that of spi 0's lcd, which makes it around 6000000.
 * This definitely does not work in practice. The limit is between 1000000 - 1500000. ASpeed is 1.5X faster than normal speed,
 * but in practice can go up to around 4500000. The normal command speed is less in ASpeed mode than normal mode, even though
 * the payload transmit speed can be much higher
 */

#define MT_NORMAL_SPEED 83000 
#define MT_ASPEED 4500000

#endif
