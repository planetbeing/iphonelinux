#include "openiboot.h"
#include "hfs/common.h"
#include "hfs/bdev.h"
#include "hfs/fs.h"
#include "hfs/hfsplus.h"
#include "util.h"
#include "ftl.h"
#include "nand.h"

int HasFSInit = FALSE;

void writeToHFSFile(HFSPlusCatalogFile* file, uint8_t* buffer, size_t bytesLeft, Volume* volume) {
	io_func* io;

	io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
	if(io == NULL) {
		hfs_panic("error opening file");
		return;
	}
	allocate((RawFile*)io->data, bytesLeft);
	
	if(!WRITE(io, 0, (size_t)bytesLeft, buffer)) {
		hfs_panic("error writing");
	}

	CLOSE(io);
}

int add_hfs(Volume* volume, uint8_t* buffer, size_t size, const char* outFileName) {
	HFSPlusCatalogRecord* record;
	int ret;
	
	record = getRecordFromPath(outFileName, volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			writeToHFSFile((HFSPlusCatalogFile*)record, buffer, size, volume);
			ret = TRUE;
		} else {
			ret = FALSE;
		}
	} else {
		if(newFile(outFileName, volume)) {
			record = getRecordFromPath(outFileName, volume, NULL, NULL);
			writeToHFSFile((HFSPlusCatalogFile*)record, buffer, size, volume);
			ret = TRUE;
		} else {
			ret = FALSE;
		}
	}
	
	if(record != NULL) {
		free(record);
	}
	
	return ret;
}

void displayFolder(HFSCatalogNodeID folderID, Volume* volume) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	HFSPlusCatalogFolder* folder;
	HFSPlusCatalogFile* file;
	
	theList = list = getFolderContents(folderID, volume);
	
	while(list != NULL) {
		if(list->record->recordType == kHFSPlusFolderRecord) {
			folder = (HFSPlusCatalogFolder*)list->record;
			bufferPrintf("%06o ", folder->permissions.fileMode);
			bufferPrintf("%3d ", folder->permissions.ownerID);
			bufferPrintf("%3d ", folder->permissions.groupID);
			bufferPrintf("%12d ", folder->valence);
		} else if(list->record->recordType == kHFSPlusFileRecord) {
			file = (HFSPlusCatalogFile*)list->record;
			bufferPrintf("%06o ", file->permissions.fileMode);
			bufferPrintf("%3d ", file->permissions.ownerID);
			bufferPrintf("%3d ", file->permissions.groupID);
			bufferPrintf("%12Ld ", file->dataFork.logicalSize);
		}
		
		bufferPrintf("                 ");

		printUnicode(&list->name);
		bufferPrintf("\r\n");
		
		list = list->next;
	}
	
	releaseCatalogRecordList(theList);
}

void displayFileLSLine(HFSPlusCatalogFile* file, const char* name) {
	bufferPrintf("%06o ", file->permissions.fileMode);
	bufferPrintf("%3d ", file->permissions.ownerID);
	bufferPrintf("%3d ", file->permissions.groupID);
	bufferPrintf("%12Ld ", file->dataFork.logicalSize);
	bufferPrintf("                 ");
	bufferPrintf("%s\r\n", name);
}

uint32_t readHFSFile(HFSPlusCatalogFile* file, uint8_t** buffer, Volume* volume) {
	io_func* io;
	size_t bytesLeft;

	io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
	if(io == NULL) {
		hfs_panic("error opening file");
		free(buffer);
		return 0;
	}

	bytesLeft = file->dataFork.logicalSize;
	*buffer = malloc(bytesLeft);
	if(!(*buffer)) {
		hfs_panic("error allocating memory");
		return 0;
	}
	
	if(!READ(io, 0, bytesLeft, *buffer)) {
		hfs_panic("error reading");
	}

	CLOSE(io);

	return file->dataFork.logicalSize;
}

