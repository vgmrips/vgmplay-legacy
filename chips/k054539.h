/*********************************************************

    Konami 054539 PCM Sound Chip

*********************************************************/

#pragma once

//#include "devlegcy.h"

/*typedef struct _k054539_interface k054539_interface;
struct _k054539_interface
{
	const char *rgnoverride;
	void (*apan)(device_t *, double, double);	// Callback for analog output mixing levels (0..1 for each channel)
	void (*irq)(device_t *);
};*/


void k054539_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_k054539(UINT8 ChipID, int clock);
void device_stop_k054539(UINT8 ChipID);
void device_reset_k054539(UINT8 ChipID);


//WRITE8_DEVICE_HANDLER( k054539_w );
//READ8_DEVICE_HANDLER( k054539_r );
void k054539_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 k054539_r(UINT8 ChipID, offs_t offset);

//* control flags, may be set at DRIVER_INIT().
#define K054539_RESET_FLAGS     0
#define K054539_REVERSE_STEREO  1
#define K054539_DISABLE_REVERB  2
#define K054539_UPDATE_AT_KEYON 4

//void k054539_init_flags(device_t *device, int flags);
void k054539_init_flags(UINT8 ChipID, int flags);

/*
    Note that the eight PCM channels of a K054539 do not have separate
    volume controls. Considering the global attenuation equation may not
    be entirely accurate, k054539_set_gain() provides means to control
    channel gain. It can be called anywhere but preferrably from
    DRIVER_INIT().

    Parameters:
        chip    : 0 / 1
        channel : 0 - 7
        gain    : 0.0=silent, 1.0=no gain, 2.0=twice as loud, etc.
*/
//void k054539_set_gain(device_t *device, int channel, double gain);
void k054539_set_gain(UINT8 ChipID, int channel, double gain);


void k054539_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					   const UINT8* ROMData);
void k054539_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);


//DECLARE_LEGACY_SOUND_DEVICE(K054539, k054539);
