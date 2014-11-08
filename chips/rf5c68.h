/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#pragma once

/******************************************/
/*WRITE8_DEVICE_HANDLER( rf5c68_w );

READ8_DEVICE_HANDLER( rf5c68_mem_r );
WRITE8_DEVICE_HANDLER( rf5c68_mem_w );

DEVICE_GET_INFO( rf5c68 );
#define SOUND_RF5C68 DEVICE_GET_INFO_NAME( rf5c68 )*/

void rf5c68_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_rf5c68(UINT8 ChipID, int clock);
void device_stop_rf5c68(UINT8 ChipID);
void device_reset_rf5c68(UINT8 ChipID);

void rf5c68_w(UINT8 ChipID, offs_t offset, UINT8 data);

UINT8 rf5c68_mem_r(UINT8 ChipID, offs_t offset);
void rf5c68_mem_w(UINT8 ChipID, offs_t offset, UINT8 data);
void rf5c68_write_ram(UINT8 ChipID, offs_t DataStart, offs_t DataLength, const UINT8* RAMData);

void rf5c68_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
