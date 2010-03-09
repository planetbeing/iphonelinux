#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct NANDDeviceType {
	uint32_t id;
	uint16_t blocksPerBank;
	uint16_t pagesPerBlock;
	uint16_t sectorsPerPage;
	uint16_t bytesPerSpare;
	uint8_t WPPulseTime;
	uint8_t WEHighHoldTime;
	uint8_t NANDSetting3;
	uint8_t NANDSetting4;
	uint32_t userSubBlksTotal;
	uint32_t ecc1;
	uint32_t ecc2;
} NANDDeviceType;

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		printf("Spit out a struct openiboot can use to configure NAND parameters from the similar table in iBoot.\n\n");
		printf("Usage: %s <decrypted iboot file> <offset>\n", argv[0]);
		return 0;
	}

	printf("static const NANDDeviceType SupportedDevices[] = {\n");
	uint32_t offset = strtol(argv[2], NULL, 0);
	FILE* f = fopen(argv[1], "rb");

	fseek(f, offset, SEEK_SET);
	while(1)
	{
		NANDDeviceType entry;
		fread(&entry, 1, sizeof(entry), f);
		if(entry.id == 0)
			break;

		printf("\t{0x%X, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d},\n",
				entry.id, entry.blocksPerBank, entry.pagesPerBlock, entry.sectorsPerPage, entry.bytesPerSpare,
				entry.WPPulseTime, entry.WEHighHoldTime, entry.NANDSetting3, entry.NANDSetting4,
				entry.userSubBlksTotal, entry.ecc1, entry.ecc2);
	}

	printf("\t{0}\n};\n");

	fclose(f);
}
