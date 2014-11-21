#pragma once

/*typedef struct _ym3812_interface ym3812_interface;
struct _ym3812_interface
{
	//void (*handler)(const device_config *device, int linestate);
	void (*handler)(int linestate);
};*/

/*READ8_DEVICE_HANDLER( ym3812_r );
WRITE8_DEVICE_HANDLER( ym3812_w );

READ8_DEVICE_HANDLER( ym3812_status_port_r );
READ8_DEVICE_HANDLER( ym3812_read_port_r );
WRITE8_DEVICE_HANDLER( ym3812_control_port_w );
WRITE8_DEVICE_HANDLER( ym3812_write_port_w );

DEVICE_GET_INFO( ym3812 );
#define SOUND_YM3812 DEVICE_GET_INFO_NAME( ym3812 )*/

void ym3812_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_ym3812(UINT8 ChipID, int clock);
void device_stop_ym3812(UINT8 ChipID);
void device_reset_ym3812(UINT8 ChipID);

UINT8 ym3812_r(UINT8 ChipID, offs_t offset);
void ym3812_w(UINT8 ChipID, offs_t offset, UINT8 data);

UINT8 ym3812_status_port_r(UINT8 ChipID, offs_t offset);
UINT8 ym3812_read_port_r(UINT8 ChipID, offs_t offset);
void ym3812_control_port_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym3812_write_port_w(UINT8 ChipID, offs_t offset, UINT8 data);

void ym3812_set_emu_core(UINT8 Emulator);
void ym3812_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
