#pragma once


/*typedef struct _ymf262_interface ymf262_interface;
struct _ymf262_interface
{
	//void (*handler)(const device_config *device, int irq);
	void (*handler)(int irq);
};*/


/*READ8_DEVICE_HANDLER( ymf262_r );
WRITE8_DEVICE_HANDLER( ymf262_w );

READ8_DEVICE_HANDLER ( ymf262_status_r );
WRITE8_DEVICE_HANDLER( ymf262_register_a_w );
WRITE8_DEVICE_HANDLER( ymf262_register_b_w );
WRITE8_DEVICE_HANDLER( ymf262_data_a_w );
WRITE8_DEVICE_HANDLER( ymf262_data_b_w );


DEVICE_GET_INFO( ymf262 );
#define SOUND_YMF262 DEVICE_GET_INFO_NAME( ymf262 )*/

void ymf262_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_ymf262(UINT8 ChipID, int clock);
void device_stop_ymf262(UINT8 ChipID);
void device_reset_ymf262(UINT8 ChipID);

UINT8 ymf262_r(UINT8 ChipID, offs_t offset);
void ymf262_w(UINT8 ChipID, offs_t offset, UINT8 data);

UINT8 ymf262_status_r(UINT8 ChipID, offs_t offset);
void ymf262_register_a_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ymf262_register_b_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ymf262_data_a_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ymf262_data_b_w(UINT8 ChipID, offs_t offset, UINT8 data);

void ymf262_set_emu_core(UINT8 Emulator);
void ymf262_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

