#include "openiboot.h"
#include "util.h"
#include "sdio.h"
#include "clock.h"
#include "gpio.h"
#include "hardware/sdio.h"

int sdio_setup()
{
	clock_gate_switch(SDIO_CLOCKGATE, ON);
	return 0;
}

void sdio_status()
{
	printf("SDIO controller registers:\n\n");
	printf("ctrl    = 0x%08x\n", (int) GET_REG(SDIO + SDIO_CTRL));
	printf("dctrl   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_DCTRL));
	printf("cmd     = 0x%08x\n", (int) GET_REG(SDIO + SDIO_CMD));
	printf("argu    = 0x%08x\n", (int) GET_REG(SDIO + SDIO_ARGU));
	printf("state   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_STATE));
	printf("stac    = 0x%08x\n", (int) GET_REG(SDIO + SDIO_STAC));
	printf("dsta    = 0x%08x\n", (int) GET_REG(SDIO + SDIO_DSTA));
	printf("fsta    = 0x%08x\n", (int) GET_REG(SDIO + SDIO_FSTA));
	printf("resp0   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_RESP0));
	printf("resp1   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_RESP1));
	printf("resp2   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_RESP2));
	printf("resp3   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_RESP3));
	printf("clkdiv  = 0x%08x\n", (int) GET_REG(SDIO + SDIO_CLKDIV));
	printf("csr     = 0x%08x\n", (int) GET_REG(SDIO + SDIO_CSR));
	printf("irq     = 0x%08x\n", (int) GET_REG(SDIO + SDIO_IRQ));
	printf("irqMask = 0x%08x\n", (int) GET_REG(SDIO + SDIO_IRQMASK));
	printf("baddr   = 0x%08x\n", (int) GET_REG(SDIO + SDIO_BADDR));
	printf("blklen  = 0x%08x\n", (int) GET_REG(SDIO + SDIO_BLKLEN));
	printf("numblk  = 0x%08x\n", (int) GET_REG(SDIO + SDIO_NUMBLK));
	printf("remblk  = 0x%08x\n", (int) GET_REG(SDIO + SDIO_REMBLK));
}

