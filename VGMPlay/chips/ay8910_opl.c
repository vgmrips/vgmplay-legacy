#include "mamedef.h"
#include "math.h"

void OPL_RegMapper(UINT16 Reg, UINT8 Data);

#define NULL	((void *)0)

#define NUM_CHANNELS 3

/* register id's */
#define AY_AFINE	(0)
#define AY_ACOARSE	(1)
#define AY_BFINE	(2)
#define AY_BCOARSE	(3)
#define AY_CFINE	(4)
#define AY_CCOARSE	(5)
#define AY_NOISEPER	(6)
#define AY_ENABLE	(7)
#define AY_AVOL		(8)
#define AY_BVOL		(9)
#define AY_CVOL		(10)
#define AY_EFINE	(11)
#define AY_ECOARSE	(12)
#define AY_ESHAPE	(13)

#define AY_PORTA	(14)
#define AY_PORTB	(15)

#define NOISE_ENABLEQ(_psg, _chan)	(((_psg)->regs[AY_ENABLE] >> (3 + _chan)) & 1)
#define TONE_ENABLEQ(_psg, _chan)	(((_psg)->regs[AY_ENABLE] >> (_chan)) & 1)
#define TONE_PERIOD(_psg, _chan)	( (_psg)->regs[(_chan) << 1] | (((_psg)->regs[((_chan) << 1) | 1] & 0x0f) << 8) )
#define NOISE_PERIOD(_psg)			( (_psg)->regs[AY_NOISEPER] & 0x1f)
#define TONE_VOLUME(_psg, _chan)	( (_psg)->regs[AY_AVOL + (_chan)] & 0x0f)
//#define TONE_ENVELOPE(_psg, _chan)	(((_psg)->regs[AY_AVOL + (_chan)] >> 4) & (((_psg)->device->type() == AY8914) ? 3 : 1))
#define TONE_ENVELOPE(_psg, _chan)	(((_psg)->regs[AY_AVOL + (_chan)] >> 4) & (((_psg)->chip_type == CHTYPE_AY8914) ? 3 : 1))
#define ENVELOPE_PERIOD(_psg)		(((_psg)->regs[AY_EFINE] | ((_psg)->regs[AY_ECOARSE]<<8)))


typedef struct _ay_ym_param ay_ym_param;
struct _ay_ym_param
{
	double r_up;
	double r_down;
	int    res_count;
	double res[32];
};

typedef struct _ay8910_context_opl ay8910_context_opl;
struct _ay8910_context_opl
{
	UINT32 clock;
	UINT8 regs[0x10];
	//INT32 count[NUM_CHANNELS];
	//UINT8 output[NUM_CHANNELS];
	//UINT8 output_noise;
	//INT32 count_noise;
	//INT32 count_env;
	//INT8 env_step;
	//UINT32 env_volume;
	//UINT8 hold,alternate,attack,holding;
	//INT32 rng;
	//UINT8 env_step_mask;
	/* init parameters ... */
	//int step;
	//int zero_is_off;
	//UINT8 vol_enabled[NUM_CHANNELS];
	const ay_ym_param *par;
	const ay_ym_param *par_env;
	INT32 vol_table[16];
	UINT8 Vol2OPL[16];
	//INT32 env_table[32];
	//INT32 vol3d_table[8*32*32*32];
	//devcb_resolved_read8 portAread;
	//devcb_resolved_read8 portBread;
	//devcb_resolved_write8 portAwrite;
	//devcb_resolved_write8 portBwrite;
	//UINT32 MuteMsk[NUM_CHANNELS];
	
	UINT8 chip_type;
	
	UINT8 LastVol[3];
	UINT16 LastFreq[4];
	UINT16 OPLFreq[4];
	UINT8 OPLVol[3];
	UINT8 Enabled;
};


static const UINT8 REG_LIST[0x0B] =
	{0x20, 0x40, 0x60, 0x80, 0xE0, 0x23, 0x43, 0x63, 0x83, 0xE3, 0xC0};
// writing Reg 83 Data #6 makes a smooth fading, if you break playing
// actually the release time could be everything - it wouldn't change anything
static const UINT8 SQUARE_FM_INS_OPL[0x0B] =
	{0x02, 0x18, 0xFF, 0x00, 0x02, 0x01, 0x00, 0xF0, 0xF6, 0x00, 0x00};	// OPL2/OPL3
static const UINT8 SQUARE_FM_INS_OPL3[0x0B] =
	{0x01, 0x3F, 0xFF, 0xFF, 0x00, 0x01, 0x00, 0xF0, 0xF6, 0x06, 0x01};	// OPL3 only
// OPL3 has a SquareWave Waveform (OPL2 has only SineWaves)
//static const UINT8 NOISE_FM_INS[0x0A] =
//	{0x0F, 0x00, 0xF0, 0xFF, 0x00, 0x01, 0x00, 0xF0, 0xF6, 0x00};

