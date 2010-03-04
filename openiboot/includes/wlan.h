#ifndef WLAN_H
#define WLAN_H

int wlan_setup();
int wlan_prog_helper(const uint8_t * firmware, int size);
int wlan_prog_real(const uint8_t* firmware, size_t size);

#endif
