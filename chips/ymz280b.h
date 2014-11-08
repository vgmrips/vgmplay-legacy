/**********************************************************************************************
 *
 *   Yamaha YMZ280B driver
 *   by Aaron Giles
 *
 **********************************************************************************************/

#pragma once

//#include "devcb.h"

typedef struct _ymz280b_interface ymz280b_interface;
struct _ymz280b_interface
{
	//void (*irq_callback)(const device_config *device, int state);	/* irq callback */
	void (*irq_callback)(int state);	/* irq callback */
	//devcb_read8 ext_read;			/* external RAM read */
	//devcb_write8 ext_write;		/* external RAM write */
};

/*READ8_DEVICE_HANDLER ( ymz280b_r );
WRITE8_DEVICE_HANDLER( ymz280b_w );

DEVICE_GET_INFO( ymz280b );
#define SOUND_YMZ280B DEVICE_GET_INFO_NAME( ymz280b )*/

void ymz280b_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_ymz280b(UINT8 ChipID, int clock);
void device_stop_ymz280b(UINT8 ChipID);
void device_reset_ymz280b(UINT8 ChipID);

UINT8 ymz280b_r(UINT8 ChipID, offs_t offset);
void ymz280b_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ymz280b_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					   const UINT8* ROMData);

void ymz280b_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
