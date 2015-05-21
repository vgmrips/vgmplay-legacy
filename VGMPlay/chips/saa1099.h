#pragma once

#ifndef __SAA1099_H__
#define __SAA1099_H__

/**********************************************
    Philips SAA1099 Sound driver
**********************************************/

//WRITE8_DEVICE_HANDLER( saa1099_control_w );
void saa1099_control_w(UINT8 ChipID, offs_t offset, UINT8 data);
//WRITE8_DEVICE_HANDLER( saa1099_data_w );
void saa1099_data_w(UINT8 ChipID, offs_t offset, UINT8 data);

//DECLARE_LEGACY_SOUND_DEVICE(SAA1099, saa1099);
void saa1099_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_saa1099(UINT8 ChipID, int clock);
void device_stop_saa1099(UINT8 ChipID);
void device_reset_saa1099(UINT8 ChipID);

void saa1099_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

#endif /* __SAA1099_H__ */
