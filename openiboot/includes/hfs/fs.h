#ifndef FS_H
#define FS_H

#include "openiboot.h"

extern int HasFSInit;

int fs_setup();
void fs_cmd_ls(int argc, char** argv);
void fs_cmd_cat(int argc, char** argv);
void fs_cmd_extract(int argc, char** argv);

#endif
