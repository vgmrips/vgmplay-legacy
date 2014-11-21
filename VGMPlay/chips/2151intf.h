#pragma once

/*typedef struct _ym2151_interface ym2151_interface;
struct _ym2151_interface
{
	//void (*irqhandler)(const device_config *device, int irq);
	void (*irqhandler)(int irq);
	write8_device_func portwritehandler;
};*/

/*READ8_DEVICE_HANDLER( ym2151_r );
WRITE8_DEVICE_HANDLER( ym2151_w );

READ8_DEVICE_HANDLER( ym2151_status_port_r );
WRITE8_DEVICE_HANDLER( ym2151_register_port_w );
WRITE8_DEVICE_HANDLER( ym2151_data_port_w );

DEVICE_GET_INFO( ym2151 );
#define SOUND_YM2151 DEVICE_GET_INFO_NAME( ym2151 )*/
void ym2151_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_ym2151(UINT8 ChipID, int clock);
void device_stop_ym2151(UINT8 ChipID);
void device_reset_ym2151(UINT8 ChipID);

UINT8 ym2151_r(UINT8 ChipID, offs_t offset);
void ym2151_w(UINT8 ChipID, offs_t offset, UINT8 data);

UINT8 ym2151_status_port_r(UINT8 ChipID, offs_t offset);
void ym2151_register_port_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym2151_data_port_w(UINT8 ChipID, offs_t offset, UINT8 data);

void ym2151_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
