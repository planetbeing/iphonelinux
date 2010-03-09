#ifndef HW_I2S_H
#define HW_I2S_H

#define I2S0_CLOCK 0x27
#define I2S1_CLOCK 0x2A

#define I2S0 0x3CA00000
#define I2S1 0x3CD00000

#define I2S_CLKCON 0
#define I2S_TXCON 0x4
#define I2S_TXCOM 0x8
#define I2S_RXCON 0x30
#define I2S_RXCOM 0x34
#define I2S_STATUS 0x3C

#ifdef CONFIG_IPOD
#define WM_I2S I2S1
#define DMA_WM_I2S_TX DMA_I2S1_TX
#define DMA_WM_I2S_RX DMA_I2S1_RX
#else
#define WM_I2S I2S0
#define DMA_WM_I2S_TX DMA_I2S0_TX
#define DMA_WM_I2S_RX DMA_I2S0_RX
#define BB_I2S I2S1
#define DMA_BB_I2S_TX DMA_I2S1_TX
#define DMA_BB_I2S_RX DMA_I2S1_RX
#endif

#endif
