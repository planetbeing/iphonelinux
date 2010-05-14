#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "util.h"
#include "sdio.h"
#include "clock.h"
#include "gpio.h"
#include "timer.h"
#include "interrupt.h"
#include "hardware/sdio.h"

#define SDIO_CARD_BUSY   0x80000000      /* Card Power up status bit */

#define ERROR_TIMEOUT (-2)
#define ERROR_ALIGN (-3)
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
int sdio_io_rw_extended(int isWrite, int function, uint32_t address, int incr_addr, void* buffer, int blocks, int count);
int sdio_io_rw_ext_helper(int isWrite, int function, uint32_t address, int incr_addr, void* buffer, int count);
int sdio_read_cis(int function);
static void sdio_handle_interrupt(uint32_t token);

typedef struct SDIOFunction
{
	int blocksize;
	int maxBlockSize;
	int enableTimeout;
	InterruptServiceRoutine irqHandler;
	uint32_t irqHandlerToken;
} SDIOFunction;

uint32_t RCA;
int NumberOfFunctions;

SDIOFunction* SDIOFunctions;

int sdio_setup()
{
	int i;

	interrupt_install(0x2a, sdio_handle_interrupt, 0);
	interrupt_enable(0x2a);

	clock_gate_switch(SDIO_CLOCKGATE, ON);

	// SDCLK = PCLK/128 ~= 400 KHz
	SET_REG(SDIO + SDIO_CLKDIV, 1 << 7);

	// Reset FIFO
	SET_REG(SDIO + SDIO_DCTRL, 0x3);
	SET_REG(SDIO + SDIO_DCTRL, 0x0);
	
	// Enable SDIO, with 1-bit card bus.
	SET_REG(SDIO + SDIO_CTRL, 0x1);

	// Clear status
	SET_REG(SDIO + SDIO_STAC, 0xFFFFFFFF);

	// Clear IRQ
	SET_REG(SDIO + SDIO_IRQ, 0xFFFFFFFF);

	if(sdio_reset() != 0)
	{
		bufferPrintf("sdio: error resetting card\r\n");
		return -1;
	}

	// Enable all IRQs
	SET_REG(SDIO + SDIO_IRQMASK, 3);

	// Enable SDIO IRQs
	SET_REG(SDIO + SDIO_CSR, 2);

	uint32_t ocr = 0;
	if(sdio_send_io_op_cond(0, &ocr) != 0)
	{
		bufferPrintf("sdio: error querying card operating conditions\r\n");
		return -1;
	}

	// eliminate values that appear too low
	ocr &= ~0x7F;

	NumberOfFunctions = (ocr & 0x70000000) >> 28;

	// we're going to go ahead and accept everything
	for(i = 23; i >= 0; --i)
	{
		if((ocr & (1 << i)) != 0)
		{
			ocr = 1 << i;
			bufferPrintf("sdio: selecting voltage index %d\r\n", i);
			if(sdio_send_io_op_cond(ocr, NULL) != 0)
			{
				bufferPrintf("sdio: error setting card operating conditions\r\n");
				return -1;
			}
		}
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

	bufferPrintf("sdio: cccr version: %d, sdio version: %d, low-speed: %d, high-speed: %d, wide bus: %d, multi-block: %d, functions: %d\r\n",
			cccr_version, sdio_version, lowspeed, highspeed, widebus, multiblock, NumberOfFunctions);

	if(!lowspeed)
	{
		// Crank us up to PCLK/4 ~= 25 MHz
		SET_REG(SDIO + SDIO_CLKDIV, 1 << 1);
	}

	if(highspeed)
	{
		if(sdio_io_rw_direct(FALSE, 0, 0x13, data, 0) != 0)
		{
			bufferPrintf("sdio: could not set high-speed\r\n");
			return -1;
		}
	}

	{
		uint8_t readBack;

		if(sdio_io_rw_direct(FALSE, 0, 0x7, 0, &data) != 0)
		{
			bufferPrintf("sdio: could not get bus interface control\r\n");
			return -1;
		}

		if(widebus)
		{
			// set wide bus on the card
			data = (data & 0x3) | 2;
		}

		if((data & (1 << 7)) == 0)
		{
			bufferPrintf("sdio: turning off pull-up resistor on DAT[3]\r\n");
			data |= 1 << 7;
		}

		if(sdio_io_rw_direct(TRUE, 0, 0x7, data, &readBack) != 0 || (readBack != data))
		{
			bufferPrintf("sdio: could not set bus interface control\r\n");
			return -1;
		}

		// set wide bus on the controller
		SET_REG(SDIO + SDIO_CTRL, GET_REG(SDIO + SDIO_CTRL) | (1 << 2));
	}

	SDIOFunctions = (SDIOFunction*) malloc((NumberOfFunctions + 1) * sizeof(SDIOFunction));

	for(i = 0; i <= NumberOfFunctions; ++i)
	{
		SDIOFunctions[i].irqHandler = NULL;

		if(sdio_read_cis(i) != 0)
		{
			bufferPrintf("sdio: could not read CIS for function %d\r\n", i);
			free(SDIOFunctions);
			return -1;
		}

		// initialize the maximum block size
		if(sdio_set_block_size(i, SDIOFunctions[i].maxBlockSize) != 0)
		{
			bufferPrintf("sdio: could not set function block size\r\n");
			free(SDIOFunctions);
			return -1;
		}
	}

	bufferPrintf("sdio: Ready!\r\n");

	return 0;
}

int sdio_read_cis(int function)
{
	int i;
	uint8_t data;

	uint32_t ptr = 0;
	for(i = 0; i < 3; ++i)
	{
		if(sdio_io_rw_direct(FALSE, 0, (function * 0x100) + 0x9 + i, 0, &data) != 0)
			return -1;

		ptr += data << (8 * i);
	}

	while(TRUE)
	{
		uint8_t type;
		uint8_t len;

		if(sdio_io_rw_direct(FALSE, 0, ptr++, 0, &type) != 0)
			return -1;

		if(type == 0xFF)
			break;

		if(sdio_io_rw_direct(FALSE, 0, ptr++, 0, &len) != 0)
			return -1;

		if(type != 0x22 && type != 0x20)
		{
			ptr += len;
			continue;
		}

		uint8_t buf[len];
		for(i = 0; i < len; ++i)
		{
			if(sdio_io_rw_direct(FALSE, 0, ptr++, 0, &buf[i]) != 0)
				return -1;
		}

		if(type == 0x22)
		{
			if(function == 0)
			{
				SDIOFunctions[function].maxBlockSize = buf[1] | (buf[2] << 8);
				SDIOFunctions[function].enableTimeout = 0;
				bufferPrintf("Function: %d, max block size: %d\r\n",
						function, SDIOFunctions[function].maxBlockSize);
			}
			else
			{
				SDIOFunctions[function].maxBlockSize = buf[12] | (buf[13] << 8);
				if(len > 29)
					SDIOFunctions[function].enableTimeout = (buf[28] | (buf[29] << 8)) * 10;
				else
					SDIOFunctions[function].enableTimeout = 1000;
				bufferPrintf("Function: %d, max block size: %d, enable timeout: %d ms\r\n",
					function, SDIOFunctions[function].maxBlockSize, SDIOFunctions[function].enableTimeout);
			}
		} else if(type == 0x20)
		{
			int manf = buf[0] | (buf[1] << 8);
			int prod = buf[2] | (buf[3] << 8);
			bufferPrintf("Manufacturer ID: 0x%x, product ID: 0x%x\r\n", manf, prod);
		}
	}

	return 0;
}

int sdio_set_block_size(int function, int blocksize)
{
	if(sdio_io_rw_direct(TRUE, 0, (function * 0x100) + 0x10, blocksize & 0xFF, NULL) != 0)
		return -1;

	if(sdio_io_rw_direct(TRUE, 0, (function * 0x100) + 0x11, (blocksize >> 8) & 0xFF, NULL) != 0)
		return -1;
	
	SDIOFunctions[function].blocksize = blocksize;

	return 0;
}

int sdio_wait_for_ready(int timeout)
{
	// wait for CMD_STATE to be CMD_IDLE, and DAT_STATE to be DAT_IDLE
	uint64_t startTime = timer_get_system_microtime();
	while(GET_REG(SDIO + SDIO_STATE) != 0)
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

#ifdef SDIO_GPIO_POWER
	gpio_pin_output(SDIO_GPIO_POWER, 0);
	udelay(5000);
	gpio_pin_output(SDIO_GPIO_POWER, 1);
	udelay(10000);
#endif

#ifdef SDIO_GPIO_DEVICE_RESET
	gpio_pin_output(SDIO_GPIO_DEVICE_RESET, 1);
	udelay(5000);
	gpio_pin_output(SDIO_GPIO_DEVICE_RESET, 0);
	udelay(10000);
#endif

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	sdio_clear_state();

#if 0
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
#endif

	return 0;
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
		{
			bufferPrintf("sdio: sdio_send_io_op_cond error, sdio status flags =  %x\r\n", ret);
			return -1;
		}

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
		{
			bufferPrintf("sdio: error, response flags = 0x%x\r\n", flags);
			return -1;
		}

		if(out)
			*out = resp & 0xFF;

		return 0;
	} else
	{
		bufferPrintf("sdio: sdio_io_rw_direct error, sdio status flags = %x\r\n", ret);
		return -1;
	}
}

