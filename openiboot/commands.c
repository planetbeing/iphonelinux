#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "commands.h"
#include "util.h"
#include "nor.h"
#include "lcd.h"
#include "images.h"
#include "timer.h"
#include "mmu.h"
#include "arm.h"
#include "gpio.h"
#include "framebuffer.h"
#include "actions.h"
#include "nvram.h"
#include "pmu.h"
#include "dma.h"
#include "nand.h"
#include "ftl.h"
#include "i2c.h"
#include "hfs/fs.h"
#include "aes.h"
#include "accel.h"
#include "sdio.h"
#include "wdt.h"
#include "wmcodec.h"
#include "multitouch.h"
#include "wlan.h"
#include "radio.h"
#include "als.h"

void cmd_reboot(int argc, char** argv) {
	Reboot();
}

void cmd_poweroff(int argc, char** argv) {
	pmu_poweroff();
}

void cmd_md(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t len = parseNumber(argv[2]);
	bufferPrintf("dumping memory 0x%x - 0x%x\r\n", address, address + len);
	buffer_dump_memory(address, len);
}

void cmd_aes(int argc, char** argv)
{
	AESKeyType keyType;

	uint8_t* data = NULL;
	uint8_t* key = NULL;
	uint8_t* iv = NULL;

	int dataLength;
	int keyLength;
	int ivLength;

	if(argc < 4)
	{
		bufferPrintf("Usage: %s <enc/dec> <gid/uid/key> [data] [iv]\r\n", argv[0]);
		return;
	}

	if(strcmp(argv[2], "gid") == 0)
	{
		keyType = AESGID;
	}
	else if(strcmp(argv[2], "uid") == 0)
	{
		keyType = AESUID;
	}
	else
	{
		hexToBytes(argv[2], &key, &keyLength);
		keyType = AESCustom;
	}

	hexToBytes(argv[3], &data, &dataLength);

	if(argc > 4)
	{
		hexToBytes(argv[4], &iv, &ivLength);
	}

	if(strcmp(argv[1], "enc") == 0)
	{
		aes_encrypt(data, dataLength, keyType, key, iv);
		bytesToHex(data, dataLength);
		bufferPrintf("\r\n");
	}
	else if(strcmp(argv[1], "dec") == 0)
	{
		aes_decrypt(data, dataLength, keyType, key, iv);
		bytesToHex(data, dataLength);
		bufferPrintf("\r\n");
	}
	else
	{
		bufferPrintf("Usage: %s <enc/dec> <GID/UID/key> [data] [iv]\r\n", argv[0]);
	}

	if(data)
		free(data);

	if(iv)
		free(iv);

	if(key)
		free(key);
}

void cmd_hexdump(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t len = parseNumber(argv[2]);
	bufferPrintf("dumping memory 0x%x - 0x%x\r\n", address, address + len);
	hexdump(address, len);
}

void cmd_cat(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t len = parseNumber(argv[2]);
	addToBuffer((char*) address, len);
}

void cmd_nor_read(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <address> <offset> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t offset = parseNumber(argv[2]);
	uint32_t len = parseNumber(argv[3]);
	bufferPrintf("Reading 0x%x - 0x%x to 0x%x...\r\n", offset, offset + len, address);
	nor_read((void*)address, offset, len);
	bufferPrintf("Done.\r\n");
}

void cmd_nor_write(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <address> <offset> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t offset = parseNumber(argv[2]);
	uint32_t len = parseNumber(argv[3]);
	bufferPrintf("Writing 0x%x - 0x%x to 0x%x...\r\n", address, address + len, offset);
	nor_write((void*)address, offset, len);
	bufferPrintf("Done.\r\n");
}

void cmd_nor_erase(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <address again for confirmation>\r\n", argv[0]);
		return;
	}
	
	uint32_t addr1 = parseNumber(argv[1]);
	uint32_t addr2 = parseNumber(argv[2]);

	if(addr1 != addr2) {
		bufferPrintf("0x%x does not match 0x%x\r\n", addr1, addr2);
		return;
	}

	bufferPrintf("Erasing 0x%x - 0x%x...\r\n", addr1, addr1 + getNORSectorSize());
	nor_erase_sector(addr1);
	bufferPrintf("Done.\r\n");
}

void cmd_images_list(int argc, char** argv) {
	images_list();
}

void cmd_images_read(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <type> <address>\r\n", argv[0]);
		return;
	}

	Image* image = images_get(fourcc(argv[1]));
	void* imageData;
	size_t length = images_read(image, &imageData);
	uint32_t address = parseNumber(argv[2]);
	memcpy((void*)address, imageData, length);
	free(imageData);
	bufferPrintf("Read %d of %s to 0x%x - 0x%x\r\n", length, argv[1], address, address + length);
}

