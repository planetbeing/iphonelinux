struct sound_settings_info {
	const char *unit;
	char numdecimals;
	char steps;
	short minval;
	short maxval;
	short defaultval;
};

enum {
	SOUND_VOLUME = 0,
	SOUND_BASS,
	SOUND_TREBLE,
	SOUND_BALANCE,
	SOUND_CHANNELS,
	SOUND_STEREO_WIDTH,
#if defined(HAVE_RECORDING)
	SOUND_LEFT_GAIN,
	SOUND_RIGHT_GAIN,
	SOUND_MIC_GAIN,
#endif
	SOUND_BASS_CUTOFF,
	SOUND_TREBLE_CUTOFF,
	SOUND_LAST_SETTING, /* Keep this last */
};

void audiohw_init();
void audiohw_postinit();
void audiohw_play_pcm(const void* addr_in, uint32_t size, int use_speaker);
void audiohw_set_headphone_vol(int vol_l, int vol_r);
void audiohw_set_lineout_vol(int vol_l, int vol_r);
void audiohw_set_aux_vol(int vol_l, int vol_r);
int audiohw_transfers_done();
void audiohw_pause();
void audiohw_resume();
uint32_t audiohw_get_position();
uint32_t audiohw_get_total();
void audiohw_mute(int mute);

void audiohw_switch_normal_call(int in_call);

#ifdef CONFIG_3G
void audiohw_set_speaker_vol(int vol);
#endif


extern const struct sound_settings_info audiohw_settings[];
