#ifndef __WS_AUDIO_H__
#define __WS_AUDIO_H__

int ws_audio_init(UINT8 ChipID, int clock);
void ws_audio_reset(UINT8 ChipID);
void ws_audio_done(UINT8 ChipID);
void ws_audio_update(UINT8 ChipID, stream_sample_t** buffer, int length);
void ws_audio_port_write(UINT8 ChipID, UINT8 port, UINT8 value);
UINT8 ws_audio_port_read(UINT8 ChipID, UINT8 port);
//void ws_audio_process(void);
//void ws_audio_sounddma(void);
void ws_write_ram(UINT8 ChipID, UINT16 offset, UINT8 value);
void ws_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
//extern int WaveAdrs;

#endif
