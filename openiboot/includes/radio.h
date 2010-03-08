#ifndef RADIO_H

int radio_setup();
int radio_write(const char* str);
int radio_read(char* buf, int len);

#endif
