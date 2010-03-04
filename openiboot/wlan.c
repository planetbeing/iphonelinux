#include "openiboot.h"
#include "util.h"
#include "sdio.h"
#include "interrupt.h"
#include "timer.h"
#include "wlan.h"

// No, I'm not really going to write a network stack here. But we're going to try to upload the firmware to
// make sure the I/O layer works properly

  
#define IF_SDIO_MODEL_8385      0x04
#define IF_SDIO_MODEL_8686      0x0b
#define IF_SDIO_MODEL_8688      0x10

#define IF_SDIO_IOPORT          0x00

#define IF_SDIO_H_INT_MASK      0x04
#define   IF_SDIO_H_INT_OFLOW   0x08
#define   IF_SDIO_H_INT_UFLOW   0x04
#define   IF_SDIO_H_INT_DNLD    0x02
#define   IF_SDIO_H_INT_UPLD    0x01

#define IF_SDIO_H_INT_STATUS    0x05
#define IF_SDIO_H_INT_RSR       0x06
#define IF_SDIO_H_INT_STATUS2   0x07

#define IF_SDIO_RD_BASE         0x10

#define IF_SDIO_STATUS          0x20
#define   IF_SDIO_IO_RDY        0x08
#define   IF_SDIO_CIS_RDY       0x04
#define   IF_SDIO_UL_RDY        0x02
#define   IF_SDIO_DL_RDY        0x01

#define IF_SDIO_C_INT_MASK      0x24
#define IF_SDIO_C_INT_STATUS    0x28
#define IF_SDIO_C_INT_RSR       0x2C

#define IF_SDIO_SCRATCH         0x34
#define IF_SDIO_SCRATCH_OLD     0x80fe
#define IF_SDIO_FW_STATUS       0x40
#define   IF_SDIO_FIRMWARE_OK   0xfedc

#define IF_SDIO_RX_LEN          0x42
#define IF_SDIO_RX_UNIT         0x43

#define IF_SDIO_EVENT           0x80fc

#define IF_SDIO_BLOCK_SIZE      256
#define CONFIGURATION_REG               0x03
#define HOST_POWER_UP                   (0x1U << 1)


static uint32_t scratch_reg;
static uint32_t ioport;

static uint16_t wlan_read_scratch(int* err)
{
	int ret;
	uint16_t scratch;

	scratch = sdio_readb(1, scratch_reg, &ret);
	if (!ret)
		scratch |= sdio_readb(1, scratch_reg + 1, &ret) << 8;

	if (err)
		*err = ret;

	if (ret)
		return 0xffff;

	return scratch;
}

void wlan_interrupt(uint32_t token)
{
}

int wlan_prog_helper(const uint8_t * firmware, int size)
{
	int ret;
	uint8_t status;
	uint8_t *chunk_buffer;
	uint32_t chunk_size;
	uint64_t startTime;

	bufferPrintf("wlan: programming firmware helper...\r\n");

	chunk_buffer = (uint8_t*) memalign(64, 4);
	if (!chunk_buffer) {
		ret = -1;
		goto release_fw;
	}

	ret = sdio_set_block_size(1, 32);
	if (ret)
		goto release;

	while (size) {
		startTime = timer_get_system_microtime();
		while (TRUE) {
			status = sdio_readb(1, IF_SDIO_STATUS, &ret);
			if (ret)
				goto release;

			if ((status & IF_SDIO_IO_RDY) &&
					(status & IF_SDIO_DL_RDY))
				break;

			if(has_elapsed(startTime, 1000 * 1000)) {
				ret = -1;
				goto release;
			}

			udelay(1000);
		}

		if(size > 60)
			chunk_size = 60;
		else
			chunk_size = size;


		*((uint32_t*)chunk_buffer) = chunk_size;
		memcpy(chunk_buffer + 4, firmware, chunk_size);

		//bufferPrintf("wlan: sending %d bytes chunk\r\n", chunk_size);
		
		ret = sdio_writesb(1, ioport,
				chunk_buffer, 64);
		if (ret)
			goto release;

		firmware += chunk_size;
		size -= chunk_size;
	}

	/* an empty block marks the end of the transfer */
	memset(chunk_buffer, 0, 4);
	ret = sdio_writesb(1, ioport, chunk_buffer, 64);
	if (ret)
		goto release;

	bufferPrintf("wlan: waiting for helper to boot\r\n");

	/* wait for the helper to boot by looking at the size register */
	startTime = timer_get_system_microtime();
	while (TRUE) {
		uint16_t req_size;

		req_size = sdio_readb(1, IF_SDIO_RD_BASE, &ret);
		if (ret)
			goto release;

		req_size |= sdio_readb(1, IF_SDIO_RD_BASE + 1, &ret) << 8;
		if (ret)
			goto release;

		if (req_size != 0)
			break;

		if(has_elapsed(startTime, 1000 * 1000)) {
			ret = -1;
			goto release;
		}

		udelay(10000);
	}

	ret = 0;
	bufferPrintf("wlan: helper has booted!\r\n");

release:
	free(chunk_buffer);

release_fw:

	if (ret)
		bufferPrintf("wlan: failed to load helper firmware\r\n");

	return ret;
}

