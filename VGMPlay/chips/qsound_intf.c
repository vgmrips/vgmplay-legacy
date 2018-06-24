//#include "emu.h"
#include "mamedef.h"
#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL
#include <math.h>
#include "qsound_intf.h"
#include "qsound_mame.h"
#include "qsound_ctr.h"

#ifdef ENABLE_ALL_CORES
#define EC_MAME		0x01	// QSound HLE core from MAME
#endif
#define EC_CTR		0x00	// superctr custom HLE

static UINT8 EMU_CORE = 0x00;
// fix broken optimization of old VGMs causing problems with the new core
static UINT8 key_on_hack = 0x00;
static UINT16 start_addr_cache[2][16];
static UINT16 pitch_cache[2][16];
static UINT16 data_latch[2];

int device_start_qsound(UINT8 ChipID, int clock)
{
	memset(start_addr_cache[ChipID], 0, sizeof(UINT16)*16);
	memset(pitch_cache[ChipID], 0, sizeof(UINT16)*16);
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		return device_start_qsoundm(ChipID, clock);
#endif
	case EC_CTR:
		if(clock < 10000000)
		{
			clock *= 15;
			key_on_hack = 1;
		}
		return device_start_qsound_ctr(ChipID, clock);
	}
	return 0;
}

void device_stop_qsound(UINT8 ChipID)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		device_stop_qsoundm(ChipID); return;
#endif
	case EC_CTR:
		device_stop_qsound_ctr(ChipID); return;
	}
}

void device_reset_qsound(UINT8 ChipID)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		device_reset_qsoundm(ChipID); return;
#endif
	case EC_CTR:
		device_reset_qsound_ctr(ChipID);
		
		// need to wait until the chip is ready before we start writing to it ...
		// we do this by time travel.
		qsoundc_wait_busy(ChipID);
		return;
	}
}

void qsound_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		qsoundm_w(ChipID, offset, data); return;
#endif
	case EC_CTR:
		if(key_on_hack)
		{
			int ch;
			switch (offset)
			{
				// need to handle three cases, as vgm_cmp can remove writes to both phase and bank
				// registers, depending on version.
				// - start address was written before end/loop, but phase register is written
				// - as above, but phase is not written (we use bank as a backup then)
				// - voice parameters are written during a note (we can't rewrite the address then)
				case 0:
					data_latch[ChipID] = (data_latch[ChipID] & 0x00ff) | (data << 8);
					break;
				case 1:
					data_latch[ChipID] = (data_latch[ChipID] & 0xff00) | data;
					break;
				case 2:
					if(data > 0x7f)
						break;
					ch = data>>3;
					
					switch(data & 7)
					{
						case 1:	// Start addr. write
							start_addr_cache[ChipID][ch] = data_latch[ChipID];
							break;
						case 2:	// Pitch write
							// (old HLE assumed writing a non-zero value after a zero value was Key On)
							if(pitch_cache[ChipID][ch] == 0 && data_latch[ChipID] != 0)
								qsoundc_write_data(ChipID, (ch << 3) + 1, start_addr_cache[ChipID][ch]);
							pitch_cache[ChipID][ch] = data_latch[ChipID];
							break;
						case 3: // Phase (old HLE also assumed this was Key On)
							qsoundc_write_data(ChipID, (ch << 3) + 1, start_addr_cache[ChipID][ch]);
						default:
							break;
					}
			}
		}
		qsoundc_w(ChipID, offset, data);
		
		// need to wait until the chip is ready before we start writing to it ...
		// we do this by time travel.
		if(offset == 2 && data == 0xe3)
			qsoundc_wait_busy(ChipID);
		
		return;
	}
}

UINT8 qsound_r(UINT8 ChipID, offs_t offset)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		return qsoundm_r(ChipID, offset);
#endif
	case EC_CTR:
		return qsoundc_r(ChipID, offset);
	}
	return 0;
}

void qsound_update(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		qsoundm_update(ChipID, outputs, samples); return;
#endif
	case EC_CTR:
		qsoundc_update(ChipID, outputs, samples); return;
	}
}

void qsound_write_rom(UINT8 ChipID, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					  const UINT8* ROMData)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		qsoundm_write_rom(ChipID, ROMSize, DataStart, DataLength, ROMData); return;
#endif
	case EC_CTR:
		qsoundc_write_rom(ChipID, ROMSize, DataStart, DataLength, ROMData); return;
	}
}

void qsound_set_mute_mask(UINT8 ChipID, UINT32 MuteMask)
{
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		qsoundm_set_mute_mask(ChipID, MuteMask); return;
#endif
	case EC_CTR:
		qsoundc_set_mute_mask(ChipID, MuteMask); return;
	}
}

void qsound_set_emu_core(UINT8 Emulator)
{
#ifdef ENABLE_ALL_CORES
	EMU_CORE = (Emulator < 0x02) ? Emulator : 0x00;
#else
	EMU_CORE = EC_CTR;
#endif
	
	return;
}
