#pragma once

//#include "devcb.h"

//typedef struct _ymf271_interface ymf271_interface;
//struct _ymf271_interface
//{
//	//devcb_read8 ext_read;		/* external memory read */
//	//devcb_write8 ext_write;	/* external memory write */
//	//void (*irq_callback)(const device_config *device, int state);	/* irq callback */
//	void (*irq_callback)(int state);	/* irq callback */
//};

/*READ8_DEVICE_HANDLER( ymf271_r );
WRITE8_DEVICE_HANDLER( ymf271_w );

DEVICE_GET_INFO( ymf271 );
#define SOUND_YMF271 DEVICE_GET_INFO_NAME( ymf271 )*/

void ymf271_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_ymf271(UINT8 ChipID, int clock);
void device_stop_ymf271(UINT8 ChipID);
void device_reset_ymf271(UINT8 ChipID);

UINT8 ymf271_r(UINT8 ChipID, offs_t offset);
void ymf271_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ymf271_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					  const UINT8* ROMData);

void ymf271_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
