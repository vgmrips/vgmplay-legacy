#pragma once

void ayxx_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_ayxx(UINT8 ChipID, int clock, UINT8 chip_type, UINT8 Flags);
void device_stop_ayxx(UINT8 ChipID);
void device_reset_ayxx(UINT8 ChipID);

void ayxx_w(UINT8 ChipID, offs_t offset, UINT8 data);

void ayxx_set_emu_core(UINT8 Emulator);
void ayxx_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