void hfs_ls(Volume* volume, const char* path) {
	HFSPlusCatalogRecord* record;
	char* name;

	record = getRecordFromPath(path, volume, &name, NULL);
	
	bufferPrintf("%s: \r\n", name);
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			displayFolder(((HFSPlusCatalogFolder*)record)->folderID, volume);  
		else
			displayFileLSLine((HFSPlusCatalogFile*)record, name);
	} else {
		bufferPrintf("No such file or directory\r\n");
	}

	bufferPrintf("Total filesystem size: %Ld, free: %Ld\r\n", ((uint64_t)volume->volumeHeader->totalBlocks - volume->volumeHeader->freeBlocks) * volume->volumeHeader->blockSize, (uint64_t)volume->volumeHeader->freeBlocks * volume->volumeHeader->blockSize);
	
	free(record);
}

void fs_cmd_ls(int argc, char** argv) {
	Volume* volume;
	io_func* io;

	if(argc < 2) {
		bufferPrintf("usage: %s <partition> <directory>\r\n", argv[0]);
		return;
	}

	io = bdev_open(parseNumber(argv[1]));
	if(io == NULL) {
		bufferPrintf("fs: cannot read partition!\r\n");
		return;
	}

	volume = openVolume(io);
	if(volume == NULL) {
		bufferPrintf("fs: cannot openHFS volume!\r\n");
		return;
	}

	if(argc > 2)
		hfs_ls(volume, argv[2]);
	else
		hfs_ls(volume, "/");

	closeVolume(volume);
	CLOSE(io);
}

void fs_cmd_cat(int argc, char** argv) {
	Volume* volume;
	io_func* io;

	if(argc < 3) {
		bufferPrintf("usage: %s <partition> <file>\r\n", argv[0]);
		return;
	}

	io = bdev_open(parseNumber(argv[1]));
	if(io == NULL) {
		bufferPrintf("fs: cannot read partition!\r\n");
		return;
	}

	volume = openVolume(io);
	if(volume == NULL) {
		bufferPrintf("fs: cannot openHFS volume!\r\n");
		return;
	}

	HFSPlusCatalogRecord* record;

	record = getRecordFromPath(argv[2], volume, NULL, NULL);

	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			uint8_t* buffer;
			uint32_t size = readHFSFile((HFSPlusCatalogFile*)record, &buffer, volume);
			buffer = realloc(buffer, size + 1);
			buffer[size] = '\0';
			bufferPrintf("%s\r\n", buffer);
			free(buffer);
		} else {
			bufferPrintf("Not a file, record type: %x\r\n", record->recordType);
		}
	} else {
		bufferPrintf("No such file or directory\r\n");
	}
	
	free(record);

	closeVolume(volume);
	CLOSE(io);
}

int fs_extract(int partition, const char* file, void* location) {
	Volume* volume;
	io_func* io;
	int ret;

	io = bdev_open(partition);
	if(io == NULL) {
		bufferPrintf("fs: cannot read partition!\r\n");
		return -1;
	}

	volume = openVolume(io);
	if(volume == NULL) {
		return -1;
	}

	HFSPlusCatalogRecord* record;

	record = getRecordFromPath(file, volume, NULL, NULL);

	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			uint8_t* buffer;
			uint32_t size = readHFSFile((HFSPlusCatalogFile*)record, &buffer, volume);
			memcpy(location, buffer, size);
			free(buffer);
			ret = size;
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	free(record);

	closeVolume(volume);
	CLOSE(io);

	return ret;
}

