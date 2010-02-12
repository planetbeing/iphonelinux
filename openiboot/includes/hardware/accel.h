#ifndef HW_ACCEL_H
#define HW_ACCEL_H

#include "hardware/s5l8900.h"

// See http://www.st.com/stonline/products/literature/ds/12726.pdf

#define ACCEL_I2C_BUS 0

// could be 0x72 and 0x73 as well
#define ACCEL_SETADDR	0x3A
#define ACCEL_GETADDR	0x3B

#define ACCEL_WHOAMI	0x0F
#define ACCEL_CTRL_REG1	0x20
#define ACCEL_CTRL_REG2	0x21
#define ACCEL_STATUS	0x27
#define ACCEL_OUTX	0x29
#define ACCEL_OUTY	0x2B
#define ACCEL_OUTZ	0x2D

#define ACCEL_WHOAMI_VALUE	0x3B

#define ACCEL_CTRL_REG1_DR	(1 << 7)
#define ACCEL_CTRL_REG1_PD	(1 << 6)
#define ACCEL_CTRL_REG1_FS	(1 << 5)
#define ACCEL_CTRL_REG1_STP	(1 << 4)
#define ACCEL_CTRL_REG1_STM	(1 << 3)
#define ACCEL_CTRL_REG1_ZEN	(1 << 2)
#define ACCEL_CTRL_REG1_YEN	(1 << 1)
#define ACCEL_CTRL_REG1_XEN	(1 << 0)

#define ACCEL_CTRL_REG2_SIM	(1 << 7)
#define ACCEL_CTRL_REG2_BOOT	(1 << 6)
#define ACCEL_CTRL_REG2_FDS	(1 << 4)
#define ACCEL_CTRL_REG2_HPEN2	(1 << 3)
#define ACCEL_CTRL_REG2_HPEN1	(1 << 2)
#define ACCEL_CTRL_REG2_HP2	(1 << 1)
#define ACCEL_CTRL_REG2_HP1	(1 << 0)

#endif
