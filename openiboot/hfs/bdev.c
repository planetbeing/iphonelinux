#include "openiboot.h"
#include "hfs/common.h"
#include "hfs/bdev.h"
#include "ftl.h"
#include "util.h"

int HasBDevInit = FALSE;

static MBR MBRData;

int bdev_setup() {
	if(HasBDevInit)
		return 0;

	ftl_setup();

	ftl_read(&MBRData, 0, sizeof(MBRData));
	MBRPartitionRecord* record = MBRData.partitions;

	while(record->type != 0) {
		bufferPrintf("bdev: partition type: %x, sectors: %d - %d\r\n", record->type, record->beginLBA, record->beginLBA + record->numSectors);
		record++;
	}

	HasBDevInit = TRUE;

	return 0;
}

static int bdevRead(io_func* io, off_t location, size_t size, void *buffer) {
	MBRPartitionRecord* record = (MBRPartitionRecord*) io->data;
	return ftl_read(buffer, location + record->beginLBA, size);
}

static int bdevWrite(io_func* io, off_t location, size_t size, void *buffer) {
	MBRPartitionRecord* record = (MBRPartitionRecord*) io->data;
	bufferPrintf("bdev: attempt to write %d sectors to partition %d, sector %d!\r\n", size, ((uint32_t)record - (uint32_t)MBRData.partitions)/sizeof(MBRPartitionRecord), location);
	return FALSE;
}

static void bdevClose(io_func* io) {
	free(io);
}

io_func* bdev_open(int partition) {
	io_func* io;

	if(MBRData.partitions[partition].type != 0xAF)
		return NULL;

	io = (io_func*) malloc(sizeof(io_func));
	io->data = &MBRData.partitions[partition];
	io->read = &bdevRead;
	io->write = &bdevWrite;
	io->close = &bdevClose;

	return io;
}

