#ifndef PMU_H
#define PMU_H

#include "openiboot.h"

typedef struct PMURegisterData {
	uint8_t reg;
	uint8_t data;
} PMURegisterData;

typedef enum PowerSupplyType {
	PowerSupplyTypeError,
	PowerSupplyTypeBattery,
	PowerSupplyTypeFirewire,
	PowerSupplyTypeUSBHost,
	PowerSupplyTypeUSBBrick500mA,
	PowerSupplyTypeUSBBrick1000mA
} PowerSupplyType;

#define PMU_IBOOTSTATE 0xF
#define PMU_IBOOTDEBUG 0x0
#define PMU_IBOOTSTAGE 0x1
#define PMU_IBOOTERRORCOUNT 0x2
#define PMU_IBOOTERRORSTAGE 0x3

int pmu_setup();
void pmu_poweroff();
void pmu_set_iboot_stage(uint8_t stage);
int pmu_get_gpmem_reg(int reg, uint8_t* out);
int pmu_set_gpmem_reg(int reg, uint8_t data);
int pmu_get_reg(int reg);
int pmu_write_reg(int reg, int data, int verify);
int pmu_write_regs(const PMURegisterData* regs, int num);
int pmu_get_battery_voltage();
PowerSupplyType pmu_get_power_supply();
void pmu_charge_settings(int UseUSB, int SuspendUSB, int StopCharger);

#endif