void cmd_kernel(int argc, char** argv) {
	uint32_t address;
	uint32_t size;

	if(argc < 3) {
		address = 0x09000000;
		size = received_file_size;
	} else {
		address = parseNumber(argv[1]);
		size = parseNumber(argv[2]);
	}

	set_kernel((void*) address, size);
	bufferPrintf("Loaded kernel at %08x - %08x\r\n", address, address + size);
}

void cmd_ramdisk(int argc, char** argv) {
	uint32_t address;
	uint32_t size;

	if(argc < 3) {
		address = 0x09000000;
		size = received_file_size;
	} else {
		address = parseNumber(argv[1]);
		size = parseNumber(argv[2]);
	}

	set_ramdisk((void*) address, size);
	bufferPrintf("Loaded ramdisk at %08x - %08x\r\n", address, address + size);
}

void cmd_rootfs(int argc, char** argv) {
	int partition;
	const char* fileName;

	if(argc < 3) {
		bufferPrintf("usage: %s <partition> <path>\r\n", argv[0]);
		return;
	} else {
		partition = parseNumber(argv[1]);
		fileName = argv[2];
	}

	set_rootfs(partition, fileName);
	bufferPrintf("set rootfs to %s on partition %d\r\n", fileName, partition);
}

void cmd_boot(int argc, char** argv) {
	char* arguments = "";

	if(argc >= 2) {
		arguments = argv[1];
	}

	bufferPrintf("Booting kernel with arguments (%s)...\r\n", arguments);

	boot_linux(arguments);
}

void cmd_go(int argc, char** argv) {
	uint32_t address;

	if(argc < 2) {
		address = 0x09000000;
	} else {
		address = parseNumber(argv[1]);
	}

	bufferPrintf("Jumping to 0x%x (interrupts disabled)\r\n", address);

	// make as if iBoot was called from ROM
	pmu_set_iboot_stage(0x1F);

	udelay(100000);

	chainload(address);
}

void cmd_jump(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <address>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	bufferPrintf("Jumping to 0x%x\r\n", address);
	udelay(100000);

	CallArm(address);
}

void cmd_mwb(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <data>\r\n", argv[0]);
		return;
	}

	uint8_t* address = (uint8_t*) parseNumber(argv[1]);
	uint8_t data = parseNumber(argv[2]);
	*address = data;
	bufferPrintf("Written to 0x%x to 0x%x\r\n", (uint8_t)data, address);
}

void cmd_mws(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <string>\r\n", argv[0]);
		return;
	}

	char* address = (char*) parseNumber(argv[1]);
	strcpy(address, argv[2]);
	bufferPrintf("Written %s to 0x%x\r\n", argv[2], address);
}

void cmd_mw(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <data>\r\n", argv[0]);
		return;
	}

	uint32_t* address = (uint32_t*) parseNumber(argv[1]);
	uint32_t data = parseNumber(argv[2]);
	*address = data;
	bufferPrintf("Written to 0x%x to 0x%x\r\n", data, address);
}

void cmd_gpio_pinstate(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <port>\r\n", argv[0]);
		return;
	}

	uint32_t port = parseNumber(argv[1]);
	bufferPrintf("Pin 0x%x state: 0x%x\r\n", port, gpio_pin_state(port));
}

void cmd_gpio_out(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <port> [0|1]\r\n", argv[0]);
		return;
	}

	uint32_t port = parseNumber(argv[1]);
	uint32_t value = parseNumber(argv[2]);
	bufferPrintf("Pin 0x%x value: %d\r\n", port, value);
    gpio_pin_output(port,value);
}
void cmd_bgcolor(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <red> <green> <blue>\r\n", argv[0]);
		return;
	}

	uint8_t red = parseNumber(argv[1]);
	uint8_t green = parseNumber(argv[2]);
	uint8_t blue = parseNumber(argv[3]);

	lcd_fill((red << 16) | (green << 8) | blue);
}

void cmd_backlight(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <0-45>\r\n", argv[0]);
		return;
	}

	uint32_t level = parseNumber(argv[1]);
	lcd_set_backlight_level(level);
	bufferPrintf("backlight set to %d\r\n", level);
}

void cmd_echo(int argc, char** argv) {
	int i;
	for(i = 1; i < argc; i++) {
		bufferPrintf("%s ", argv[i]);
	}
	bufferPrintf("\r\n");
}

