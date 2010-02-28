#ifndef HW_SPI_H
#define HW_SPI_H

#include "hardware/s5l8900.h"

// Device
#define SPI0 0x3C300000
#define SPI1 0x3CE00000
#define SPI2 0x3D200000

// Registers

#define CONTROL 0x0
#define SETUP 0x4
#define STATUS 0x8
#define UNKREG1 0xC
#define TXDATA 0x10
#define RXDATA 0x20
#define CLKDIVIDER 0x30
#define UNKREG2 0x34
#define UNKREG3 0x38

// Values
#define MAX_TX_BUFFER 8
#define TX_BUFFER_LEFT(x) GET_BITS(status, 4, 4)
#define RX_BUFFER_LEFT(x) GET_BITS(status, 8, 4)

#define CLOCK_SHIFT 12
#define MAX_DIVIDER 0x3FF

#define SPI0_CLOCKGATE 0x22
#define SPI1_CLOCKGATE 0x2B
#define SPI2_CLOCKGATE 0x2F

#define SPI0_IRQ 0x9
#define SPI1_IRQ 0xA
#define SPI2_IRQ 0xB

#define GPIO_SPI0_CS0_IPHONE 0x400
#define GPIO_SPI0_CS0_IPOD 0x700

#ifdef CONFIG_IPOD
#define GPIO_SPI2_CS0 0x1804
#define GPIO_SPI2_CS1 0x705
#endif

#ifdef CONFIG_IPHONE
#define GPIO_SPI2_CS0 0x705
#endif

#ifdef CONFIG_IPOD
#define GPIO_SPI0_CS0 GPIO_SPI0_CS0_IPOD
#else
#define GPIO_SPI0_CS0 GPIO_SPI0_CS0_IPHONE
#endif

#define GPIO_SPI1_CS0 0x1800

#ifdef CONFIG_3G
#define GPIO_SPI0_CS1 0x705
#define GPIO_SPI0_CS2 0x706
#endif

#define NUM_SPIPORTS 3

#endif

