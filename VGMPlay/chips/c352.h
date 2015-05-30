#pragma once

#ifndef __C352_H__
#define __C352_H__

//#include  "devlegcy.h"

void c352_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_c352(UINT8 ChipID, int clock, int clkdiv);
void device_stop_c352(UINT8 ChipID);
void device_reset_c352(UINT8 ChipID);

//READ16_DEVICE_HANDLER( c352_r );
//WRITE16_DEVICE_HANDLER( c352_w );
UINT16 c352_r(UINT8 ChipID, offs_t offset);
void c352_w(UINT8 ChipID, offs_t offset, UINT16 data);

void c352_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					const UINT8* ROMData);

void c352_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

//DECLARE_LEGACY_SOUND_DEVICE(C352, c352);

#endif /* __C352_H__ */

