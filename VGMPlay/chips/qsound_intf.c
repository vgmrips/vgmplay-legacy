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
static UINT16 data_latch[2];

int device_start_qsound(UINT8 ChipID, int clock)
{
	memset(start_addr_cache[ChipID], 0, sizeof(UINT16)*16);
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
		device_reset_qsound_ctr(ChipID); return;
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
			switch (offset)
			{
				case 0:
					data_latch[ChipID] = (data_latch[ChipID] & 0x00ff) | (data << 8);
					break;
				case 1:
					data_latch[ChipID] = (data_latch[ChipID] & 0xff00) | data;
					break;
				case 2:
					if(data > 0x7f)
						break;
					int ch = data>>3;
					
					// Start addr. write
					if((data & 7) == 1)
						start_addr_cache[ChipID][ch] = data_latch[ChipID];
					// Phase (old HLE assumed this was Key On)
					else if((data & 7) == 3)
						qsoundc_write_data(ChipID, (ch << 3) + 1, start_addr_cache[ChipID][ch]);
					// Bank
					else if((data & 7) == 0)
						qsoundc_write_data(ChipID, (((ch+1)&15) << 3) + 1, start_addr_cache[ChipID][(ch+1)&15]);
					
					break;
			}
		}
		qsoundc_w(ChipID, offset, data); return;
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