int wlan_prog_real(const uint8_t* firmware, size_t size)
{
	int ret;
	uint8_t status;
	uint8_t *chunk_buffer;
	uint32_t chunk_size;
	size_t req_size;
	uint64_t startTime;

	bufferPrintf("wlan: programming firmware...\r\n");

	chunk_buffer = (uint8_t*) memalign(512, 4);
	if (!chunk_buffer) {
		ret = -1;
		goto release_fw;
	}
	
	ret = sdio_set_block_size(1, 32);
	if (ret)
		goto release;

	while (size) {
		startTime = timer_get_system_microtime();
		while (1) {
			status = sdio_readb(1, IF_SDIO_STATUS, &ret);
			if (ret)
				goto release;
			if ((status & IF_SDIO_IO_RDY) &&
					(status & IF_SDIO_DL_RDY))
				break;
			if(has_elapsed(startTime, 1000 * 1000)) {
				ret = -1;
				goto release;
			}
			udelay(1000);
		}

		req_size = sdio_readb(1, IF_SDIO_RD_BASE, &ret);
		if (ret)
			goto release;

		req_size |= sdio_readb(1, IF_SDIO_RD_BASE + 1, &ret) << 8;
		if (ret)
			goto release;

		//bufferPrintf("wlan: firmware helper wants %d bytes\r\n", (int)req_size);

		if (req_size == 0) {
			bufferPrintf("wlan: firmware helper gave up early\r\n");
			ret = -1;
			goto release;
		}

		if (req_size & 0x01) {
			bufferPrintf("wlan: firmware helper signalled error\r\n");
			ret = -1;
			goto release;
		}

		if (req_size > size)
			req_size = size;

		while (req_size) {
			if(req_size > 512)
				chunk_size = 512;
			else
				chunk_size = req_size;

			memcpy(chunk_buffer, firmware, chunk_size);

			//bufferPrintf("wlan: sending %d bytes (%d bytes) chunk\r\n",
			//   chunk_size, (chunk_size + 31) / 32 * 32);

			int to_send;
			to_send = chunk_size / 32;
			to_send *= 32;

			if(to_send < chunk_size)
				to_send += 32;

			ret = sdio_writesb(1, ioport,
					chunk_buffer, to_send);
			if (ret)
				goto release;

			firmware += chunk_size;
			size -= chunk_size;
			req_size -= chunk_size;
		}
	}

	ret = 0;

	bufferPrintf("wlan: waiting for firmware to boot\r\n");

	/* wait for the firmware to boot */
	startTime = timer_get_system_microtime();
	while (TRUE) {
		uint16_t scratch;

		scratch = wlan_read_scratch(&ret);
		if (ret)
			goto release;

		if (scratch == IF_SDIO_FIRMWARE_OK)
			break;

		if(has_elapsed(startTime, 1000 * 1000)) {
			ret = -1;
			goto release;
		}

		udelay(10000);
	}

	ret = 0;

	bufferPrintf("wlan: firmware booted!\r\n");

release:
	free(chunk_buffer);

release_fw:

	if (ret)
		bufferPrintf("wlan: failed to load firmware\r\n");

	return ret;
}

int wlan_setup()
{
	int ret;

	scratch_reg = 0x34;

	ret = sdio_enable_func(1);
	if(ret)
	{
		bufferPrintf("wlan: cannot enable function\r\n");
		return -1;
	}

	ret = sdio_claim_irq(1, wlan_interrupt, 0);
	if(ret)
	{
		bufferPrintf("wlan: cannot claim irq\r\n");
		return -1;
	}

	ioport = sdio_readb(1, 0x0, &ret);
	if(ret)
	{
		bufferPrintf("wlan: cannot read ioport\r\n");
		return -1;
	}

	ioport |= sdio_readb(1, 0x1, &ret) << 8;
	if(ret)
	{
		bufferPrintf("wlan: cannot read ioport\r\n");
		return -1;
	}


	ioport |= sdio_readb(1, 0x2, &ret) << 16;
	if(ret)
	{
		bufferPrintf("wlan: cannot read ioport\r\n");
		return -1;
	}

	bufferPrintf("wlan: ioport = 0x%x\r\n", ioport);

	uint16_t scratch = wlan_read_scratch(&ret);
	if(ret)
	{
		bufferPrintf("wlan: cannot read scratch register\r\n");
		return -1;
	}

	bufferPrintf("wlan: firmware status = 0x%x\r\n", scratch);

	return 0;
}

