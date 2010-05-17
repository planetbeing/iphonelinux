#ifndef PIEZO_H
#define PIEZO_H

void piezo_buzz(int hertz, unsigned int microseconds);
void piezo_play(const char* command);

#endif
