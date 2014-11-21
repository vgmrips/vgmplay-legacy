#include "mamedef.h"
#ifdef ENABLE_ALL_CORES
#include "c6280.h"
#endif
#include "Ootake_PSG.h"

#ifdef ENABLE_ALL_CORES
#define EC_MAME		0x01
#endif
#define EC_OOTAKE	0x00

#define NULL	((void *)0)

typedef struct _c6280_state
{
	void* chip;
} c6280_state;

extern UINT32 SampleRate;
extern UINT8 CHIP_SAMPLING_MODE;
extern INT32 CHIP_SAMPLE_RATE;
static UINT8 EMU_CORE = 0x00;

#define MAX_CHIPS	0x02
static c6280_state C6280Data[MAX_CHIPS];

void c6280_update(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	c6280_state* info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		c6280m_update(info->chip, outputs, samples);
		break;
#endif
	case EC_OOTAKE:
		PSG_Mix(info->chip, outputs, samples);
		break;
	}
}

int device_start_c6280(UINT8 ChipID, int clock)
{
	c6280_state* info;
	int rate;
	
	if (ChipID >= MAX_CHIPS)
		return 0;
	
	info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		rate = (clock & 0x7FFFFFFF)/16;
		if (((CHIP_SAMPLING_MODE & 0x01) && rate < CHIP_SAMPLE_RATE) ||
			CHIP_SAMPLING_MODE == 0x02)
			rate = CHIP_SAMPLE_RATE;
		
		info->chip = device_start_c6280m(clock, rate);
		if (info->chip == NULL)
			return 0;
		break;
#endif
	case EC_OOTAKE:
		rate = SampleRate;
		info->chip = PSG_Init(clock, rate);
		if (info->chip == NULL)
			return 0;
		break;
	}
 
	return rate;
}

void device_stop_c6280(UINT8 ChipID)
{
	c6280_state* info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		device_stop_c6280m(info->chip);
		break;
#endif
	case EC_OOTAKE:
		PSG_Deinit(info->chip);
		break;
	}
	info->chip = NULL;
	
	return;
}

void device_reset_c6280(UINT8 ChipID)
{
	c6280_state* info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		device_reset_c6280m(info->chip);
		break;
#endif
	case EC_OOTAKE:
		PSG_ResetVolumeReg(info->chip);
		break;
	}
	return;
}

UINT8 c6280_r(UINT8 ChipID, offs_t offset)
{
	c6280_state* info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		return c6280m_r(info->chip, offset);
#endif
	case EC_OOTAKE:
		return PSG_Read(info->chip, offset);
	default:
		return 0x00;
	}
}

void c6280_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	c6280_state* info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		c6280m_w(info->chip, offset, data);
		break;
#endif
	case EC_OOTAKE:
		PSG_Write(info->chip, offset, data);
		break;
	}
	
	return;
}


void c6280_set_emu_core(UINT8 Emulator)
{
#ifdef ENABLE_ALL_CORES
	EMU_CORE = (Emulator < 0x02) ? Emulator : 0x00;
#else
	EMU_CORE = EC_OOTAKE;
#endif
	
	return;
}

void c6280_set_mute_mask(UINT8 ChipID, UINT32 MuteMask)
{
	c6280_state* info = &C6280Data[ChipID];
	switch(EMU_CORE)
	{
#ifdef ENABLE_ALL_CORES
	case EC_MAME:
		c6280m_set_mute_mask(info->chip, MuteMask);
		break;
#endif
	case EC_OOTAKE:
		PSG_SetMuteMask(info->chip, MuteMask);
		break;
	}
	
	return;
}
