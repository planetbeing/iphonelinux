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

	EnterCriticalSection();
	arm_disable_caches();
	mmu_disable();
	CallArm(address);
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

void cmd_help(int argc, char** argv) {
	OPIBCommand* curCommand = CommandList;
	while(curCommand->name != NULL) {
		bufferPrintf("%-20s%s\r\n", curCommand->name, curCommand->description);
		curCommand++;
	}
}

OPIBCommand CommandList[] = 
	{
		{"reboot", "reboot the device", cmd_reboot},
		{"md", "display a block of memory as 32-bit integers", cmd_md},
		{"mw", "write a 32-bit dword into a memory address", cmd_mw},
		{"mwb", "write a byte into a memory address", cmd_mwb},
		{"hexdump", "display a block of memory like 'hexdump -C'", cmd_hexdump},
		{"cat", "dumps a block of memory", cmd_cat},
		{"nor_read", "read a block of NOR into RAM", cmd_nor_read},
		{"nor_write", "write RAM into NOR", cmd_nor_write},
		{"nor_erase", "erase a block of NOR", cmd_nor_erase},
		{"images_list", "list the images available on NOR", cmd_images_list},
		{"images_read", "read an image on NOR", cmd_images_read},
		{"bgcolor", "fill the framebuffer with a color", cmd_bgcolor},
		{"go", "jump to a specified address (interrupts disabled)", cmd_go},
		{"jump", "jump to a specified address (interrupts enabled)", cmd_jump},
		{"help", "list the available commands", cmd_help},
		{NULL, NULL}
	};
