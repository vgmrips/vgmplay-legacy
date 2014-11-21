#pragma once

/*typedef struct _y8950_interface y8950_interface;
struct _y8950_interface
{
	//void (*handler)(const device_config *device, int linestate);
	void (*handler)(int linestate);

	read8_device_func keyboardread;
	write8_device_func keyboardwrite;
	read8_device_func portread;
	write8_device_func portwrite;
};*/

/*READ8_DEVICE_HANDLER( y8950_r );
WRITE8_DEVICE_HANDLER( y8950_w );

READ8_DEVICE_HANDLER( y8950_status_port_r );
READ8_DEVICE_HANDLER( y8950_read_port_r );
WRITE8_DEVICE_HANDLER( y8950_control_port_w );
WRITE8_DEVICE_HANDLER( y8950_write_port_w );

DEVICE_GET_INFO( y8950 );
#define SOUND_Y8950 DEVICE_GET_INFO_NAME( y8950 )*/
void y8950_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_y8950(UINT8 ChipID, int clock);
void device_stop_y8950(UINT8 ChipID);
void device_reset_y8950(UINT8 ChipID);

UINT8 y8950_r(UINT8 ChipID, offs_t offset);
void y8950_w(UINT8 ChipID, offs_t offset, UINT8 data);

UINT8 y8950_status_port_r(UINT8 ChipID, offs_t offset);
UINT8 y8950_read_port_r(UINT8 ChipID, offs_t offset);
void y8950_control_port_w(UINT8 ChipID, offs_t offset, UINT8 data);
void y8950_write_port_w(UINT8 ChipID, offs_t offset, UINT8 data);

void y8950_write_data_pcmrom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart,
							  offs_t DataLength, const UINT8* ROMData);
void y8950_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
