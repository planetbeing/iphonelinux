#include "openiboot.h"
#include "pmu.h"
#include "hardware/pmu.h"
#include "i2c.h"
#include "timer.h"
#include "gpio.h"
#include "lcd.h"

static uint32_t GPMemCachedPresent = 0;
static uint8_t GPMemCache[PMU_MAXREG + 1];

int pmu_setup() {
	return 0;
}

static void pmu_write_oocshdwn(int data) {
	uint8_t registers[1];
	uint8_t discardData[5];
	uint8_t poweroffData[] = {7, 0xAA, 0xFC, 0x0, 0x0, 0x0};
	registers[0] = 2;
	i2c_rx(PMU_I2C_BUS, PMU_GETADDR, registers, sizeof(registers), discardData, sizeof(data));
	i2c_tx(PMU_I2C_BUS, PMU_SETADDR, poweroffData, sizeof(poweroffData));
	pmu_write_reg(PMU_OOCSHDWN, data, FALSE);
	while(TRUE) {
		udelay(100000);
	}
}

void pmu_poweroff() {
	lcd_shutdown();
	pmu_write_oocshdwn(PMU_OOCSHDWN_GOSTBY);
}

void pmu_set_iboot_stage(uint8_t stage) {
	int8_t state;
	pmu_get_gpmem_reg(PMU_IBOOTSTATE, (uint8_t*) &state);

	if(state >= 0) {
		pmu_set_gpmem_reg(PMU_IBOOTSTAGE, stage);
	}
}

int pmu_get_gpmem_reg(int reg, uint8_t* out) {
	if(reg > PMU_MAXREG)
		return -1;

	if((GPMemCachedPresent & (0x1 << reg)) == 0) {
		uint8_t registers[1];

		registers[0] = reg + 0x67;

		if(i2c_rx(PMU_I2C_BUS, PMU_GETADDR, registers, 1, &GPMemCache[reg], 1) != 0) {
			return -1;
		}

		GPMemCachedPresent |= (0x1 << reg);
	}

	*out = GPMemCache[reg];

	return 0;	
}

int pmu_set_gpmem_reg(int reg, uint8_t data) {
	if(pmu_write_reg(reg + 0x67, data, TRUE) == 0) {
		GPMemCache[reg] = data;
		GPMemCachedPresent |= (0x1 << reg);
		return 0;
	}

	return -1;
}

int pmu_get_reg(int reg) {
	uint8_t registers[1];
	uint8_t out[1];

	registers[0] = reg;

	i2c_rx(PMU_I2C_BUS, PMU_GETADDR, registers, 1, out, 1);
	return out[0];
}

int pmu_get_regs(int reg, uint8_t* out, int count) {
	uint8_t registers[1];

	registers[0] = reg;

	return i2c_rx(PMU_I2C_BUS, PMU_GETADDR, registers, 1, out, count);
}

int pmu_write_reg(int reg, int data, int verify) {
	uint8_t command[2];

	command[0] = reg;
	command[1] = data;

	i2c_tx(PMU_I2C_BUS, PMU_SETADDR, command, sizeof(command));

	if(!verify)
		return 0;

	uint8_t pmuReg = reg;
	uint8_t buffer = 0;
	i2c_rx(PMU_I2C_BUS, PMU_GETADDR, &pmuReg, 1, &buffer, 1);

	if(buffer == data)
		return 0;
	else
		return -1;
}

int pmu_write_regs(const PMURegisterData* regs, int num) {
	int i;
	for(i = 0; i < num; i++) {
		pmu_write_reg(regs[i].reg, regs[i].data, 1);
	}

	return 0;
}

int query_adc(int flags) {
	pmu_write_reg(PMU_ADCC3, 0, FALSE);
	pmu_write_reg(PMU_ADCC3, 0, FALSE);
	udelay(30);
	pmu_write_reg(PMU_ADCC2, 0, FALSE);
	pmu_write_reg(PMU_ADCC1, PMU_ADCC1_ADCSTART | (PMU_ADCC1_ADC_AV_16 << PMU_ADCC1_ADC_AV_SHIFT) | (PMU_ADCC1_ADCINMUX_BATSNS_DIV << PMU_ADCC1_ADCINMUX_SHIFT) | flags, FALSE);
	udelay(30000);
	uint8_t lower = pmu_get_reg(PMU_ADCS3);
	if((lower & 0x80) == 0x80) {
		uint8_t upper = pmu_get_reg(PMU_ADCS1);
		return ((upper << 2) | (lower & 0x3)) * 6000 / 1023;
	} else {
		return -1;
	}
}

int pmu_get_battery_voltage() {
	return query_adc(0);
}

static int bcd_to_int(int bcd) {
	return (bcd & 0xF) + (((bcd >> 4) & 0xF) * 10);
}