/* Volume Table (4-bit AY8910 -> 6-bit OPL Conversion)
	AY8910		OPL
	Idx	Vol		Idx	Vol
	??	??		??	??
*/
/*static const UINT8 VOL_OPL[0x10] =
	{0x3F, 0x38, 0x34, 0x20, 0x2C, 0x28, 0x24, 0x20,
	 0x1C, 0x18, 0x14, 0x10, 0x0C, 0x08, 0x04, 0x00};*/


extern UINT8 OPL_MODE;
#define MAX_CHIPS	0x02
static ay8910_context_opl AY8910Data[MAX_CHIPS];

static const ay_ym_param ym2149_param =
{
	630, 801,
	16,
	{ 73770, 37586, 27458, 21451, 15864, 12371, 8922,  6796,
	   4763,  3521,  2403,  1737,  1123,   762,  438,   251 },
};

static const ay_ym_param ym2149_param_env =
{
	630, 801,
	32,
	{ 103350, 73770, 52657, 37586, 32125, 27458, 24269, 21451,
	   18447, 15864, 14009, 12371, 10506,  8922,  7787,  6796,
	    5689,  4763,  4095,  3521,  2909,  2403,  2043,  1737,
	    1397,  1123,   925,   762,   578,   438,   332,   251 },
};

static const ay_ym_param ay8910_param =
{
	800000, 8000000,
	16,
	{ 15950, 15350, 15090, 14760, 14275, 13620, 12890, 11370,
	  10600,  8590,  7190,  5985,  4820,  3945,  3017,  2345 }
};

static void SendVolume(ay8910_context_opl* chip, UINT8 Channel)
{
	UINT8 Volume;
	
	//if (Channel >= 0x04)
	//	Channel &= 0x03;
	
	Volume = TONE_VOLUME(chip, Channel);
	if (Volume == chip->LastVol[Channel])
		return;
	else
		chip->LastVol[Channel] = Volume;
	
	if (! (Volume & 0x10))
	{
		chip->OPLVol[Channel] = chip->Vol2OPL[Volume];
	}
	else
	{
		// TODO: Handle hardware envelope shapes correctly
		chip->OPLVol[Channel] = 0x00;
	}
	
	if (chip->Enabled & (1 << Channel))
		OPL_RegMapper(0x43 + Channel, 0x00 | 0x3F);
	else	// 0 - enable
		OPL_RegMapper(0x43 + Channel, 0x00 | chip->OPLVol[Channel]);
	
	return;
}

static void SendFrequency(ay8910_context_opl* chip, UINT8 Channel)
{
	const double OPL_CHIP_RATE = 3579545.0 / 72.0;
	
	UINT16 Period;
	double FreqVal;
	INT16 FNum;
	INT8 BlockVal;
	
	//Channel &= 0x03;
	Period = TONE_PERIOD(chip, Channel);
	if (Period == chip->LastFreq[Channel])
		return;
	else
		chip->LastFreq[Channel] = Period;
	
	if (Period)
		FreqVal = (chip->clock / 16.0) / Period;
	else
		FreqVal = 0.0;
	if (FreqVal > OPL_CHIP_RATE)
		FreqVal = 0.0;
	
	BlockVal = (UINT8)(0x05 + (log(FreqVal) - log(440.0)) / log(2.0));
	if (BlockVal < 0x00)
	{
		BlockVal = 0x00;
		FNum = 0x000;
	}
	else if (BlockVal > 0x07)
	{
		BlockVal = 0x07;
		FNum = 0x3FF;
	}
	else
	{
		FNum = (UINT16)(FreqVal * (1 << (20 - BlockVal)) / OPL_CHIP_RATE + 0.5);
		if (FNum < 0x000)
			FNum = 0x000;
		else if (FNum > 0x3FF)
			FNum = 0x3FF;
	}
	
	chip->OPLFreq[Channel] = (BlockVal << 10) | (FNum << 0);
	
	OPL_RegMapper(0xA0 | Channel, chip->OPLFreq[Channel] & 0x00FF);
	/*if (chip->Enabled & (1 << Channel))
		OPL_RegMapper(0xB0 | Channel, 0x00 | (chip->OPLFreq[Channel] >> 8));
	else	// 0 - enable*/
		OPL_RegMapper(0xB0 | Channel, 0x20 | (chip->OPLFreq[Channel] >> 8));
	
	return;
}

