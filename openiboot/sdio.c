#include "openiboot.h"
#include "util.h"
#include "sdio.h"
#include "clock.h"
#include "gpio.h"
#include "timer.h"
#include "hardware/sdio.h"

#define SDIO_CARD_BUSY   0x80000000      /* Card Power up status bit */

#define ERROR_TIMEOUT (-2)
#define SUCCESS_NO_DATA 0
#define SUCCESS_DATA (2 << 4)

int sdio_wait_for_ready(int timeout);
int sdio_wait_for_cmd_ready(int timeout);
int sdio_execute_command(int timeout);
int sdio_reset();
void sdio_clear_state();
int sdio_select_card(uint32_t rca);
int sdio_send_io_op_cond(uint32_t ocr, uint32_t* rocr);
int sdio_get_rca(uint32_t* rca);
int sdio_io_rw_direct(int isWrite, int function, uint32_t address, uint8_t in, uint8_t* out);

uint32_t RCA;
int NumberOfFunctions;

int sdio_setup()
{
	clock_gate_switch(SDIO_CLOCKGATE, ON);

	// SDCLK = PCLK/128 ~= 400 KHz
	SET_REG(SDIO + SDIO_CLKDIV, 1 << 7);

	// Enable, 1-bit card bus, little endian
	SET_REG(SDIO + SDIO_CTRL, (1 << 0) | (1 << 5));

	//SET_REG(SDIO + SDIO_CTRL, (1 << 0) | (1 << 2) | (1 << 5));

	if(sdio_reset() != 0)
	{
		bufferPrintf("sdio: error resetting card\r\n");
		return -1;
	}

	uint32_t ocr = 0;
	if(sdio_send_io_op_cond(0, &ocr) != 0)
	{
		bufferPrintf("sdio: error querying card operating conditions\r\n");
		sdio_status();
		return -1;
	}

	// eliminate values that appear too low
	ocr &= ~0x7F;

	NumberOfFunctions = (ocr & 0x70000000) >> 28;

	// we're going to go ahead and accept everything
	if(sdio_send_io_op_cond(ocr, NULL) != 0)
	{
		bufferPrintf("sdio: error setting card operating conditions\r\n");
		return -1;
	}

	if(sdio_get_rca(&RCA) != 0)
	{
		bufferPrintf("sdio: error getting card relative address\r\n");
		return -1;
	}

	if(sdio_select_card(RCA) != 0)
	{
		bufferPrintf("sdio: error selecting card\r\n");
		return -1;
	}

	uint8_t data;

	// read version info at CCCR
	if(sdio_io_rw_direct(FALSE, 0, 0, 0, &data) != 0)
	{
		bufferPrintf("sdio: could not read card common control registers\r\n");
		return -1;
	}

	uint16_t cccr_version = data & 0xf;
	uint16_t sdio_version = (data >> 4) & 0xf;

	if(cccr_version > 2)
	{
		bufferPrintf("sdio: unrecognized CCCR version!\r\n");
		return -1;
	}

	if(sdio_version > 3)
	{
		bufferPrintf("sdio: unrecognized SDIO version!\r\n");
		return -1;
	}

	// read capabilities at CCCR
	if(sdio_io_rw_direct(FALSE, 0, 0x8, 0, &data) != 0)
	{
		bufferPrintf("sdio: could not read card capabilities\r\n");
		return -1;
	}

	int lowspeed = FALSE;
	int multiblock = FALSE;
	int widebus = FALSE;
	int highspeed = FALSE;

	if(data & (1 << 6))
		lowspeed = TRUE;
	else
		lowspeed = FALSE;

	if(lowspeed && (data & (1 <<7)))
		widebus = TRUE;
	else if(!lowspeed)
		widebus = TRUE;

	if(data & (1 << 1))
		multiblock = TRUE;
	else
		multiblock = FALSE;


	if(cccr_version >= 2)
	{
		// read high-speed capabilities at CCCR
		if(sdio_io_rw_direct(FALSE, 0, 0x13, 0, &data) != 0)
		{
			bufferPrintf("sdio: could not read high-speed capabilities\r\n");
			return -1;
		}

		if(data & (1 << 0))
			highspeed = TRUE;
		else
			highspeed = FALSE;
	}

	bufferPrintf("sdio: low-speed: %d, high-speed: %d, wide bus: %d, multi-block: %d, functions: %d\r\n",
			lowspeed, highspeed, widebus, multiblock, NumberOfFunctions);

	if(!lowspeed)
	{
		// Crank us up to PCLK/2 ~= 25 MHz
		SET_REG(SDIO + SDIO_CLKDIV, 1 << 0);
	}

	if(highspeed)
	{
		if(sdio_io_rw_direct(FALSE, 0, 0x13, data, 0) != 0)
		{
			bufferPrintf("sdio: could not set high-speed\r\n");
			return -1;
		}
	}

	if(widebus)
	{
		if(sdio_io_rw_direct(FALSE, 0, 0x7, 0, &data) != 0)
		{
			bufferPrintf("sdio: could not get bus interface control\r\n");
			return -1;
		}

		// set wide bus on the card
		data = (data & 0x3) | 2;

		if(sdio_io_rw_direct(FALSE, 0, 0x7, data, 0) != 0)
		{
			bufferPrintf("sdio: could not set bus interface control\r\n");
			return -1;
		}

		// set wide bus on the controller
		SET_REG(SDIO + SDIO_CTRL, GET_REG(SDIO + SDIO_CTRL) | (1 << 2));
	}

	bufferPrintf("sdio: Ready!\r\n");

	return 0;
}

