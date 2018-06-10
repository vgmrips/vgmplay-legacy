#pragma once

void qsoundc_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_qsound_ctr(UINT8 ChipID, int clock);
void device_stop_qsound_ctr(UINT8 ChipID);
void device_reset_qsound_ctr(UINT8 ChipID);

void qsoundc_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 qsoundc_r(UINT8 ChipID, offs_t offset);
void qsoundc_write_data(UINT8 ChipID, UINT8 address, UINT16 data);

void qsoundc_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					   const UINT8* ROMData);
void qsoundc_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

void qsoundc_wait_busy(UINT8 ChipID);
