#ifndef ACTIONS_H
#define ACTIONS_H

#include "openiboot.h"

void chainload(uint32_t address);
void set_kernel(void* location, int size);
void set_ramdisk(void* location, int size);
void set_rootfs(int partition, const char* fileName);
void boot_linux(const char* args);

#ifndef NO_HFS
void boot_linux_from_files();
#endif

#endif
