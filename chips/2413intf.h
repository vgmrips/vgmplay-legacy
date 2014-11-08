#pragma once

/*WRITE8_DEVICE_HANDLER( ym2413_w );

WRITE8_DEVICE_HANDLER( ym2413_register_port_w );
WRITE8_DEVICE_HANDLER( ym2413_data_port_w );

DEVICE_GET_INFO( ym2413 );
#define SOUND_YM2413 DEVICE_GET_INFO_NAME( ym2413 )*/

void ym2413_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_ym2413(UINT8 ChipID, int clock);
void device_stop_ym2413(UINT8 ChipID);
void device_reset_ym2413(UINT8 ChipID);

void ym2413_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym2413_register_port_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym2413_data_port_w(UINT8 ChipID, offs_t offset, UINT8 data);

void ym2413_set_emu_core(UINT8 Emulator);
void ym2413_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
void ym2413_set_panning(UINT8 ChipID, INT16* PanVals);