const char* get_dayofweek_str(int day) {
	switch(day) {
		case 0:
			return "Sunday";
		case 1:
			return "Monday";
		case 2:
			return "Tuesday";
		case 3:
			return "Wednesday";
		case 4:
			return "Thursday";
		case 5:
			return "Friday";
		case 6:
			return "Saturday";
	}

	return NULL;
}

static const int days_in_months_leap_year[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
static const int days_in_months[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
uint64_t pmu_get_epoch()
{
	int i;
	int days;
	int years;
	uint64_t secs;
	int32_t offset;
	uint8_t rtc_data[PMU_RTCYR - PMU_RTCSC + 1];
	uint8_t rtc_data2[PMU_RTCYR - PMU_RTCSC + 1];

	do
	{
		pmu_get_regs(PMU_RTCSC, rtc_data, PMU_RTCYR - PMU_RTCSC + 1);
		pmu_get_regs(PMU_RTCSC, rtc_data2, PMU_RTCYR - PMU_RTCSC + 1);
	} while(rtc_data2[0] != rtc_data[0]);

	secs = bcd_to_int(rtc_data[0] & PMU_RTCSC_MASK);

	secs += 60 * bcd_to_int(rtc_data[PMU_RTCMN - PMU_RTCSC] & PMU_RTCMN_MASK);
	secs += 3600 * bcd_to_int(rtc_data[PMU_RTCHR - PMU_RTCSC] & PMU_RTCHR_MASK);

	days = bcd_to_int(rtc_data[PMU_RTCDT - PMU_RTCSC] & PMU_RTCDT_MASK) - 1;

	years = 2000 + bcd_to_int(rtc_data[PMU_RTCYR - PMU_RTCSC] & PMU_RTCYR_MASK);
	for(i = 1970; i < years; ++i)
	{
		if((i & 0x3) != 0)
			days += 365;	// non-leap year
		else
			days += 366;	// leap year
	}
	
	if((years & 0x3) != 0)
		days += days_in_months[(rtc_data[PMU_RTCMT - PMU_RTCSC] & PMU_RTCMT_MASK) - 1];
	else
		days += days_in_months_leap_year[(rtc_data[PMU_RTCMT - PMU_RTCSC] & PMU_RTCMT_MASK) - 1];

	secs += ((uint64_t)days) * 86400;

	pmu_get_regs(0x6B, (uint8_t*) &offset, sizeof(offset));

	secs += offset;
	return secs;
}

void epoch_to_date(uint64_t epoch, int* year, int* month, int* day, int* day_of_week, int* hour, int* minute, int* second)
{
	int i;
	int dec = 0;
	int days_since_1970 = 0;
	const int* months_to_days;

	i = 1970;	
	while(epoch >= dec)
	{
		epoch -= dec;
		if((i & 0x3) != 0)
		{
			dec = 365 * 86400;
			if(epoch >= dec) days_since_1970 += 365;
		} else
		{
			dec = 366 * 86400;
			if(epoch >= dec) days_since_1970 += 366;
		}
		++i;
	}

	*year = i - 1;

	if(((i - 1) & 0x3) != 0)
		months_to_days = days_in_months;
	else
		months_to_days = days_in_months_leap_year;

	for(i = 0; i < 12; ++i)
	{
		dec = months_to_days[i] * 86400;
		if(epoch < dec)
		{
			days_since_1970 += months_to_days[i - 1];
			epoch -= months_to_days[i - 1] * 86400;
			*month = i;
			break;
		}
	}

	for(i = 0; i < 31; ++i)
	{
		if(epoch < 86400)
		{
			*day = i + 1;
			break;
		}
		epoch -= 86400;
	}

	days_since_1970 += i;

	*day_of_week = (days_since_1970 + 4) % 7;

	for(i = 0; i < 24; ++i)
	{
		if(epoch < 3600)
		{
			*hour = i;
			break;
		}
		epoch -= 3600;
	}

	for(i = 0; i < 60; ++i)
	{
		if(epoch < 60)
		{
			*minute = i;
			break;
		}
		epoch -= 60;
	}

	*second = epoch;
}

void pmu_date(int* year, int* month, int* day, int* day_of_week, int* hour, int* minute, int* second)
{
	epoch_to_date(pmu_get_epoch(), year, month, day, day_of_week, hour, minute, second);
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
	int mbcs1 = pmu_get_reg(PMU_MBCS1);

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

int pmu_gpio(int gpio, int is_output, int value)
{
	int mask;

	if(gpio < 1 || gpio > 3)
		return -1;

	mask = 1 << (gpio - 1);

	pmu_write_reg(PMU_GPIO1CFG + (gpio - 1), (value == 0) ? 0 : 0x7, 0);

	if(is_output)
		pmu_write_reg(PMU_GPIOCTL, pmu_get_reg(PMU_GPIOCTL) & ~mask, 0);
	else
		pmu_write_reg(PMU_GPIOCTL, pmu_get_reg(PMU_GPIOCTL) | mask, 0);

	return 0;
}
