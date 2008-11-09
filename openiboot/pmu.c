#include "openiboot.h"
#include "pmu.h"
#include "hardware/pmu.h"
#include "i2c.h"
#include "timer.h"
#include "gpio.h"

static uint32_t GPMemCachedPresent = 0;
static uint8_t GPMemCache[PMU_MAXREG + 1];

int pmu_setup() {
	return 0;
}

int pmu_get_gpmem_reg(int bus, int reg, uint8_t* out) {
	if(reg > PMU_MAXREG)
		return -1;

	if((GPMemCachedPresent & (0x1 << reg)) == 0) {
		uint8_t registers[1];

		registers[0] = reg + 0x67;

		if(i2c_rx(bus, PMU_GETADDR, registers, 1, &GPMemCache[reg], 1) != 0) {
			return -1;
		}

		GPMemCachedPresent |= (0x1 << reg);
	}

	*out = GPMemCache[reg];

	return 0;	
}

int pmu_get_reg(int bus, int reg) {
	uint8_t registers[1];
	uint8_t out[1];

	registers[0] = reg;

	i2c_rx(bus, PMU_GETADDR, registers, 1, out, 1);
	return out[0];
}

int pmu_write_reg(int bus, int reg, int data, int verify) {
	uint8_t command[2];

	command[0] = reg;
	command[1] = data;

	i2c_tx(bus, PMU_SETADDR, command, sizeof(command));

	if(!verify)
		return 0;

	uint8_t pmuReg = reg;
	uint8_t buffer = 0;
	i2c_rx(bus, PMU_GETADDR, &pmuReg, 1, &buffer, 1);

	if(buffer == data)
		return 0;
	else
		return -1;
}

int pmu_write_regs(const PMURegisterData* regs, int num) {
	int i;
	for(i = 0; i < num; i++) {
		pmu_write_reg(regs[i].bus, regs[i].reg, regs[i].data, 1);
	}

	return 0;
}

int query_adc(int flags) {
	int bus = 0;
	pmu_write_reg(bus, PMU_ADCC3, 0, FALSE);
	pmu_write_reg(bus, PMU_ADCC3, 0, FALSE);
	udelay(30);
	pmu_write_reg(bus, PMU_ADCC2, 0, FALSE);
	pmu_write_reg(bus, PMU_ADCC1, PMU_ADCC1_ADCSTART | (PMU_ADCC1_ADC_AV_16 << PMU_ADCC1_ADC_AV_SHIFT) | (PMU_ADCC1_ADCINMUX_BATSNS_DIV << PMU_ADCC1_ADCINMUX_SHIFT) | flags, FALSE);
	udelay(30000);
	uint8_t lower = pmu_get_reg(bus, PMU_ADCS3);
	if((lower & 0x80) == 0x80) {
		uint8_t upper = pmu_get_reg(bus, PMU_ADCS1);
		return ((upper << 2) | (lower & 0x3)) * 6000 / 1023;
	} else {
		return -1;
	}
}

int pmu_get_battery_voltage() {
	return query_adc(0);
}

static PowerSupplyType identify_usb_charger() {
	int dn;
	int dp;

	gpio_pin_output(PMU_GPIO_CHARGER_IDENTIFY_DN, 1);
	dn = query_adc(PMU_ADCC1_ADCINMUX_ADCIN2_DIV << PMU_ADCC1_ADCINMUX_SHIFT);
	if(dn < 0)
		dn = 0;
	gpio_pin_output(PMU_GPIO_CHARGER_IDENTIFY_DN, 0);

	gpio_pin_output(PMU_GPIO_CHARGER_IDENTIFY_DP, 1);
	dp = query_adc(PMU_ADCC1_ADCINMUX_ADCIN2_DIV << PMU_ADCC1_ADCINMUX_SHIFT);
	if(dp < 0)
		dp = 0;
	gpio_pin_output(PMU_GPIO_CHARGER_IDENTIFY_DP, 0);

	if(dn < 99 || dp < 99) {
		return PowerSupplyTypeUSBHost;
	}

	int x = (dn * 1000) / dp;
	if((x - 1291) <= 214) {
		return PowerSupplyTypeUSBBrick1000mA;
	}

	if((x - 901) <= 219 && dn <= 367 ) {
		return PowerSupplyTypeUSBBrick500mA;
	} else {
		return PowerSupplyTypeUSBHost;
	}
}

PowerSupplyType pmu_get_power_supply() {
	int mbcs1 = pmu_get_reg(0, PMU_MBCS1);

	if(mbcs1 & PMU_MBCS1_ADAPTPRES)
		return PowerSupplyTypeFirewire;

	if(mbcs1 & PMU_MBCS1_USBOK)
		return identify_usb_charger();
	else
		return PowerSupplyTypeBattery;


}

void pmu_charge_settings(int UseUSB, int SuspendUSB, int StopCharger) {
	PowerSupplyType type = pmu_get_power_supply();

	if(type != PowerSupplyTypeUSBHost)	// No need to suspend USB, since we're not plugged into a USB host
		SuspendUSB = FALSE;

	if(SuspendUSB)
		gpio_pin_output(PMU_GPIO_CHARGER_USB_SUSPEND, 1);
	else
		gpio_pin_output(PMU_GPIO_CHARGER_USB_SUSPEND, 0);

	if(StopCharger) {
		gpio_pin_output(PMU_GPIO_CHARGER_SUSPEND, 1);
		gpio_pin_output(PMU_GPIO_CHARGER_SHUTDOWN, 1);
	} else {
		gpio_pin_output(PMU_GPIO_CHARGER_SUSPEND, 0);
		gpio_pin_output(PMU_GPIO_CHARGER_SHUTDOWN, 0);
	}

	if(type == PowerSupplyTypeUSBBrick500mA || type == PowerSupplyTypeUSBBrick1000mA || (type == PowerSupplyTypeUSBHost && UseUSB))
		gpio_pin_output(PMU_GPIO_CHARGER_USB_500_1000, 1);
	else
		gpio_pin_output(PMU_GPIO_CHARGER_USB_500_1000, 0);

	if(type == PowerSupplyTypeUSBBrick1000mA)
		gpio_pin_output(PMU_GPIO_CHARGER_USB_1000, 1);
	else
		gpio_pin_output(PMU_GPIO_CHARGER_USB_1000, 0);
}

