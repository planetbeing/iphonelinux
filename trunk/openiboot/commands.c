#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "commands.h"
#include "util.h"
#include "nor.h"

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

void cmd_getfile(int argc, char** argv) {
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
	bufferPrintf("Reading 0x%x - 0x%x to 0x%x...\r\n", address, address + offset, len);
	nor_read((void*)address, offset, len);
	bufferPrintf("Done.\r\n");
}

void cmd_nor_write(int argc, char** argv) {
}

void cmd_nor_erase(int argc, char** argv) {
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
		{"hexdump", "display a block of memory like 'hexdump -C'", cmd_hexdump},
		{"getfile", "dumps a block of memory", cmd_getfile},
		{"nor_read", "read a block of NOR into RAM", cmd_nor_read},
		{"nor_write", "write RAM into NOR", cmd_nor_write},
		{"nor_erase", "erase a block of NOR", cmd_nor_erase},
		{"help", "list the available commands", cmd_help},
		{NULL, NULL}
	};
