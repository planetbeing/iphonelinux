#include "openiboot.h"
#include "syscfg.h"
#include "nor.h"
#include "util.h"

#define SCFG_MAGIC 0x53436667
#define CNTB_MAGIC 0x434e5442
#define SCFG_LOCATION 0x4000

typedef struct SCfgHeader
{
	uint32_t magic;
	uint32_t bytes_used;
	uint32_t bytes_total;
	uint32_t version;
	uint32_t unknown;
	uint32_t entries;
} SCfgHeader;

typedef struct SCfgEntry
{
	uint32_t magic;
	union {
		uint8_t data[16];
		struct {
			uint32_t type;
			uint32_t size;
			uint32_t offset;
		} cntb;
	};
} SCfgEntry;

typedef struct OIBSyscfgEntry
{
	int type;
	uint32_t size;
	uint8_t* data;
} OIBSyscfgEntry;

static SCfgHeader header;
static OIBSyscfgEntry* entries;

int syscfg_setup()
{
	int i;
	SCfgEntry curEntry;
	uint32_t cursor;

	nor_read(&header, SCFG_LOCATION, sizeof(header));
	if(header.magic != SCFG_MAGIC)
	{
		bufferPrintf("syscfg: cannot find readable syscfg partition!\r\n");
		return -1;
	}

	bufferPrintf("syscfg: found version 0x%08x with %d entries using %d of %d bytes\r\n", header.version, header.entries, header.bytes_used, header.bytes_total);

	entries = (OIBSyscfgEntry*) malloc(sizeof(OIBSyscfgEntry) * header.entries);

	cursor = SCFG_LOCATION + sizeof(header);
	for(i = 0; i < header.entries; ++i)
	{
		nor_read(&curEntry, cursor, sizeof(curEntry));

		if(curEntry.magic != CNTB_MAGIC)
		{
			entries[i].type = curEntry.magic;
			entries[i].size = 16;
			entries[i].data = (uint8_t*) malloc(16);
			memcpy(entries[i].data, curEntry.data, 16);
		} else
		{
			entries[i].type = curEntry.cntb.type;
			entries[i].size = curEntry.cntb.size;
			entries[i].data = (uint8_t*) malloc(curEntry.cntb.size);
			nor_read(entries[i].data, SCFG_LOCATION + curEntry.cntb.offset, curEntry.cntb.size);
		}

		cursor += sizeof(curEntry);
	}

	return 0;
}

uint8_t* syscfg_get_entry(uint32_t type, int* size)
{
	int i;

	for(i = 0; i < header.entries; ++i)
	{
		if(entries[i].type == type)
		{
			*size = entries[i].size;
			return entries[i].data;
		}
	}

	return NULL;
}
