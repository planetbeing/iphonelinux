#ifndef HW_ALS_H
#define HW_ALS_H

#define ALS_I2C 0

#ifdef CONFIG_3G
#define ALS_ADDR 0x88
#else
#define ALS_ADDR 0x92
#endif
#define ALS_INT 0x49

#endif
