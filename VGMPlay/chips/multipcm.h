#pragma once

//#include "devlegcy.h"

void MultiPCM_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_multipcm(UINT8 ChipID, int clock);
void device_stop_multipcm(UINT8 ChipID);
void device_reset_multipcm(UINT8 ChipID);

//WRITE8_DEVICE_HANDLER( multipcm_w );
//READ8_DEVICE_HANDLER( multipcm_r );
void multipcm_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 multipcm_r(UINT8 ChipID, offs_t offset);

void multipcm_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
						const UINT8* ROMData);
//void multipcm_set_bank(running_device *device, UINT32 leftoffs, UINT32 rightoffs);
void multipcm_set_bank(UINT8 ChipID, UINT32 leftoffs, UINT32 rightoffs);
void multipcm_bank_write(UINT8 ChipID, UINT8 offset, UINT16 data);

void multipcm_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
//DECLARE_LEGACY_SOUND_DEVICE(MULTIPCM, multipcm);
