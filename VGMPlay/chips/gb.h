
/* Custom Sound Interface */
UINT8 gb_wave_r(UINT8 ChipID, offs_t offset);
void gb_wave_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 gb_sound_r(UINT8 ChipID, offs_t offset);
void gb_sound_w(UINT8 ChipID, offs_t offset, UINT8 data);

void gameboy_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_gameboy_sound(UINT8 ChipID, int clock);
void device_stop_gameboy_sound(UINT8 ChipID);
void device_reset_gameboy_sound(UINT8 ChipID);

void gameboy_sound_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
void gameboy_sound_set_options(UINT8 Flags);