int sdio_io_rw_extended(int isWrite, int function, uint32_t address, int incr_addr, void* buffer, int blocks, int count)
{
	int ret;

	uint32_t buf = (uint32_t) buffer;

	if((buf & ~0x3) != buf)
		return ERROR_ALIGN;

	ret = sdio_wait_for_ready(100);
	if(ret)
		return ret;

	sdio_clear_state();

	CleanAndInvalidateCPUDataCache();

	SET_REG(SDIO + SDIO_BADDR, buf);

	uint32_t arg = 0;
	arg |= isWrite ? 0x80000000 : 0; // write bit
	arg |= function << 28;
	arg |= address << 9;
	arg |= count;

	if(incr_addr)
		arg |= 0x04000000;

	if(blocks)
	{
		// set block bit if we have a large number of bytes
		arg |= 0x08000000;

		SET_REG(SDIO + SDIO_BLKLEN, SDIOFunctions[function].blocksize);
		SET_REG(SDIO + SDIO_NUMBLK, count);

		// unknown register set in block mode
		SET_REG(SDIO + SDIO_CTRL, GET_REG(SDIO + SDIO_CTRL) | 0x8000);
	} else
	{
		SET_REG(SDIO + SDIO_BLKLEN, count);
		SET_REG(SDIO + SDIO_NUMBLK, 1);

		SET_REG(SDIO + SDIO_CTRL, GET_REG(SDIO + SDIO_CTRL) & ~0x8000);
	}

	SET_REG(SDIO + SDIO_DCTRL, 3);
	SET_REG(SDIO + SDIO_DCTRL, 0);

	// disable block transfer done IRQ, we're going to handle it via polling
	SET_REG(SDIO + SDIO_IRQMASK, GET_REG(SDIO + SDIO_IRQMASK) & ~3);

	// set the argument
	SET_REG(SDIO + SDIO_ARGU, arg);

	if(isWrite)
	{
		// Command 53 = 0x35, addressed with data, write, response type 5 (IO_RW_DIRECT)
		SET_REG(SDIO + SDIO_CMD, 0x35 | (3 << 6) | (1 << 8) | (5 << 16));
	}
	else
	{
		// Command 53 = 0x35, addressed with data, response type 5 (IO_RW_DIRECT)
		SET_REG(SDIO + SDIO_CMD, 0x35 | (3 << 6) | (5 << 16));
	}

	ret = sdio_wait_for_cmd_ready(100);
	if(ret)
		return ret;

	int status = sdio_execute_command(100);
	ret = status & 0xF;
	if(ret == 0)
	{
		int resp = GET_REG(SDIO + SDIO_RESP0);
		int flags = resp >> 8;

		//bufferPrintf("Response: 0x%x\r\n", resp);

		// see if everything except IO_CURRENT_STATE is in a no error state
		if((flags & (~0x30)) != 0)
			return -1;

		// clear the bits we acknlowedged
		sdio_clear_state();

		// start data transfer
		SET_REG(SDIO + SDIO_DCTRL, 0x10);

		// wait until all data transfer is done
		uint64_t startTime = timer_get_system_microtime();
		while((GET_REG(SDIO + SDIO_IRQ) & 1) == 0)
		{
			//bufferPrintf("dsta: 0x%x, fsta: 0x%x, state: 0x%x\r\n", GET_REG(SDIO + SDIO_DSTA), GET_REG(SDIO + SDIO_FSTA), GET_REG(SDIO + SDIO_STATE));
			// FIXME: yield
			if(has_elapsed(startTime, 100 * 1000)) {
				return ERROR_TIMEOUT;
			}
		}

		// clear the interrupt bit
		SET_REG(SDIO + SDIO_IRQ, 1);

		// reenable interrupts
		SET_REG(SDIO + SDIO_IRQMASK, GET_REG(SDIO + SDIO_IRQMASK) | 3);

		status = GET_REG(SDIO + SDIO_DSTA);

		CleanAndInvalidateCPUDataCache();

		SET_REG(SDIO + SDIO_CTRL, GET_REG(SDIO + SDIO_CTRL) & ~0x8000);

		return (status >> 15);
	} else
		return -1;
}

