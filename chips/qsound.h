/*********************************************************

    Capcom Q-Sound system

*********************************************************/

#pragma once

//#include "devlegcy.h"

#define QSOUND_CLOCK    4000000   /* default 4MHz clock */

void qsound_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_qsound(UINT8 ChipID, int clock);
void device_stop_qsound(UINT8 ChipID);
void device_reset_qsound(UINT8 ChipID);


//WRITE8_DEVICE_HANDLER( qsound_w );
//READ8_DEVICE_HANDLER( qsound_r );
void qsound_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 qsound_r(UINT8 ChipID, offs_t offset);


void qsound_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					   const UINT8* ROMData);
void qsound_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

//DECLARE_LEGACY_SOUND_DEVICE(QSOUND, qsound);
