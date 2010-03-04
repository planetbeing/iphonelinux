#ifndef SDIO_H
#define SDIO_H

#include "openiboot.h"
#include "interrupt.h"

int sdio_setup();

void sdio_status();

int sdio_set_block_size(int function, int blocksize);
int sdio_claim_irq(int function, InterruptServiceRoutine handler, uint32_t token);
int sdio_enable_func(int function);
uint8_t sdio_readb(int function, uint32_t address, int* err_ret);
void sdio_writeb(int function, uint8_t val, uint32_t address, int* err_ret);
int sdio_writesb(int function, uint32_t address, void* src, int count);
int sdio_readsb(int function, void* dst, uint32_t address, int count);
int sdio_memcpy_toio(int function, uint32_t address, void* src, int count);
int sdio_memcpy_fromio(int function, void* dst, uint32_t address, int count);

#endif