int sdio_io_rw_ext_helper(int isWrite, int function, uint32_t address, int incr_addr, void* buffer, int count)
{
	int ret;

	if(count >= SDIOFunctions[function].blocksize)
	{
		int blocks = count / SDIOFunctions[function].blocksize;

		ret = sdio_io_rw_extended(isWrite, function, address, incr_addr, buffer, 1, blocks);

		if(ret < 0)
			return ret;

		if(isWrite && ret != SUCCESS_DATA)
			return -1;

		if((!isWrite) && ret != 0)
			return -1;

		int done = blocks * SDIOFunctions[function].blocksize;

		address += done;
		buffer = (void*)(((uint32_t)buffer) + done);
		count -= done;
	}

	if(count == 0)
		return 0;

	ret = sdio_io_rw_extended(isWrite, function, address, incr_addr, buffer, 0, count);

	if(ret < 0)
		return ret;

	if(ret != SUCCESS_DATA)
		return -1;

	return 0;
}

int sdio_claim_irq(int function, InterruptServiceRoutine handler, uint32_t token)
{
	SDIOFunctions[function].irqHandler = handler;
	SDIOFunctions[function].irqHandlerToken = token;

	int ret;
	uint8_t reg;

	ret = sdio_io_rw_direct(FALSE, 0, 0x4, 0, &reg);
	if(ret)
		return ret;

	// function
	reg |= 1 << function;

	// master
	reg |= 1;

	ret = sdio_io_rw_direct(TRUE, 0, 0x4, reg, NULL);
	if(ret)
		return ret;

	return 0;
}

