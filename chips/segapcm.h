/*********************************************************/
/*    SEGA 8bit PCM                                      */
/*********************************************************/

#pragma once

#define   BANK_256    (11)
#define   BANK_512    (12)
#define   BANK_12M    (13)
#define   BANK_MASK7    (0x70<<16)
#define   BANK_MASKF    (0xf0<<16)
#define   BANK_MASKF8   (0xf8<<16)

typedef struct _sega_pcm_interface sega_pcm_interface;
struct _sega_pcm_interface
{
	int  bank;
};

/*WRITE8_DEVICE_HANDLER( sega_pcm_w );
READ8_DEVICE_HANDLER( sega_pcm_r );

DEVICE_GET_INFO( segapcm );
#define SOUND_SEGAPCM DEVICE_GET_INFO_NAME( segapcm )*/

void SEGAPCM_update(UINT8 ChipID, stream_sample_t **outputs, int samples);

int device_start_segapcm(UINT8 ChipID, int clock, int intf_bank);
void device_stop_segapcm(UINT8 ChipID);
void device_reset_segapcm(UINT8 ChipID);

void sega_pcm_w(UINT8 ChipID, offs_t offset, UINT8 data);
UINT8 sega_pcm_r(UINT8 ChipID, offs_t offset);
void sega_pcm_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
						const UINT8* ROMData);

void segapcm_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);

