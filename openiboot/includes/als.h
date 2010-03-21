#ifndef ALS_H
#define ALS_H

int als_setup();
void als_sethighthreshold(uint16_t value);
void als_setlowthreshold(uint16_t value);
uint16_t als_data0();
uint16_t als_data1();
void als_enable_interrupt();
void als_disable_interrupt();

#endif