uint8_t sdio_readb(int function, uint32_t address, int* err_ret)
{
	uint8_t val;

	int ret = sdio_io_rw_direct(FALSE, function, address, 0, &val);
	if(ret)
	{
		if(err_ret)
			*err_ret = ret;

		return 0xFF;
	}

	*err_ret = 0;

	return val;
}

void sdio_writeb(int function, uint8_t val, uint32_t address, int* err_ret)
{
	int ret = sdio_io_rw_direct(TRUE, function, address, val, NULL);
	if(err_ret)
		*err_ret = ret;
}

int sdio_writesb(int function, uint32_t address, void* src, int count)
{
	return sdio_io_rw_ext_helper(TRUE, function, address, FALSE, src, count);
}

int sdio_readsb(int function, void* dst, uint32_t address, int count)
{
	return sdio_io_rw_ext_helper(FALSE, function, address, FALSE, dst, count);
}

int sdio_memcpy_toio(int function, uint32_t address, void* src, int count)
{
	return sdio_io_rw_ext_helper(TRUE, function, address, FALSE, src, count);
}

int sdio_memcpy_fromio(int function, void* dst, uint32_t address, int count)
{
	return sdio_io_rw_ext_helper(FALSE, function, address, TRUE, dst, count);
}

static void sdio_handle_interrupt(uint32_t token)
{
	int status = GET_REG(SDIO + SDIO_IRQ);	
	SET_REG(SDIO + SDIO_IRQ, status);

	// SDIO irq, find handler
	if(status & 2)
	{
		SET_REG(SDIO + SDIO_IRQMASK, GET_REG(SDIO + SDIO_IRQMASK) & ~2);
		uint8_t reg;

		int ret = sdio_io_rw_direct(FALSE, 0, 0x5, 0, &reg);
		if(!ret)
		{
			int i;
			for(i = 1; i <= 8; ++i)
			{
				if((reg & (1 << i)) != 0 && SDIOFunctions[i].irqHandler)
				{
					SDIOFunctions[i].irqHandler(SDIOFunctions[i].irqHandlerToken);
				}
			}
		}

		SET_REG(SDIO + SDIO_IRQMASK, GET_REG(SDIO + SDIO_IRQMASK) | 2);
	}
}

int sdio_enable_func(int function)
{
	int ret;
	uint8_t reg;

	ret = sdio_io_rw_direct(FALSE, 0, 0x2, 0, &reg);
	if (ret)
		goto err;

	reg |= 1 << function;

	ret = sdio_io_rw_direct(TRUE, 0, 0x2, reg, NULL);
	if (ret)
		goto err;

	uint64_t startTime = timer_get_system_microtime();
	while (1) {
		ret = sdio_io_rw_direct(FALSE, 0, 0x3, 0, &reg);
		if (ret)
			goto err;

		if (reg & (1 << function))
			break;

		if(has_elapsed(startTime, SDIOFunctions[function].enableTimeout * 1000))
		{
			ret = ERROR_TIMEOUT;
			goto err;
		}
	}

	bufferPrintf("sdio: enabled function %d\n", function);

	return 0;

err:
	bufferPrintf("sdio: failed to enable function %d\n", function);
	return ret;
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

