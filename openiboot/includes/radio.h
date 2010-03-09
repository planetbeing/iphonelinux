#ifndef RADIO_H

int radio_setup();
int radio_write(const char* str);
int radio_read(char* buf, int len);
int radio_wait_for_ok(int tries);
int radio_cmd(const char* cmd, int tries);

void vibrator_loop(int frequency, int period, int timeOn);
void vibrator_once(int frequency, int time);
void vibrator_off();

int speaker_setup();
void speaker_vol(int vol);

#endif
