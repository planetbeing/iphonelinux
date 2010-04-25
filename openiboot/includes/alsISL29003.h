#ifndef ALS_H
#define ALS_H

int als_setup();
void als_sethighthreshold(uint16_t value);
void als_setlowthreshold(uint16_t value);
uint16_t als_sensordata();
uint16_t als_timerdata();
void als_enable_interrupt();
void als_disable_interrupt();

#endif
