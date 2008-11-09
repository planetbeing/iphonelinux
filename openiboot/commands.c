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

void cmd_reboot(int argc, char** argv) {
	Reboot();
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

void cmd_go(int argc, char** argv) {
	if(argc < 2) {
		bufferPrintf("Usage: %s <address>\r\n", argv[0]);
		return;
	}

	uint32_t address = parseNumber(argv[1]);
	bufferPrintf("Jumping to 0x%x (interrupts disabled)\r\n", address);

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

void cmd_bgcolor(int argc, char** argv) {
	if(argc < 4) {
		bufferPrintf("Usage: %s <red> <green> <blue>\r\n", argv[0]);
		return;
	}

	uint8_t red = parseNumber(argv[1]);
	uint8_t blue = parseNumber(argv[2]);
	uint8_t green = parseNumber(argv[3]);

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
	nvram_save();
	bufferPrintf("Environment saved\r\n");
}

void cmd_install(int argc, char** argv) {
	images_install(&_start, (uint32_t)&OpenIBootEnd - (uint32_t)&_start);
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
	bufferPrintf("dma_request: %d\r\n", dma_request(DMA_MEMORY, 4, 8, DMA_MEMORY, 4, 8, &controller, &channel));
	bufferPrintf("dma_perform(controller: %d, channel %d): %d\r\n", controller, channel, dma_perform(source, dest, size, FALSE, &controller, &channel));
	bufferPrintf("dma_finish(controller: %d, channel %d): %d\r\n", controller, channel, dma_finish(controller, channel, 500));
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
		address += Data->bytesPerSpare;
	}

	bufferPrintf("done!\r\n");
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
		{"reboot", "reboot the device", cmd_reboot},
		{"echo", "echo back a line", cmd_echo},
		{"clear", "clears the screen", cmd_clear},
		{"text", "turns text display on or off", cmd_text},
		{"md", "display a block of memory as 32-bit integers", cmd_md},
		{"mw", "write a 32-bit dword into a memory address", cmd_mw},
		{"mwb", "write a byte into a memory address", cmd_mwb},
		{"hexdump", "display a block of memory like 'hexdump -C'", cmd_hexdump},
		{"cat", "dumps a block of memory", cmd_cat},
		{"gpio_pinstate", "get the state of a GPIO pin", cmd_gpio_pinstate},
		{"dma", "perform a DMA transfer", cmd_dma},
		{"nand_read", "read a page of NAND into RAM", cmd_nand_read},
		{"nand_read_spare", "read a page of NAND's spare into RAM", cmd_nand_read_spare},
		{"vfl_read", "read a page of VFL into RAM", cmd_vfl_read},
		{"nor_read", "read a block of NOR into RAM", cmd_nor_read},
		{"nor_write", "write RAM into NOR", cmd_nor_write},
		{"nor_erase", "erase a block of NOR", cmd_nor_erase},
		{"images_list", "list the images available on NOR", cmd_images_list},
		{"images_read", "read an image on NOR", cmd_images_read},
		{"pmu_voltage", "get the battery voltage", cmd_pmu_voltage},
		{"pmu_powersupply", "get the power supply type", cmd_pmu_powersupply},
		{"pmu_charge", "turn on and off the power charger", cmd_pmu_charge},
		{"printenv", "list the environment variables in nvram", cmd_printenv},
		{"setenv", "sets an environment variable", cmd_setenv},
		{"saveenv", "saves the environment variables in nvram", cmd_saveenv},
		{"bgcolor", "fill the framebuffer with a color", cmd_bgcolor},
		{"backlight", "set the backlight level", cmd_backlight},
		{"go", "jump to a specified address (interrupts disabled)", cmd_go},
		{"jump", "jump to a specified address (interrupts enabled)", cmd_jump},
		{"help", "list the available commands", cmd_help},
		{NULL, NULL}
	};
