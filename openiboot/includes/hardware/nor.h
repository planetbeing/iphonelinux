#ifndef HW_NOR_H
#define HW_NOR_H

#include "hardware/s5l8900.h"

// Device
//#define NOR 0x54000000
#define NOR 0x24000000

// Registers
#define COMMAND 0xAAAA
#define LOCK 0x5554
#define VENDOR 0
#define DEVICE 2

// Values
#define DATA_MODE 0xFFFF
#define COMMAND_UNLOCK 0xAAAA
#define COMMAND_LOCK 0xF0F0
#define COMMAND_IDENTIFY 0x9090
#define COMMAND_WRITE 0xA0A0
#define COMMAND_ERASE 0x8080
#define ERASE_DATA 0x3030
#define LOCK_UNLOCK 0x5555

#define NOR_SPI_READ 0x03
#define NOR_SPI_WREN 0x06
#define NOR_SPI_WRDI 0x04
#define NOR_SPI_PRGM 0x02
#define NOR_SPI_RDSR 0x05
#define NOR_SPI_WRSR 0x01
#define NOR_SPI_EWSR 0x50
#define NOR_SPI_AIPG 0xAD
#define NOR_SPI_ERSE_4KB 0x20
#define NOR_SPI_JEDECID 0x9F

#define NOR_SPI_SR_BUSY 0
#define NOR_SPI_SR_WEL 1
#define NOR_SPI_SR_BP0 2
#define NOR_SPI_SR_BP1 3
#define NOR_SPI_SR_BP2 4
#define NOR_SPI_SR_BP3 5
#define NOR_SPI_SR_AAI 6
#define NOR_SPI_SR_BPL 7

#define NOR_T_BP 10000
#define NOR_T_SE 25000

#define NOR_CLOCKGATE 0x1E

#endif

