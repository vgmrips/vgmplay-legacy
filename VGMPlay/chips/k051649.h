#pragma once

//#ifndef __K051649_H__
//#define __K051649_H__

//#include "devlegcy.h"

/*WRITE8_DEVICE_HANDLER( k051649_waveform_w );
READ8_DEVICE_HANDLER( k051649_waveform_r );
WRITE8_DEVICE_HANDLER( k051649_volume_w );
WRITE8_DEVICE_HANDLER( k051649_frequency_w );
WRITE8_DEVICE_HANDLER( k051649_keyonoff_w );

WRITE8_DEVICE_HANDLER( k052539_waveform_w );

DECLARE_LEGACY_SOUND_DEVICE(K051649, k051649);*/

void k051649_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_k051649(UINT8 ChipID, int clock);
void device_stop_k051649(UINT8 ChipID);
void device_reset_k051649(UINT8 ChipID);

void k051649_waveform_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 k051649_waveform_r(UINT8 ChipID, offs_t offset);
void k051649_volume_w(UINT8 ChipID, offs_t offset, UINT8 data);
void k051649_frequency_w(UINT8 ChipID, offs_t offset, UINT8 data);
void k051649_keyonoff_w(UINT8 ChipID, offs_t offset, UINT8 data);

void k052539_waveform_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 k052539_waveform_r(UINT8 ChipID, offs_t offset);

void k051649_test_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 k051649_test_r(UINT8 ChipID, offs_t offset);

void k051649_w(UINT8 ChipID, offs_t offset, UINT8 data);

void k051649_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

//#endif /* __K051649_H__ */