void fs_cmd_extract(int argc, char** argv) {
	Volume* volume;
	io_func* io;

	if(argc < 4) {
		bufferPrintf("usage: %s <partition> <file> <location>\r\n", argv[0]);
		return;
	}

	io = bdev_open(parseNumber(argv[1]));
	if(io == NULL) {
		bufferPrintf("fs: cannot read partition!\r\n");
		return;
	}

	volume = openVolume(io);
	if(volume == NULL) {
		bufferPrintf("fs: cannot openHFS volume!\r\n");
		return;
	}

	HFSPlusCatalogRecord* record;

	record = getRecordFromPath(argv[2], volume, NULL, NULL);

	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			uint8_t* buffer;
			uint32_t size = readHFSFile((HFSPlusCatalogFile*)record, &buffer, volume);
			uint32_t address = parseNumber(argv[3]);
			memcpy((uint8_t*)address, buffer, size);
			free(buffer);
			bufferPrintf("%d bytes of %s extracted to 0x%x\r\n", size, argv[2], address);
		} else {
			bufferPrintf("Not a file, record type: %x\r\n", record->recordType);
		}
	} else {
		bufferPrintf("No such file or directory\r\n");
	}
	
	free(record);

	closeVolume(volume);
	CLOSE(io);
}

void fs_cmd_add(int argc, char** argv) {
	Volume* volume;
	io_func* io;

	if(argc < 5) {
		bufferPrintf("usage: %s <partition> <file> <location> <size>\r\n", argv[0]);
		return;
	}

	io = bdev_open(parseNumber(argv[1]));
	if(io == NULL) {
		bufferPrintf("fs: cannot read partition!\r\n");
		return;
	}

	volume = openVolume(io);
	if(volume == NULL) {
		bufferPrintf("fs: cannot openHFS volume!\r\n");
		return;
	}

	uint32_t address = parseNumber(argv[3]);
	uint32_t size = parseNumber(argv[4]);

	if(add_hfs(volume, (uint8_t*) address, size, argv[2]))
	{
		bufferPrintf("%d bytes of 0x%x stored in %s\r\n", size, address, argv[2]);
	}
	else
	{
		bufferPrintf("add_hfs failed for %s!\r\n", argv[2]);
	}

	closeVolume(volume);
	CLOSE(io);

	if(!ftl_sync())
	{
		bufferPrintf("FTL sync error!\r\n");
	}
}

ExtentList* fs_get_extents(int partition, const char* fileName) {
	Volume* volume;
	io_func* io;
	unsigned int partitionStart;
	unsigned int physBlockSize;
	ExtentList* list = NULL;

	io = bdev_open(partition);
	if(io == NULL) {
		return NULL;
	}

	physBlockSize = (nand_get_geometry())->bytesPerPage;
	partitionStart = bdev_get_start(partition);

	volume = openVolume(io);
	if(volume == NULL) {
		goto out;
	}

	HFSPlusCatalogRecord* record;

	record = getRecordFromPath(fileName, volume, NULL, NULL);

	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			io_func* fileIO;
			HFSPlusCatalogFile* file = (HFSPlusCatalogFile*) record;
			unsigned int allocationBlockSize = volume->volumeHeader->blockSize;
			int numExtents = 0;
			Extent* extent;
			int i;

			fileIO = openRawFile(file->fileID, &file->dataFork, record, volume);
			if(!fileIO)
				goto out_free;

			extent = ((RawFile*)fileIO->data)->extents;
			while(extent != NULL)
			{
				numExtents++;
				extent = extent->next;
			}

			list = (ExtentList*) malloc(sizeof(ExtentList));
			list->numExtents = numExtents;

			extent = ((RawFile*)fileIO->data)->extents;
			for(i = 0; i < list->numExtents; i++)
			{
				list->extents[i].startBlock = partitionStart + (extent->startBlock * (allocationBlockSize / physBlockSize));
				list->extents[i].blockCount = extent->blockCount * (allocationBlockSize / physBlockSize);
				extent = extent->next;
			}

			CLOSE(fileIO);
		} else {
			goto out_free;
		}
	} else {
		goto out_close;
	}

out_free:
	free(record);

out_close:
	closeVolume(volume);

out:
	CLOSE(io);

	return list;
}

int fs_setup() {
	if(HasFSInit)
		return 0;

	bdev_setup();

	HasFSInit = TRUE;

	return 0;
}

