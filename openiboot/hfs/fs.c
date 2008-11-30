#include "openiboot.h"
#include "hfs/common.h"
#include "hfs/bdev.h"
#include "hfs/fs.h"
#include "hfs/hfsplus.h"
#include "util.h"

int HasFSInit = FALSE;

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

int fs_setup() {
	if(HasFSInit)
		return 0;

	bdev_setup();

	HasFSInit = TRUE;

	return 0;
}

