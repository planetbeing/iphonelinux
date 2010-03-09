#ifndef RADIO_H

int radio_setup();
int radio_write(const char* str);
int radio_read(char* buf, int len);
int radio_wait_for_ok(int tries);

void vibrator_loop(int frequency, int period, int timeOn);
void vibrator_once(int frequency, int time);
void vibrator_off();

#endif
