#include "openiboot.h"
#include "piezo.h"
#include "timer.h"
#include "clock.h"
#include "util.h"
#include "hardware/timer.h"

static const int notes[] = {2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951};

void piezo_buzz(int hertz, unsigned int microseconds)
{
	// The second option represents the duty cycle of the PWM that drives the buzzer. In this case it is always 50%.
	if(hertz > 0)
	{
		int prescaler = 1;
		uint32_t count = TicksPerSec / hertz;

		while(count > 0xFFFF)
		{
			count >>= 1;
			prescaler <<= 1;
		}

		timer_init(PiezoTimer, count >> 1, count, prescaler - 1, 0, FALSE, FALSE, FALSE, FALSE);
		timer_on_off(PiezoTimer, ON);
	}

	udelay(microseconds);

	if(hertz > 0)
		timer_on_off(PiezoTimer, OFF);
}

static inline int note_frequency(char note, int octave, int is_flat, int is_sharp)
{
	int idx;

	if(is_flat && is_sharp)
		return -1;

	if(octave < 0 || octave > 6)
		return -1;

	if(note == 'C')
		idx = 0;
	else if(note == 'D')
		idx = 2;
	else if(note == 'E')
		idx = 4;
	else if(note == 'F')
		idx = 5;
	else if(note == 'G')
		idx = 7;
	else if(note == 'A')
		idx = 9;
	else if(note == 'B')
		idx = 11;
	else
		return -1;

	if(is_flat && note == 'C')
		return -1;

	if(is_sharp && note == 'B')
		return -1;

	if(is_flat)
		idx = (idx - 1) % 12;

	if(is_sharp)
		idx = (idx + 1) % 12;

	return notes[idx]/(1 << (6 - octave));
}

static inline unsigned int tempo_to_duration(int tempo)
{
	return 60000000U / (unsigned int) tempo;
}

void piezo_play(const char* command)
{
	int octave = 3;
	const char* cur = command;
	unsigned int tempo = tempo_to_duration(120);
	unsigned int duration = tempo;
	int mode = 0;

	while(*cur != '\0')
	{
		int frequency = 0;
		int cur_duration = duration;

		if(*cur == 'O' || *cur == 'o')
		{
			octave = *(cur + 1) - '0';
			if(octave < 0 || octave > 6)
				return;
			cur += 2;
			continue;
		} else if(*cur == '<')
		{
			--octave;
			if(octave < 0 || octave > 6)
				return;
			++cur;
			continue;
		} else if(*cur == '>')
		{
			++octave;
			if(octave < 0 || octave > 6)
				return;
			++cur;
			continue;
		} else if(*cur == 'N' || *cur == 'n')
		{
			int a, b;

			++cur;

			a = *cur - '0';
			if(a < 0 || a > 9)
				return;

			++cur;

			b = *cur - '0';
			if(b >= 0 && b <= 9)
			{
				a = a * 10 + b;
				++cur;
			}

			if(a < 0 || a > 84)
				return;

			if(a == 0)
				frequency = 0;
			else
			{
				int xnote, xoctave;

				xnote = (a - 1) % 12;
				xoctave = (a - 1) / 12;

				frequency = notes[xnote] / (1 << (6 - xoctave));
			}
		} else if(*cur == 'L' || *cur == 'l')
		{
			int a, b;

			++cur;

			a = *cur - '0';
			if(a < 0 || a > 9)
				return;

			++cur;

			b = *cur - '0';
			if(b >= 0 && b <= 9)
			{
				a = a * 10 + b;
				++cur;
			}

			if(a < 1 || a > 64)
				return;

			duration = (tempo * 4) / a;
			continue;
		} else if(*cur == 'P' || *cur == 'p')
		{
			int a, b;

			++cur;

			a = *cur - '0';
			if(a < 0 || a > 9)
				return;

			++cur;

			b = *cur - '0';
			if(b >= 0 && b <= 9)
			{
				a = a * 10 + b;
				++cur;
			}

			if(a < 1 || a > 64)
				return;

			udelay((tempo * 4) / a);
			continue;
		} else if(*cur == 'T' || *cur == 't')
		{
			int a, b;

			++cur;

			a = *cur - '0';
			if(a < 0 || a > 9)
				return;

			++cur;

			b = *cur - '0';
			if(b >= 0 && b <= 9)
			{
				a = a * 10 + b;
				++cur;
			}

			// intentionally repeated to possibly try a third character.
			b = *cur - '0';
			if(b >= 0 && b <= 9)
			{
				a = a * 10 + b;
				++cur;
			}

			if(a < 32 || a > 255)
				return;

			tempo = tempo_to_duration(a);
			duration = tempo;
			continue;
		} else if((*cur >= 'A' && *cur <= 'G') || (*cur >= 'a' && *cur <= 'g'))
		{
			int is_flat = 0, is_sharp = 0;
			char note = *cur;

			++cur;

			if(*cur == '#' || *cur == '+')
			{
				is_sharp = 1;
				++cur;
			} else if(*cur == '-')
			{
				is_flat = 1;
				++cur;
			}

			if(note >= 'a' && note <= 'g')
				frequency = note_frequency(note - 'a' + 'A', octave, is_flat, is_sharp);
			else
				frequency = note_frequency(note, octave, is_flat, is_sharp);

			if(frequency < 0)
				return;
		} else if(*cur == 'M' || *cur =='m')
		{
			++cur;
			if(*cur == 'N' || *cur == 'n')
			{
				mode = 0;
			} else if(*cur == 'L' || *cur == 'l')
			{
				mode = 1;
			} else if(*cur == 'S' || *cur == 's')
			{
				mode = 2;
			}
			++cur;
			continue;
		} else
		{
			++cur;
			continue;
		}

		if(*cur == '.')
		{
			cur_duration = (cur_duration * 3) / 2;
			++cur;
		}

		if(mode == 1)
		{
			piezo_buzz(frequency, cur_duration);
		} else if(mode == 0)
		{
			piezo_buzz(frequency, (cur_duration * 7) / 8);
			udelay(cur_duration / 8);
		} else if(mode == 2)
		{
			piezo_buzz(frequency, (cur_duration * 3) / 4);
			udelay(cur_duration / 4);
		}
	}
}