void cmd_clear(int argc, char** argv) {
	framebuffer_clear();
}

void cmd_printenv(int argc, char** argv) {
	nvram_listvars();
}

void cmd_setenv(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <name> <value>\r\n", argv[0]);
		return;
	}

	nvram_setvar(argv[1], argv[2]);
	bufferPrintf("Set %s = %s\r\n", argv[1], argv[2]);
}

void cmd_saveenv(int argc, char** argv) {
	bufferPrintf("Saving environment, this may take awhile...\r\n");
	nvram_save();
	bufferPrintf("Environment saved\r\n");
}

void cmd_install(int argc, char** argv) {
	bufferPrintf("Installing Images...\r\n");
	images_install(&_start, (uint32_t)&OpenIBootEnd - (uint32_t)&_start);
	bufferPrintf("Images installed\r\n");
}

void cmd_uninstall(int argc, char** argv) {
	images_uninstall();
}

void cmd_pmu_voltage(int argc, char** argv) {
	bufferPrintf("battery voltage: %d mV\r\n", pmu_get_battery_voltage());
}

void cmd_pmu_powersupply(int argc, char** argv) {
	PowerSupplyType power = pmu_get_power_supply();
	bufferPrintf("power supply type: ");
	switch(power) {
		case PowerSupplyTypeError:
			bufferPrintf("Unknown");
			break;
			
		case PowerSupplyTypeBattery:
			bufferPrintf("Battery");
			break;
			
		case PowerSupplyTypeFirewire:
			bufferPrintf("Firewire");
			break;
			
		case PowerSupplyTypeUSBHost:
			bufferPrintf("USB host");
			break;
			
		case PowerSupplyTypeUSBBrick500mA:
			bufferPrintf("500 mA brick");
			break;
			
		case PowerSupplyTypeUSBBrick1000mA:
			bufferPrintf("1000 mA brick");
			break;
	}
	bufferPrintf("\r\n");
}

void cmd_pmu_charge(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <on|off>\r\n", argv[0]);
		return;
	}

	if(strcmp(argv[1], "on") == 0) {
		pmu_charge_settings(TRUE, FALSE, FALSE);
		bufferPrintf("Charger on\r\n");
	} else if(strcmp(argv[1], "off") == 0) {
		pmu_charge_settings(FALSE, FALSE, TRUE);
		bufferPrintf("Charger off\r\n");
	} else {
		bufferPrintf("Usage: %s <on|off>\r\n", argv[0]);
		return;
	}
}

void cmd_pmu_nvram(int argc, char** argv) {
	uint8_t reg;

	pmu_get_gpmem_reg(PMU_IBOOTSTATE, &reg);
	bufferPrintf("0: [iBootState] %02x\r\n", reg);
	pmu_get_gpmem_reg(PMU_IBOOTDEBUG, &reg);
	bufferPrintf("1: [iBootDebug] %02x\r\n", reg);
	pmu_get_gpmem_reg(PMU_IBOOTSTAGE, &reg);
	bufferPrintf("2: [iBootStage] %02x\r\n", reg);
	pmu_get_gpmem_reg(PMU_IBOOTERRORCOUNT, &reg);
	bufferPrintf("3: [iBootErrorCount] %02x\r\n", reg);
	pmu_get_gpmem_reg(PMU_IBOOTERRORSTAGE, &reg);
	bufferPrintf("4: [iBootErrorStage] %02x\r\n", reg);
}

void cmd_dma(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <source> <dest> <size>\r\n", argv[0]);
		return;
	}

	uint32_t source = parseNumber(argv[1]);
	uint32_t dest = parseNumber(argv[2]);
	uint32_t size = parseNumber(argv[3]);

	int controller = 0;
	int channel = 0;
	bufferPrintf("dma_request: %d\r\n", dma_request(DMA_MEMORY, 4, 8, DMA_MEMORY, 4, 8, &controller, &channel, NULL));
	bufferPrintf("dma_perform(controller: %d, channel %d): %d\r\n", controller, channel, dma_perform(source, dest, size, FALSE, &controller, &channel));
	bufferPrintf("dma_finish(controller: %d, channel %d): %d\r\n", controller, channel, dma_finish(controller, channel, 500));
}

void cmd_nand_erase(int argc, char** argv)
{
	if(argc < 3) {
		bufferPrintf("Usage: %s <bank> <block> -- You probably don't want to do this.\r\n", argv[0]);
		return;
	}

	uint32_t bank = parseNumber(argv[1]);
	uint32_t block = parseNumber(argv[2]);

	bufferPrintf("Erasing bank %d, block %d...\r\n", bank, block);
	bufferPrintf("nand_erase: %d\r\n", nand_erase(bank, block));
}

