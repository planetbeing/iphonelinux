#ifndef FS_H
#define FS_H

#include "openiboot.h"
#include "hfs/hfsplus.h"

typedef struct ExtentListItem {
	uint32_t startBlock;
	uint32_t blockCount;
} ExtentListItem;

typedef struct ExtentList {
	uint32_t numExtents;
	ExtentListItem extents[16];
} ExtentList;

extern int HasFSInit;

uint32_t readHFSFile(HFSPlusCatalogFile* file, uint8_t** buffer, Volume* volume);

int fs_setup();
ExtentList* fs_get_extents(int partition, const char* fileName);
void fs_cmd_ls(int argc, char** argv);
void fs_cmd_cat(int argc, char** argv);
void fs_cmd_extract(int argc, char** argv);
void fs_cmd_add(int argc, char** argv);
int fs_extract(int partition, const char* file, void* location);

#endif
