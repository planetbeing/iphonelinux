#ifndef HW_SDIO_H
#define HW_SDIO_H

#include "hardware/s5l8900.h"

#define SDIO 0x38D00000
#define SDIO_CLOCKGATE 0xB

#ifdef CONFIG_IPOD
#define SDIO_GPIO_POWER 0x1701
#endif

#define SDIO_CTRL	0x0
#define SDIO_DCTRL	0x4
#define SDIO_CMD	0x8
#define SDIO_ARGU	0xC
#define SDIO_STATE	0x10
#define SDIO_STAC	0x14
#define SDIO_DSTA	0x18
#define SDIO_FSTA	0x1C
#define SDIO_RESP0	0x20
#define SDIO_RESP1	0x24
#define SDIO_RESP2	0x28
#define SDIO_RESP3	0x2C
#define SDIO_CLKDIV	0x30
#define SDIO_CSR	0x34
#define SDIO_IRQ	0x38
#define SDIO_IRQMASK	0x3C
#define SDIO_BADDR	0x44
#define SDIO_BLKLEN	0x48
#define SDIO_NUMBLK	0x4C
#define SDIO_REMBLK	0x50

#ifndef CONFIG_IPOD
#define SDIO_GPIO_DEVICE_RESET 0x607
#endif

#endif
