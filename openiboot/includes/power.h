#ifndef POWER_H
#define POWER_H

#include "openiboot.h"

int power_setup();
int power_ctrl(uint32_t device, OnOff on_off);

#endif