void ay8910_write_opl(UINT8 ChipID, UINT8 r, UINT8 v)
{
	ay8910_context_opl* chip = &AY8910Data[ChipID];
	UINT8 CurChn;
	UINT8 EnDiff;
	
	chip->regs[r] = v;

	switch(r)
	{
	case AY_AFINE:
	case AY_ACOARSE:
	case AY_BFINE:
	case AY_BCOARSE:
	case AY_CFINE:
	case AY_CCOARSE:
		SendFrequency(chip, r >> 1);
		break;
	case AY_NOISEPER:
		break;
	case AY_AVOL:
	case AY_BVOL:
	case AY_CVOL:
		SendVolume(chip, r - AY_AVOL);
		break;
	case AY_EFINE:
	case AY_ECOARSE:
		break;
	case AY_ENABLE:
		EnDiff = chip->Enabled ^ v;
		for (CurChn = 0x00; CurChn < 0x03; CurChn ++)
		{
			if (EnDiff & (1 << CurChn))
			{
				// using the Key On bit sounds horrible, if it's disabled and enabled a lot
				// (see Final Fantasy MSX: Chaos Temple
				/*if (v & (1 << CurChn))
					OPL_RegMapper(0xB0 | CurChn, 0x00 | (chip->OPLFreq[CurChn] >> 8));
				else	// 0 - enable
					OPL_RegMapper(0xB0 | CurChn, 0x20 | (chip->OPLFreq[CurChn] >> 8));*/
				if (v & (1 << CurChn))
					OPL_RegMapper(0x43 + CurChn, 0x00 | 0x3F);
				else	// 0 - enable
					OPL_RegMapper(0x43 + CurChn, 0x00 | chip->OPLVol[CurChn]);
			}
		}
		chip->Enabled = v;
		break;
	case AY_ESHAPE:
		break;
	case AY_PORTA:
	case AY_PORTB:
		break;
	}
	
	return;
}

INLINE void build_single_table(double rl, const ay_ym_param *par, INT32 *tab, UINT8* OPLtab)
{
	int j;
	double rt, rw = 0;
	double temp[32], min=10.0, max=0.0;
	
	for (j=0; j < par->res_count; j++)
	{
		rt = 1.0 / par->r_down + 1.0 / rl;
		
		rw = 1.0 / par->res[j];
		rt += 1.0 / par->res[j];
		
		rw += 1.0 / par->r_up;
		rt += 1.0 / par->r_up;
		
		temp[j] = rw / rt;
		if (temp[j] < min)
			min = temp[j];
		if (temp[j] > max)
			max = temp[j];
	}
	for (j = 0; j < par->res_count; j ++)
	{
		tab[j] = (INT32)(0x10000 * temp[j]);
		rt = (temp[j] - min) / (max - min);
		rt = -8.0 * log(temp[j]) / log(2.0);
		OPLtab[j] = (UINT8)rt;
	}
	
	return;
}

static void ay8910_start_opl(int clock, ay8910_context_opl* chip, UINT8 chip_type)
{
	const UINT8* SQUARE_FM_INS;
	UINT8 i;
	UINT8 reg;
	
	chip->clock = clock;
	chip->chip_type = chip_type;
	if ((chip_type & 0xF0) == 0x00)	// CHTYPE_AY89xx variants
	{
	//	chip->step = 2;
		chip->par = &ay8910_param;
		chip->par_env = &ay8910_param;
		//chip->zero_is_off = 0;
	//	chip->env_step_mask = 0x0F;
	}
	else //if ((chip_type & 0xF0) == 0x10)	// CHTYPE_YMxxxx variants (also YM2203/2608/2610)
	{
	//	chip->step = 1;
		chip->par = &ym2149_param;
		chip->par_env = &ym2149_param_env;
		//chip->zero_is_off = 0;
	//	chip->env_step_mask = 0x1F;
		
		/*// YM2149 master clock divider?
		if (info->intf->flags & YM2149_PIN26_LOW)
			master_clock /= 2;*/
	}
	
	for (i = 0; i < 3; i ++)
	{
		build_single_table(1000, chip->par, chip->vol_table, chip->Vol2OPL);
		//build_single_table(1000, chip->par_env, psg->env_table, NULL);
	}
	
	for (i = 0x00; i < 0x10; i ++)
		chip->regs[i] = 0x00;
	
	if (OPL_MODE == 0x03)
		SQUARE_FM_INS = SQUARE_FM_INS_OPL3;
	else
		SQUARE_FM_INS = SQUARE_FM_INS_OPL;
	// Init Instruments
	for (i = 0; i < 3; i++)
	{
		for (reg = 0x00; reg < 0x0A; reg ++)
			OPL_RegMapper(REG_LIST[reg] + i, SQUARE_FM_INS[reg]);
		OPL_RegMapper(REG_LIST[0x0A] + i, 0x30 | SQUARE_FM_INS[0x0A]);
	}
	
	chip->Enabled = 0xFF;
	for (i = 0; i < 3; i ++)
	{
		chip->LastVol[i] = 0xFF;
		chip->LastFreq[i] = 0xFFFF;
		chip->OPLFreq[i] = 0x0000;
		SendVolume(chip, i);
		//SendFrequency(chip, i);
		OPL_RegMapper(0xB0 | i, 0x00);
	}
	
	return;
}


void start_ay8910_opl(UINT8 ChipID, int clock, UINT8 chip_type)
{
	ay8910_context_opl* chip;

	if (ChipID >= MAX_CHIPS)
		return;
	
	chip = &AY8910Data[ChipID];
	ay8910_start_opl(clock & 0x7FFFFFFF, chip, chip_type);
	
	return;
}