int sdio_wait_for_ready(int timeout)
{
	// wait for CMD_STATE to be CMD_IDLE
	uint64_t startTime = timer_get_system_microtime();
	while(((GET_REG(SDIO + SDIO_STATE) >> 4) & 3) != 0)
	{
		// FIXME: yield
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	return 0;

}

void sdio_clear_state()
{
	// clear current status
	SET_REG(SDIO + SDIO_STAC, GET_REG(SDIO + SDIO_DSTA));
}

int sdio_wait_for_cmd_ready(int timeout)
{
	// wait for the command to be ready to transmit
	uint64_t startTime = timer_get_system_microtime();
	while((GET_REG(SDIO + SDIO_DSTA) & 1) == 0)
	{
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	return 0;
}

int sdio_execute_command(int timeout)
{
	// set the execute bit
	SET_REG(SDIO + SDIO_CMD, GET_REG(SDIO + SDIO_CMD) | (1 << 31));

	// wait for the command to at least get transmitted
	uint64_t startTime = timer_get_system_microtime();
	while(((GET_REG(SDIO + SDIO_DSTA) >> 2) & 1) == 0)
	{
		// FIXME: yield
		if(has_elapsed(startTime, timeout * 1000)) {
			return ERROR_TIMEOUT;
		}
	}

	// wait for the command status to get back to idle
	// the reason command sent status is checked first is to avoid race conditions
	// where the state hasn't yet transitioned from idle
	while(((GET_REG(SDIO + SDIO_STATE) >> 4) & 7) != 0)
	{
		// FIXME: yield
		// No check here because device has its own timeout status
	}

	int status = GET_REG(SDIO + SDIO_DSTA);
	return (status >> 15);
}

int sdio_reset()
{
	int ret;

	gpio_pin_output(SDIO_GPIO_DEVICE_RESET, 1);
	udelay(10000);
	gpio_pin_output(SDIO_GPIO_DEVICE_RESET, 0);
	udelay(10000);

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	sdio_clear_state();

	SET_REG(SDIO + SDIO_ARGU, 0);
	SET_REG(SDIO + SDIO_CMD, 0);

	ret = sdio_wait_for_cmd_ready(100);
	if(ret)
		return ret;

	ret = sdio_execute_command(100);
	if(ret == SUCCESS_NO_DATA)
		return 0;
	else
		return -1;
}

int sdio_send_io_op_cond(uint32_t ocr, uint32_t* rocr)
{
	int ret;

	// clear the upper bits that would indicate card status
	ocr &= 0x1FFFFFF;

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	uint32_t response = 0;

	int i;
	for(i = 0; i < 100; ++i)
	{
		sdio_clear_state();

		SET_REG(SDIO + SDIO_ARGU, ocr);

		// Command 0x5, has response, response type 4
		SET_REG(SDIO + SDIO_CMD, 0x5 | (1 << 6) | (4 << 16));

		ret = sdio_wait_for_cmd_ready(100);
		if(ret)
			return ret;

		ret = sdio_execute_command(100);

		// CMD5 has a special format with reserved bits 1-7, so we ignore the CRC bit
		if((ret & (~8)) != SUCCESS_NO_DATA)
			return ret;

		response = GET_REG(SDIO + SDIO_RESP0);

		// just probing, do a single pass
		if(ocr == 0)
		{
			ret = 0;
			break;
		}

		if(response & SDIO_CARD_BUSY)
		{
			ret = 0;
			break;
		}

		ret = ERROR_TIMEOUT;

		udelay(100);
	}

	if(ret)
		return ret;

	if(rocr)
		*rocr = response;

	return 0;
}

// Gets the Relative Card Address
int sdio_get_rca(uint32_t* rca)
{
	int ret;

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	sdio_clear_state();

	// SEND_RELATIVE_ADDRESS(0) to probe for the address
	SET_REG(SDIO + SDIO_ARGU, 0);

	// Command 0x3, has response, response type 6 (SEND_RELATIVE_ADDRESS)
	SET_REG(SDIO + SDIO_CMD, 0x3 | (1 << 6) | (6 << 16));

	ret = sdio_wait_for_cmd_ready(100);
	if(ret)
		return ret;

	ret = sdio_execute_command(100);
	if(ret == SUCCESS_NO_DATA)
	{
		if(rca)
			*rca = GET_REG(SDIO + SDIO_RESP0) >> 16;
		return 0;
	}
	else
		return -1;

}

int sdio_select_card(uint32_t rca)
{
	int ret;

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	sdio_clear_state();

	SET_REG(SDIO + SDIO_ARGU, rca << 16);

	if(rca)
	{
		// Command 0x3, addressed, response type 1 (SELECT_CARD)
		SET_REG(SDIO + SDIO_CMD, 0x7 | (2 << 6) | (1 << 16));
	} else
	{
		// Deselect all cards
		// Command 0x3, addressed, no response
		SET_REG(SDIO + SDIO_CMD, 0x7 | (2 << 6));
	}

	ret = sdio_wait_for_cmd_ready(100);
	if(ret)
		return ret;

	ret = sdio_execute_command(100);
	if(ret == SUCCESS_NO_DATA)
		return 0;
	else
		return -1;

}

int sdio_io_rw_direct(int isWrite, int function, uint32_t address, uint8_t in, uint8_t* out)
{
	int ret;

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	sdio_clear_state();

	uint32_t arg = 0;
	arg |= isWrite ? 0x80000000 : 0; // write bit
	arg |= isWrite ? in : 0; // write value
	arg |= function << 28;
	arg |= address << 9;

	if(isWrite && out)
	{
		// set read-after-write if we want a value out
		arg |= 0x08000000;
	}

	SET_REG(SDIO + SDIO_ARGU, arg);

	// Command 52 = 0x34, addressed, response type 5 (IO_RW_DIRECT)
	SET_REG(SDIO + SDIO_CMD, 0x34 | (2 << 6) | (5 << 16));

	ret = sdio_wait_for_cmd_ready(100);
	if(ret)
		return ret;

	ret = sdio_execute_command(100);
	if(ret == SUCCESS_NO_DATA)
	{
		int resp = GET_REG(SDIO + SDIO_RESP0);
		int flags = resp >> 8;

		// see if everything except IO_CURRENT_STATE is in a no error state
		if((flags & (~0x30)) != 0)
			return -1;

		if(out)
			*out = resp & 0xFF;

		return 0;
	} else
		return -1;
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
	bufferPrintf("Status: 0x%x\r\n", (GET_REG(SDIO + SDIO_DSTA) >> 15) & 0xF);
	bufferPrintf("OCR: 0x%x\r\n", GET_REG(SDIO + SDIO_RESP0) >> 7);
}

