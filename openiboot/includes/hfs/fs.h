#ifndef FS_H
#define FS_H

#include "openiboot.h"

typedef struct ExtentListItem {
	uint32_t startBlock;
	uint32_t blockCount;
} ExtentListItem;

typedef struct ExtentList {
	uint32_t numExtents;
	ExtentListItem extents[16];
} ExtentList;

extern int HasFSInit;

int fs_setup();
ExtentList* fs_get_extents(int partition, const char* fileName);
void fs_cmd_ls(int argc, char** argv);
void fs_cmd_cat(int argc, char** argv);
void fs_cmd_extract(int argc, char** argv);

#endif