void cmd_nand_read(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <address> <bank> <page> [pages]\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t bank = parseNumber(argv[2]);
	uint32_t page = parseNumber(argv[3]);
	uint32_t pages = 1;
	if(argc >= 5) {
		pages = parseNumber(argv[4]);
	}

	bufferPrintf("reading bank %d, pages %d - %d into %x\r\n", bank, page, page + pages - 1, address);
	NANDData* Data = nand_get_geometry();
	
	while(pages > 0) {	
		int ret = nand_read(bank, page, (uint8_t*) address, NULL, TRUE, FALSE);
		if(ret != 0)
			bufferPrintf("nand_read: %x\r\n", ret);

		pages--;
		page++;
		address += Data->bytesPerPage;
	}

	bufferPrintf("done!\r\n");
}

void cmd_nand_ecc(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <data> <ecc>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t ecc = parseNumber(argv[2]);

	bufferPrintf("nand_calculate_ecc(%x, %x) = %d\r\n", address, ecc, nand_calculate_ecc((uint8_t*) address, (uint8_t*) ecc));
}

void cmd_nand_write(int argc, char** argv) {
	if(argc < 6) {
		bufferPrintf("Usage: %s <data> <spare> <bank> <page> <ecc>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t spare = parseNumber(argv[2]);
	uint32_t bank = parseNumber(argv[3]);
	uint32_t page = parseNumber(argv[4]);
	uint32_t ecc = parseNumber(argv[5]);

	bufferPrintf("nand_write(%d, %d, %x, %x, %d) = %d\r\n", bank, page, address, spare, ecc, nand_write(bank, page, (uint8_t*) address, (uint8_t*) spare, ecc));
}

void cmd_nand_read_spare(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <address> <bank> <page> [pages]\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t bank = parseNumber(argv[2]);
	uint32_t page = parseNumber(argv[3]);
	uint32_t pages = 1;
	if(argc >= 5) {
		pages = parseNumber(argv[4]);
	}

	bufferPrintf("reading bank %d, pages %d - %d spare into %x\r\n", bank, page, page + pages - 1, address);
	NANDData* Data = nand_get_geometry();
	
	while(pages > 0) {	
		int ret = nand_read(bank, page, NULL, (uint8_t*) address, FALSE, FALSE);
		if(ret != 0)
			bufferPrintf("nand_read: %x\r\n", ret);

		pages--;
		page++;
		address += Data->bytesPerSpare;
	}

	bufferPrintf("done!\r\n");
}

void cmd_vfl_read(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <page>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t page = parseNumber(argv[2]);

	bufferPrintf("Reading virtual page %d into 0x%x\r\n", page, address);
	bufferPrintf("VFL_read: %x\r\n", VFL_Read(page, (uint8_t*) address, NULL, TRUE, NULL));
}

void cmd_vfl_erase(int argc, char** argv) {
	int count;

	if(argc < 2) {
		bufferPrintf("Usage: %s <block> [count]\r\n", argv[0]);
		return;
	}

	if(argc < 3) {
		count = 1;
	} else {
		count = parseNumber(argv[2]);
	}

	uint32_t block = parseNumber(argv[1]);
	uint32_t firstBlock = block;

	for(; block < (firstBlock + count); block++) {
		bufferPrintf("VFL_Erase(%d): %x\r\n", block, VFL_Erase(block));
	}
}

void cmd_ftl_read(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <address> <lpn> <pages>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t page = parseNumber(argv[2]);
	uint32_t pages = parseNumber(argv[3]);

	bufferPrintf("Reading %d pages, starting at %d into 0x%x\r\n", pages, page, address);
	bufferPrintf("FTL_read: %x\r\n", FTL_Read(page, pages, (uint8_t*) address));
}

void cmd_ftl_sync(int argc, char** argv) {
	bufferPrintf("Syncing FTL...\r\n");
	if(ftl_sync())
		bufferPrintf("Success!\r\n");
	else
		bufferPrintf("Error.\r\n");
}

void cmd_bdev_read(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <address> <offset> <bytes>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t offset = parseNumber(argv[2]);
	uint32_t bytes = parseNumber(argv[3]);

	bufferPrintf("Reading %d bytes, starting at %d into 0x%x\r\n", bytes, offset, address);
	bufferPrintf("ftl_read: %x\r\n", ftl_read((uint8_t*) address, offset, bytes));
}

void cmd_text(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <on|off>\r\n", argv[0]);
		return;
	}

	if(strcmp(argv[1], "on") == 0) {
		framebuffer_setdisplaytext(ON);
		bufferPrintf("Text display ON\r\n");
	} else if(strcmp(argv[1], "off") == 0) {
		framebuffer_setdisplaytext(OFF);
		bufferPrintf("Text display OFF\r\n");
	} else {
		bufferPrintf("Unrecognized option: %s\r\n", argv[1]);
	}
}

void cmd_malloc_stats(int argc, char** argv) {
	malloc_stats();
}

void cmd_frequency(int argc, char** argv) {
	bufferPrintf("Clock frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseClock));
	bufferPrintf("Memory frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseMemory));
	bufferPrintf("Bus frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseBus));
	bufferPrintf("Unknown frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseUnknown));
	bufferPrintf("Peripheral frequency: %d Hz\r\n", clock_get_frequency(FrequencyBasePeripheral));
	bufferPrintf("Display frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseDisplay));
	bufferPrintf("Fixed frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseFixed));
	bufferPrintf("Timebase frequency: %d Hz\r\n", clock_get_frequency(FrequencyBaseTimebase));
}

void cmd_ftl_mapping(int argc, char** argv) {
	ftl_printdata();
}

void cmd_nand_status(int agc, char** argv) {
	bufferPrintf("nand status: %x\r\n", nand_read_status());
}

void cmd_version(int argc, char** argv) {
	bufferPrintf("%s\r\n", OPENIBOOT_VERSION_STR);
}

void cmd_time(int argc, char** argv) {
	int day;
	int month;
	int year;
	int hour;
	int minute;
	int second;
	int day_of_week;
	pmu_date(&year, &month, &day, &day_of_week, &hour, &minute, &second);
	bufferPrintf("Current time: %02d:%02d:%02d, %s %02d/%02d/%02d GMT\r\n", hour, minute, second, get_dayofweek_str(day_of_week), month, day, year);
	//bufferPrintf("Current time: %02d:%02d:%02d, %s %02d/%02d/20%02d\r\n", pmu_get_hours(), pmu_get_minutes(), pmu_get_seconds(), pmu_get_dayofweek_str(), pmu_get_month(), pmu_get_day(), pmu_get_year());
	//bufferPrintf("Current time: %llu\n", pmu_get_epoch());
}

void cmd_iic_read(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("usage: %s <bus> <address> <register>\n", argv[0]);
		return;
	}


	int bus = parseNumber(argv[1]);
	int address = parseNumber(argv[2]);
	uint8_t registers[1];
	uint8_t out[1];

	registers[0] = parseNumber(argv[3]);

	int error = i2c_rx(bus, address, registers, 1, out, 1);
	
	bufferPrintf("result: %d, error: %d\r\n", (int) out[0], error);
}

void cmd_iic_write(int argc, char** argv) {
	if(argc < 5) {
		bufferPrintf("usage: %s <bus> <address> <register> <value>\r\n", argv[0]);
		return;
	}

	uint8_t buffer[2];
	int bus = parseNumber(argv[1]);
	int address = parseNumber(argv[2]);
	buffer[0] = parseNumber(argv[3]);
	buffer[1] = parseNumber(argv[4]);

	int error = i2c_tx(bus, address, buffer, 2);
	
	bufferPrintf("result: %d\r\n", error);
}

void cmd_accel(int argc, char** argv) {
	int x = accel_get_x();
	int y = accel_get_y();
	int z = accel_get_z();
	
	bufferPrintf("x: %d, y: %d, z: %d\r\n", x, y, z);
}

#ifndef CONFIG_IPOD
void cmd_als(int argc, char** argv) {
	bufferPrintf("data = %d\r\n", als_data());
}

void cmd_als_channel(int argc, char** argv) {
	if(argc < 2)
	{
		bufferPrintf("usage: %s <channel>\r\n", argv[0]);
		return;
	}

	int channel = parseNumber(argv[1]);
	bufferPrintf("Setting als channel to %d\r\n", channel);
	als_setchannel(channel);
}

void cmd_als_en(int argc, char** argv) {
	bufferPrintf("Enabling ALS interrupt.\r\n");
	als_enable_interrupt();
}

void cmd_als_dis(int argc, char** argv) {
	bufferPrintf("Disabling ALS interrupt.\r\n");
	als_disable_interrupt();
}
#endif

void cmd_sdio_status(int argc, char** argv) {
	sdio_status();
}

void cmd_sdio_setup(int argc, char** argv) {
	sdio_setup();
}


void cmd_wdt(int argc, char** argv)
{
	bufferPrintf("counter: %d\r\n", wdt_counter());
}

void cmd_audiohw_position(int argc, char** argv)
{
	bufferPrintf("playback position: %u / %u\r\n", audiohw_get_position(), audiohw_get_total());
}

void cmd_audiohw_pause(int argc, char** argv)
{
	audiohw_pause();
	bufferPrintf("Paused.\r\n");
}

void cmd_audiohw_resume(int argc, char** argv)
{
	audiohw_resume();
	bufferPrintf("Resumed.\r\n");
}

void cmd_audiohw_transfers_done(int argc, char** argv)
{
	bufferPrintf("transfers done: %d\r\n", audiohw_transfers_done());
}

void cmd_audiohw_play_pcm(int argc, char** argv)
{
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <len> [use-headphones]\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t len = parseNumber(argv[2]);
	uint32_t useHeadphones = 0;

	if(argc > 3)
		useHeadphones = parseNumber(argv[3]);

	if(useHeadphones)
	{
		bufferPrintf("playing PCM 0x%x - 0x%x using headphones\r\n", address, address + len);
		audiohw_play_pcm((void*)address, len, FALSE);
	} else
	{
		bufferPrintf("playing PCM 0x%x - 0x%x using speakers\r\n", address, address + len);
		audiohw_play_pcm((void*)address, len, TRUE);
	}
}

void cmd_audiohw_headphone_vol(int argc, char** argv)
{
	if(argc < 2)
	{
		bufferPrintf("%s <left> [right] (between 0:%d and 63:%d dB)\r\n", argv[0], audiohw_settings[SOUND_VOLUME].minval, audiohw_settings[SOUND_VOLUME].maxval);
		return;
	}

	int left = parseNumber(argv[1]);
	int right;

	if(argc >= 3)
		right = parseNumber(argv[2]);
	else
		right = left;

	audiohw_set_headphone_vol(left, right);

	bufferPrintf("Set headphone volumes to: %d / %d\r\n", left, right);
}

#ifndef CONFIG_IPOD
void cmd_audiohw_speaker_vol(int argc, char** argv)
{
	if(argc < 2)
	{
#ifdef CONFIG_3G
		bufferPrintf("%s <loudspeaker volume> (between 0 and 100)\r\n", argv[0]);
#else
		bufferPrintf("%s <loudspeaker volume> [speaker volume] (between 0 and 100... 'speaker' is the one next to your ear)\r\n", argv[0]);
#endif
		return;
	}

	int vol = parseNumber(argv[1]);

#ifdef CONFIG_3G
	audiohw_set_speaker_vol(vol);
	bufferPrintf("Set speaker volume to: %d\r\n", vol);
#else
	loudspeaker_vol(vol);

	bufferPrintf("Set loudspeaker volume to: %d\r\n", vol);

	if(argc > 2)
	{
		vol = parseNumber(argv[2]);
		speaker_vol(vol);

		bufferPrintf("Set speaker volume to: %d\r\n", vol);
	}
#endif
}
#endif

#ifdef CONFIG_IPHONE
void cmd_multitouch_setup(int argc, char** argv)
{
	if(argc < 5)
	{
		bufferPrintf("%s <a-speed fw> <a-speed fw len> <main fw> <main fw len>\r\n", argv[0]);
		return;
	}

	uint8_t* aspeedFW = (uint8_t*) parseNumber(argv[1]);
	uint32_t aspeedFWLen = parseNumber(argv[2]);
	uint8_t* mainFW = (uint8_t*) parseNumber(argv[3]);
	uint32_t mainFWLen = parseNumber(argv[4]);

	multitouch_setup(aspeedFW, aspeedFWLen, mainFW, mainFWLen);
}
#else
void cmd_multitouch_setup(int argc, char** argv)
{
	if(argc < 3)
	{
		bufferPrintf("%s <constructed fw> <constructed fw len>\r\n", argv[0]);
		return;
	}

	uint8_t* constructedFW = (uint8_t*) parseNumber(argv[1]);
	uint32_t constructedFWLen = parseNumber(argv[2]);

	multitouch_setup(constructedFW, constructedFWLen);
}
#endif

void cmd_wlan_prog_helper(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t len = parseNumber(argv[2]);

	wlan_prog_helper((void*) address, len);
}

void cmd_wlan_prog_real(int argc, char** argv) {
	if(argc < 3) {
		bufferPrintf("Usage: %s <address> <len>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	uint32_t len = parseNumber(argv[2]);

	wlan_prog_real((void*) address, len);
}

#ifndef CONFIG_IPOD
void cmd_radio_send(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <command>\r\n", argv[0]);
		return;
	}

	radio_write(argv[1]);
	radio_write("\r\n");
	
	char buf[100];
	int c = radio_read(buf, sizeof(buf));
	printf("radio reply: %s", buf);

	while(c == (sizeof(buf) - 1))
	{
		c = radio_read(buf, sizeof(buf));
		printf("%s", buf);
	}

	printf("\n");
}

void cmd_radio_nvram_list(int argc, char** argv) {
	radio_nvram_list();
}

void cmd_radio_register(int argc, char** argv) {
	bufferPrintf("Registering with cellular network...\r\n");
	if(radio_register(10 * 1000) != 0)
		bufferPrintf("Failed.\r\n");
}

void cmd_radio_call(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <phone number>\r\n", argv[0]);
		return;
	}

	bufferPrintf("Calling %s...\r\n", argv[1]);

	radio_call(argv[1]);
}

void cmd_radio_hangup(int argc, char** argv) {
	radio_hangup(argv[1]);
}

void cmd_vibrator_loop(int argc, char** argv)
{
	if(argc < 4) {
		bufferPrintf("Usage: %s <frequency 1-12> <period in ms> <time vibrator on during cycle in ms>\r\n", argv[0]);
		return;
	}

	int frequency = parseNumber(argv[1]);
	int period = parseNumber(argv[2]);
	int timeOn = parseNumber(argv[3]);

	bufferPrintf("Turning on vibrator at frequency %d in a %d ms cycle with %d duty time.\r\n", frequency, period, timeOn);

	vibrator_loop(frequency, period, timeOn);
}

void cmd_vibrator_once(int argc, char** argv)
{
	if(argc < 3) {
		bufferPrintf("Usage: %s <frequency 1-12> <duration in ms>\r\n", argv[0]);
		return;
	}

	int frequency = parseNumber(argv[1]);
	int time = parseNumber(argv[2]);

	bufferPrintf("Turning on vibrator at frequency %d for %d ms.\r\n", frequency, time);

	vibrator_once(frequency, time);
}

void cmd_vibrator_off(int argc, char** argv)
{
	bufferPrintf("Turning off vibrator.\r\n");

	vibrator_off();
}
#endif

void cmd_help(int argc, char** argv) {
	OPIBCommand* curCommand = CommandList;
	while(curCommand->name != NULL) {
		bufferPrintf("%-20s%s\r\n", curCommand->name, curCommand->description);
		curCommand++;
	}
}

OPIBCommand CommandList[] = 
	{
		{"install", "install openiboot onto the device", cmd_install},
		{"uninstall", "uninstall openiboot from the device", cmd_uninstall},
		{"reboot", "reboot the device", cmd_reboot},
		{"poweroff", "power off the device", cmd_poweroff},
		{"echo", "echo back a line", cmd_echo},
		{"clear", "clears the screen", cmd_clear},
		{"text", "turns text display on or off", cmd_text},
		{"md", "display a block of memory as 32-bit integers", cmd_md},
		{"mw", "write a 32-bit dword into a memory address", cmd_mw},
		{"mwb", "write a byte into a memory address", cmd_mwb},
		{"mws", "write a string into a memory address", cmd_mws},
		{"aes", "use the hardware crypto engine", cmd_aes},
		{"hexdump", "display a block of memory like 'hexdump -C'", cmd_hexdump},
		{"cat", "dumps a block of memory", cmd_cat},
		{"gpio_pinstate", "get the state of a GPIO pin", cmd_gpio_pinstate},
		{"gpio_out", "set the state of a GPIO pin", cmd_gpio_out},
		{"dma", "perform a DMA transfer", cmd_dma},
		{"nand_read", "read a page of NAND into RAM", cmd_nand_read},
		{"nand_write", "write a page of NAND", cmd_nand_write},
		{"nand_read_spare", "read a page of NAND's spare into RAM", cmd_nand_read_spare},
		{"nand_status", "read NAND status", cmd_nand_status},
		{"nand_ecc", "hardware ECC a page", cmd_nand_ecc},
		{"nand_erase", "erase a NAND block", cmd_nand_erase},
		{"vfl_read", "read a page of VFL into RAM", cmd_vfl_read},
		{"vfl_erase", "erase a block of VFL", cmd_vfl_erase},
		{"ftl_read", "read a page of FTL into RAM", cmd_ftl_read},
		{"ftl_mapping", "print FTL mapping information", cmd_ftl_mapping},
		{"ftl_sync", "commit the current FTL context", cmd_ftl_sync},
		{"bdev_read", "read bytes from a NAND block device", cmd_bdev_read},
#ifndef NO_HFS
		{"fs_ls", "list files and folders", fs_cmd_ls},
		{"fs_cat", "display a file", fs_cmd_cat},
		{"fs_extract", "extract a file into memory", fs_cmd_extract},
		{"fs_add", "store a file from memory", fs_cmd_add},
#endif
		{"nor_read", "read a block of NOR into RAM", cmd_nor_read},
		{"nor_write", "write RAM into NOR", cmd_nor_write},
		{"nor_erase", "erase a block of NOR", cmd_nor_erase},
		{"iic_read", "read a IIC register", cmd_iic_read},
		{"iic_write", "write a IIC register", cmd_iic_write},
		{"accel", "display accelerometer data", cmd_accel},
#ifndef CONFIG_IPOD
		{"als", "display ambient light sensor data", cmd_als},
		{"als_channel", "set channel to get ALS data from", cmd_als_channel},
		{"als_en", "enable continuous reporting of ALS data", cmd_als_en},
		{"als_dis", "disable continuous reporting of ALS data", cmd_als_dis},
#endif
		{"sdio_status", "display sdio registers", cmd_sdio_status},
		{"sdio_setup", "restart SDIO stuff", cmd_sdio_setup},
		{"wlan_prog_helper", "program wlan fw helper", cmd_wlan_prog_helper},
		{"wlan_prog_real", "program wlan fw", cmd_wlan_prog_real},
#ifndef CONFIG_IPOD
		{"radio_send", "send a command to the baseband", cmd_radio_send},
		{"radio_nvram_list", "list entries in baseband NVRAM", cmd_radio_nvram_list},
		{"radio_register", "register with a cellular network", cmd_radio_register},
		{"radio_call", "make a call", cmd_radio_call},
		{"radio_hangup", "hang up", cmd_radio_hangup},
		{"vibrator_loop", "turn the vibrator on in a loop", cmd_vibrator_loop},
		{"vibrator_once", "vibrate once", cmd_vibrator_once},
		{"vibrator_off", "turn the vibrator off", cmd_vibrator_off},
#endif
		{"images_list", "list the images available on NOR", cmd_images_list},
		{"images_read", "read an image on NOR", cmd_images_read},
		{"pmu_voltage", "get the battery voltage", cmd_pmu_voltage},
		{"pmu_powersupply", "get the power supply type", cmd_pmu_powersupply},
		{"pmu_charge", "turn on and off the power charger", cmd_pmu_charge},
		{"pmu_nvram", "list powernvram registers", cmd_pmu_nvram},
		{"malloc_stats", "display malloc stats", cmd_malloc_stats},
		{"frequency", "display clock frequencies", cmd_frequency},
		{"printenv", "list the environment variables in nvram", cmd_printenv},
		{"setenv", "sets an environment variable", cmd_setenv},
		{"saveenv", "saves the environment variables in nvram", cmd_saveenv},
		{"bgcolor", "fill the framebuffer with a color", cmd_bgcolor},
		{"backlight", "set the backlight level", cmd_backlight},
		{"kernel", "load a Linux kernel", cmd_kernel},
		{"ramdisk", "load a Linux ramdisk", cmd_ramdisk},
		{"rootfs", "specify a file as the Linux rootfs", cmd_rootfs},
		{"boot", "boot a Linux kernel", cmd_boot},
		{"go", "jump to a specified address (interrupts disabled)", cmd_go},
		{"jump", "jump to a specified address (interrupts enabled)", cmd_jump},
		{"version", "display the version string", cmd_version},
		{"time", "display the current time according to the RTC", cmd_time},
		{"wdt", "display the current wdt stats", cmd_wdt},
		{"audiohw_transfers_done", "display how many times the audio buffer has been played", cmd_audiohw_transfers_done},
		{"audiohw_play_pcm", "queue some PCM data for playback", cmd_audiohw_play_pcm},
		{"audiohw_headphone_vol", "set the headphone volume", cmd_audiohw_headphone_vol},
#ifndef CONFIG_IPOD
		{"audiohw_speaker_vol", "set the speaker volume", cmd_audiohw_speaker_vol},
#endif
		{"audiohw_position", "print the playback position", cmd_audiohw_position},
		{"audiohw_pause", "pause playback", cmd_audiohw_pause},
		{"audiohw_resume", "resume playback", cmd_audiohw_resume},
		{"multitouch_setup", "setup the multitouch chip", cmd_multitouch_setup},
		{"help", "list the available commands", cmd_help},
		{NULL, NULL}
	};
