#ifndef RADIO_H

int radio_setup();
int radio_write(const char* str);
int radio_read(char* buf, int len);
int radio_wait_for_ok(int tries);
int radio_cmd(const char* cmd, int tries);

void radio_nvram_list();

void vibrator_loop(int frequency, int period, int timeOn);
void vibrator_once(int frequency, int time);
void vibrator_off();

int speaker_setup();
void loudspeaker_vol(int vol);
void speaker_vol(int vol);
int radio_register(int timeout);
void radio_call(const char* number);
void radio_hangup();
int radio_nvram_get(int type_in, uint8_t** data_out);

#endif
