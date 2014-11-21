#pragma once

#include "fm.h"
#include "ay8910.h"
#include "emu2149.h"


void ym2610_update_request(void *param);

/*typedef struct _ym2610_interface ym2610_interface;
struct _ym2610_interface
{
	//void ( *handler )( const device_config *device, int irq );	// IRQ handler for the YM2610
	void ( *handler )( int irq );	// IRQ handler for the YM2610
};*/

/*READ8_DEVICE_HANDLER( ym2610_r );
WRITE8_DEVICE_HANDLER( ym2610_w );

READ8_DEVICE_HANDLER( ym2610_status_port_a_r );
READ8_DEVICE_HANDLER( ym2610_status_port_b_r );
READ8_DEVICE_HANDLER( ym2610_read_port_r );

WRITE8_DEVICE_HANDLER( ym2610_control_port_a_w );
WRITE8_DEVICE_HANDLER( ym2610_control_port_b_w );
WRITE8_DEVICE_HANDLER( ym2610_data_port_a_w );
WRITE8_DEVICE_HANDLER( ym2610_data_port_b_w );


DEVICE_GET_INFO( ym2610 );
DEVICE_GET_INFO( ym2610b );

#define SOUND_YM2610 DEVICE_GET_INFO_NAME( ym2610 )
#define SOUND_YM2610B DEVICE_GET_INFO_NAME( ym2610b )*/

void ym2610_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
void ym2610b_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
void ym2610_stream_update_ay(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_ym2610(UINT8 ChipID, int clock, UINT8 AYDisable, int* AYrate);
void device_stop_ym2610(UINT8 ChipID);
void device_reset_ym2610(UINT8 ChipID);

UINT8 ym2610_r(UINT8 ChipID, offs_t offset);
void ym2610_w(UINT8 ChipID, offs_t offset, UINT8 data);

UINT8 ym2610_status_port_a_r(UINT8 ChipID, offs_t offset);
UINT8 ym2610_status_port_b_r(UINT8 ChipID, offs_t offset);
UINT8 ym2610_read_port_r(UINT8 ChipID, offs_t offset);

void ym2610_control_port_a_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym2610_control_port_b_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym2610_data_port_a_w(UINT8 ChipID, offs_t offset, UINT8 data);
void ym2610_data_port_b_w(UINT8 ChipID, offs_t offset, UINT8 data);

void ym2610_set_ay_emu_core(UINT8 Emulator);
void ym2610_write_data_pcmrom(UINT8 ChipID, UINT8 rom_id, offs_t ROMSize, offs_t DataStart,
							  offs_t DataLength, const UINT8* ROMData);
void ym2610_set_mute_mask(UINT8 ChipID, UINT32 MuteMaskFM, UINT32 MuteMaskAY);

