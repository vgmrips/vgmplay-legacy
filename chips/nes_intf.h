#pragma once

void nes_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_nes(UINT8 ChipID, int clock);
void device_stop_nes(UINT8 ChipID);
void device_reset_nes(UINT8 ChipID);

void nes_w(UINT8 ChipID, offs_t offset, UINT8 data);

void nes_write_ram(UINT8 ChipID, offs_t DataStart, offs_t DataLength, const UINT8* RAMData);

void nes_set_emu_core(UINT8 Emulator);
void nes_set_options(UINT16 Options);
void nes_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
