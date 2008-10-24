#ifndef NOR_H
#define NOR_H

#include "openiboot.h"

int nor_setup();

typedef struct NorInfo {

} NorInfo;

void nor_write_word(uint32_t offset, uint16_t data);
uint16_t nor_read_word(uint32_t offset);
void nor_erase_sector(uint32_t offset);

void nor_read(void* buffer, int offset, int len);
void nor_write(void* buffer, int offset, int len);

int getNORSectorSize();

#endif
