/****************************************************************

    MAME / MESS functions

****************************************************************/

#include "mamedef.h"
//#include "sndintrf.h"
//#include "streams.h"
#include "sn76496.h"
#include "sn76489.h"
#include "sn764intf.h"


#define EC_MAME		0x00	// SN76496 core from MAME
#ifdef ENABLE_ALL_CORES
#define EC_MAXIM	0x01	// SN76489 core by Maxim (from in_vgm)
#endif

#define NULL	((void *)0)

/* for stream system */
typedef struct _sn764xx_state sn764xx_state;
struct _sn764xx_state
{
	void *chip;
};

static UINT8 EMU_CORE = 0x00;

extern UINT32 SampleRate;
#define MAX_CHIPS	0x02
static sn764xx_state SN764xxData[MAX_CHIPS];

void sn764xx_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(EMU_CORE)
	{
	case EC_MAME:
		SN76496Update(info->chip, outputs, samples);
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		SN76489_Update((SN76489_Context*)info->chip, outputs, samples);
		break;
#endif
	}
}

int device_start_sn764xx(UINT8 ChipID, int clock, int shiftregwidth, int noisetaps,
						 int negate, int stereo, int clockdivider, int freq0)
{
	sn764xx_state *info;
	int rate;
	
	if (ChipID >= MAX_CHIPS)
		return 0;
	
	info = &SN764xxData[ChipID];
	/* emulator create */
	switch(EMU_CORE)
	{
	case EC_MAME:
		rate = sn76496_start(&info->chip, clock, shiftregwidth, noisetaps,
							negate, stereo, clockdivider, freq0);
		sn76496_freq_limiter(clock & 0x3FFFFFFF, clockdivider, SampleRate);
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		rate = SampleRate;
		info->chip = SN76489_Init(clock, rate);
		if (info->chip == NULL)
			return 0;
		SN76489_Config((SN76489_Context*)info->chip, noisetaps, shiftregwidth, 0);
		break;
#endif
	}
 
	return rate;
}

void device_stop_sn764xx(UINT8 ChipID)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(EMU_CORE)
	{
	case EC_MAME:
		sn76496_shutdown(info->chip);
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		SN76489_Shutdown((SN76489_Context*)info->chip);
		break;
#endif
	}
}

void device_reset_sn764xx(UINT8 ChipID)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(EMU_CORE)
	{
	case EC_MAME:
		sn76496_reset(info->chip);
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		SN76489_Reset((SN76489_Context*)info->chip);
		break;
#endif
	}
}


void sn764xx_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(EMU_CORE)
	{
	case EC_MAME:
		switch(offset)
		{
		case 0x00:
			sn76496_write_reg(info->chip, offset & 1, data);
			break;
		case 0x01:
			sn76496_stereo_w(info->chip, offset, data);
			break;
		}
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		switch(offset)
		{
		case 0x00:
			SN76489_Write((SN76489_Context*)info->chip, data);
			break;
		case 0x01:
			SN76489_GGStereoWrite((SN76489_Context*)info->chip, data);
			break;
		}
		break;
#endif
	}
}

void sn764xx_set_emu_core(UINT8 Emulator)
{
#ifdef ENABLE_ALL_CORES
	EMU_CORE = (Emulator < 0x02) ? Emulator : 0x00;
#else
	EMU_CORE = EC_MAME;
#endif
	
	return;
}

void sn764xx_set_mute_mask(UINT8 ChipID, UINT32 MuteMask)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(EMU_CORE)
	{
	case EC_MAME:
		sn76496_set_mutemask(info->chip, MuteMask);
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		SN76489_SetMute(info->chip, ~MuteMask & 0x0F);
		break;
#endif
	}
	
	return;
}

void sn764xx_set_panning(UINT8 ChipID, INT16* PanVals)
{
	sn764xx_state *info = &SN764xxData[ChipID];
	switch(EMU_CORE)
	{
	case EC_MAME:
		break;
#ifdef ENABLE_ALL_CORES
	case EC_MAXIM:
		SN76489_SetPanning(info->chip, PanVals[0x00], PanVals[0x01], PanVals[0x02], PanVals[0x03]);
		break;
#endif
	}
	
	return;
}
