#include "mamedef.h"
#include "math.h"

void OPL_RegMapper(UINT16 Reg, UINT8 Data);

#define NULL	((void *)0)

typedef struct _sn76496_state_opl sn76496_state_opl;
struct _sn76496_state_opl
{
	int clock;
	//sound_stream * Channel;
	//INT32 VolTable[16];	/* volume table (for 4-bit to db conversion)*/
	INT32 Register[8];	/* registers */
	INT32 LastRegister;	/* last register written */
	//INT32 Volume[4];	/* db volume of voice 0-2 and noise */
	UINT8 Volume[4];	/* native volume of voice 0-2 and noise */
	//UINT32 RNG;			/* noise generator LFSR*/
	//INT32 FeedbackMask;	/* mask for feedback */
	//INT32 WhitenoiseTaps;	/* mask for white noise taps */
	//INT32 FeedbackInvert;	/* feedback invert flag (xor vs xnor) */
	//INT32 Negate;		/* output negate flag */
	INT32 Stereo;		/* whether we're dealing with stereo or not */
	UINT8 StereoMask;	/* the stereo output mask */
	INT32 Period[4];	/* Length of 1/2 of waveform */
	//INT32 Count[4];		/* Position within the waveform */
	//INT32 Output[4];	/* 1-bit output of each channel, pre-volume */
	//INT32 CyclestoREADY;/* number of cycles until the READY line goes active */
	unsigned char NgpFlags;	// bit 7 - NGP Mode on/off, bit 0 - is 2nd NGP chip
	sn76496_state_opl* NgpChip2;	// Pointer to other Chip
	
	UINT8 LastVol[4];
	UINT8 RegC0_M[4];
	UINT8 RegC0_L[4];
};

#define NOISEMODE (R->Register[6]&4)?1:0

const unsigned char REG_LIST[0x0B] =
	{0x20, 0x40, 0x60, 0x80, 0xE0, 0x23, 0x43, 0x63, 0x83, 0xE3, 0xC0};
// writing Reg 83 Data #6 makes a smooth fading, if you break playing
// actually the release time could be everything - it wouldn't change anything
const unsigned char SQUARE_FM_INS_OPL[0x0B] =
	{0x02, 0x18, 0xFF, 0x00, 0x02, 0x01, 0x00, 0xF0, 0xF6, 0x00, 0x00};	// OPL2/OPL3
const unsigned char SQUARE_FM_INS_OPL3[0x0B] =
	{0x01, 0x3F, 0xFF, 0xFF, 0x00, 0x01, 0x00, 0xF0, 0xF6, 0x06, 0x01};	// OPL3 only
// OPL3 has a SquareWave Waveform (OPL2 has only SineWaves)
const unsigned char NOISE_FM_INS[0x0A] =
	{0x0F, 0x00, 0xF0, 0xFF, 0x00, 0x01, 0x00, 0xF0, 0xF6, 0x00};
const unsigned char NOISE_FM_REG_C0[0x02] =
	{0x0B, 0x0E};	// periodic, white
//	{0x01, 0x0E};

/* Volume Table (4-bit SN76496 -> 6-bit OPL Conversion)
	SN76496		OPL
	Idx	Vol		Idx	Vol
	0	100.0%	00	100.0%
				01	 91.7%
				02	 84.1%
	1	 79.4%	03	 77.1%
				04	 70.7%
	2	 63.1%	05	 64.8%
				06	 59.5%
				07	 54.5%
	3	 50.1%	08	 50.0%
				09	 45.9%
				0A	 42.0%
	4	 39.8%	0B	 38.6%
				0C	 35.4%
	5	 31.6%	0D	 32.4%
				0E	 29.7%
				0F	 27.3%
	6	 25.1%	10	 25.0%
				11	 22.9%
				12	 21.0%
	7	 20.0%	13	 19.3%
				14	 17.7%
	8	 15.8%	15	 16.2%
				16	 14.9%
				17	 13.6%
	9	 12.6%	18	 12.5%
				19	 11.5%
				1A	 10.5%
	A	 10.0%	1B	  9.6%
				1C	  8.8%
	B	  7.9%	1D	  8.1%
				1E	  7.4%
				1F	  6.8%
	C	  6.3%	20	  6.3%
				21	  5.7%
				22	  5.3%
	D	  5.0%	23	  4.8%
				24	  4.4%
	E	  4.0%	25	  4.1%
				26	  3.7%
				27	  3.4%
				28	  3.1%
				29	  2.9%
				2A	  2.6%
				2B	  2.4%
				2C	  2.2%
				2D	  2.0%
				2E	  1.9%
				2F	  1.7%
				30	  1.6%
				31	  1.4%
				32	  1.3%
				33	  1.2%
				34	  1.1%
				35	  1.0%
				36	  0.9%
				37	  0.9%
				38	  0.8%
				39	  0.7%
				3A	  0.7%
				3B	  0.6%
				3C	  0.6%
				3D	  0.5%
				3E	  0.5%
	F	  0.0%	3F	  0.4%
*/
const unsigned char VOL_OPL[0x10] =
	{0x00, 0x03, 0x05, 0x08, 0x0B, 0x0D, 0x10, 0x13,
	 0x15, 0x18, 0x1B, 0x1D, 0x20, 0x23, 0x25, 0x3F};


extern unsigned char OPL_MODE;
#define MAX_CHIPS	0x02
static sn76496_state_opl SN76496Data[MAX_CHIPS];
static unsigned char LastChipInit;

void sn76496_stereo_opl(UINT8 ChipID, offs_t offset, UINT8 data)
{
	sn76496_state_opl *R = &SN76496Data[ChipID];
	unsigned char i;
	unsigned char st_data;
	
	if (! R->Stereo)
		return;
	
	R->StereoMask = data;
	for (i = 0; i < 4; i ++)
	{
		st_data = 0x00;
		st_data |= (R->StereoMask & (0x10 << i)) ? 0x10 : 0x00;	// Left Channel
		st_data |= (R->StereoMask & (0x01 << i)) ? 0x20 : 0x00;	// Right Channel
		R->RegC0_M[i] = st_data;
		OPL_RegMapper(REG_LIST[0x0A] | i, R->RegC0_M[i] | R->RegC0_L[i]);
	}
	
	return;
}

void sn76496_refresh_t6w28_opl(UINT8 ChipID)
{
	sn76496_state_opl *R = &SN76496Data[ChipID];
	sn76496_state_opl *R2 = R->NgpChip2;
	unsigned char CurChn;
	unsigned char Channel;
	
	for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
	{
		Channel = CurChn & 0x03;
		if (! (CurChn & 0x04))
		{
			// Tone Chip: Left Channel
			R->RegC0_M[Channel] = 0x10;
			OPL_RegMapper(REG_LIST[0x0A] | CurChn, R->RegC0_M[Channel] | R->RegC0_L[Channel]);
		}
		else
		{
			// Noise Chip: Right Channel
			R2->RegC0_M[Channel] = 0x20;
			OPL_RegMapper(REG_LIST[0x0A] | CurChn, R2->RegC0_M[Channel] | R2->RegC0_L[Channel]);
		}
	}
	
	return;
}

static void SendVolume(sn76496_state_opl* R, UINT8 Channel)
{
	unsigned char ChnFnl;
	unsigned char ChnOp;
	unsigned char OPLVol;
	
	if (Channel >= 0x04)
		Channel &= 0x03;
	
	if (R->Volume[Channel] == R->LastVol[Channel])
		return;
	else
		R->LastVol[Channel] = R->Volume[Channel];
	
	ChnFnl = ((R->NgpFlags & 0x01) << 2) | Channel;
	
	ChnOp = (ChnFnl / 0x03) * 0x08 + (ChnFnl % 0x03);
	OPLVol = VOL_OPL[R->Volume[Channel]];
	if (Channel == 0x03)
	{
		if (R->RegC0_L[Channel] & 0x01)
			OPL_RegMapper(0x40 | (ChnOp + 0x00), 0x00 | OPLVol);
	}
	OPL_RegMapper(0x40 | (ChnOp + 0x03), 0x00 | OPLVol);
	
	return;
}

static void SendFrequency(sn76496_state_opl* R, UINT8 Channel)
{
	const double OPL_CHIP_RATE = 3579545.0 / 72.0;
	
	double FreqVal;
	signed short int FNum;
	signed char BlockVal;
	sn76496_state_opl* R2;
	unsigned char ChnB;
	
	Channel &= 0x03;
	if (R->NgpFlags & 0x80)
	{
		if ((R->NgpFlags & 0x01) ^ (Channel == 0x03))
			return;
		
		R2 = R->NgpChip2;
		ChnB = Channel | 0x04;
	}
	
	if (R->Period[Channel])
		FreqVal = (R->clock / 32.0) / R->Period[Channel];
	else
		FreqVal = 0.0;
	if (Channel == 0x03)
	{
		R->RegC0_L[Channel] = NOISE_FM_REG_C0[(R->Register[6] >> 2) & 0x01];
		OPL_RegMapper(REG_LIST[0x0A] | Channel, R->RegC0_M[Channel] | R->RegC0_L[Channel]);
		if (R->NgpFlags & 0x80)
		{
			OPL_RegMapper(REG_LIST[0x0A] | ChnB, R2->RegC0_M[Channel] | R->RegC0_L[Channel]);
			FNum = (ChnB / 0x03) * 0x08 + (ChnB % 0x03);
		}
		
		if (! (R->RegC0_L[Channel] & 0x01))
		{
			//FreqVal = 220.0;
			FreqVal /= 8.0;	// too high frequencies sound weird
			// reset Modulator-Volume
			OPL_RegMapper(0x40 | 0x08, NOISE_FM_INS[0x01]);
			if (R->NgpFlags & 0x80)
				OPL_RegMapper(0x40 | FNum, NOISE_FM_INS[0x01]);
		}
		else
		{
			// set Modulator-Volume (because of Additive Synthesis)
			OPL_RegMapper(0x40 | 0x08, 0x00 | VOL_OPL[R->Volume[Channel]]);
			if (R->NgpFlags & 0x80)
				OPL_RegMapper(0x40 | FNum, 0x00 | VOL_OPL[R2->Volume[Channel]]);
		}
	}
	else if (FreqVal > OPL_CHIP_RATE)
	{
		FreqVal = 0.0;
	}
	
	BlockVal = (unsigned char)(0x05 + (log(FreqVal) - log(440.0)) / log(2.0));
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
		FNum = (unsigned short int)(FreqVal * (1 << (20 - BlockVal)) / OPL_CHIP_RATE + 0.5);
		if (FNum < 0x000)
			FNum = 0x000;
		else if (FNum > 0x3FF)
			FNum = 0x3FF;
	}
	
	if (Channel == 0x03)	// stop Noise Note before restarting
		OPL_RegMapper(0xB0 | Channel, 0x00);
	OPL_RegMapper(0xA0 | Channel, FNum & 0x00FF);
	OPL_RegMapper(0xB0 | Channel, 0x20 | (BlockVal << 2) |( (FNum & 0x0300) >> 8));
	if (R->NgpFlags & 0x80)
	{
		// Send 2nd Channel Set
		if (Channel == 0x03)
			OPL_RegMapper(0xB0 | ChnB, 0x00);
		OPL_RegMapper(0xA0 | ChnB, FNum & 0x00FF);
		OPL_RegMapper(0xB0 | ChnB, 0x20 | (BlockVal << 2) |( (FNum & 0x0300) >> 8));
	}
	
	return;
}

void sn76496_write_opl(UINT8 ChipID, offs_t offset, UINT8 data)
{
	sn76496_state_opl *R = &SN76496Data[ChipID];
	unsigned char n, r, c;
	
	if (data & 0x80)
	{
		r = (data & 0x70) >> 4;
		R->LastRegister = r;
		R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
	}
	else
    {
		r = R->LastRegister;
	}
	c = r/2;
	switch (r)
	{
		case 0:	/* tone 0 : frequency */
		case 2:	/* tone 1 : frequency */
		case 4:	/* tone 2 : frequency */
		    if ((data & 0x80) == 0)
				R->Register[r] = (R->Register[r] & 0x0f) | ((data & 0x3f) << 4);
			R->Period[c] = R->Register[r];
		    if (data & 0x80)
				break;	// only send after receiving a Data Command
			SendFrequency(R, c);
			if (r == 4)
			{
				/* update noise shift frequency */
				if ((R->Register[6] & 0x03) == 0x03)
				{
					R->Period[3] = 2 * R->Period[2];
					SendFrequency(R, 3);
				}
			}
			break;
		case 1:	/* tone 0 : volume */
		case 3:	/* tone 1 : volume */
		case 5:	/* tone 2 : volume */
		case 7:	/* noise  : volume */
			//R->Volume[c] = R->VolTable[data & 0x0f];
			R->Volume[c] = data & 0x0F;
			SendVolume(R, c);
			if ((data & 0x80) == 0)
			{
				R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
				// Register is set, but frequency is NOT refreshed
			}
			break;
		case 6:	/* noise  : frequency, mode */
		    if ((data & 0x80) == 0)
				R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
			n = R->Register[6];
			/* N/512,N/1024,N/2048,Tone #3 output */
			R->Period[3] = ((n&3) == 3) ? 2 * R->Period[2] : (1 << (5+(n&3)));
			SendFrequency(R, 3);
			    /* Reset noise shifter */
			//R->RNG = R->FeedbackMask;
			//R->Output[3] = R->RNG & 1;
			break;
	}
}

static void SN76496_init(int clock, sn76496_state_opl *R, int stereo)
{
	const unsigned char* SQUARE_FM_INS;
	unsigned char i;
	unsigned char reg;

	R->NgpFlags = 0x00;
	R->NgpChip2 = NULL;
	R->clock = clock;
	//for (i = 0;i < 4;i++)
	//	R->Volume[i] = 0;

	R->LastRegister = 0;
	for (i = 0;i < 8;i+=2)
	{
		R->Register[i] = 0;
		R->Register[i + 1] = 0x0f;	/* volume = 0 */
	}

	if (OPL_MODE == 0x03)
		SQUARE_FM_INS = SQUARE_FM_INS_OPL3;
	else
		SQUARE_FM_INS = SQUARE_FM_INS_OPL;
	// Init Instruments
	for (i = 0;i < 3;i++)
	{
		for (reg = 0x00; reg < 0x0A; reg ++)
		{
			OPL_RegMapper(REG_LIST[reg] + i, SQUARE_FM_INS[reg]);
		}
		R->RegC0_L[i] = SQUARE_FM_INS[0x0A];
		R->RegC0_M[i] = 0x30;
		OPL_RegMapper(REG_LIST[0x0A] + i, R->RegC0_M[i] | R->RegC0_L[i]);
	}
	for (reg = 0x00; reg < 0x0A; reg ++)
	{
		OPL_RegMapper(REG_LIST[reg] + 0x08, NOISE_FM_INS[reg]);
	}
	R->RegC0_L[3] = NOISE_FM_REG_C0[0x00];
	R->RegC0_M[3] = 0x30;
	OPL_RegMapper(REG_LIST[0x0A] + 0x03, R->RegC0_M[i] | R->RegC0_L[i]);
	
	for (i = 0;i < 4;i++)
	{
		R->LastVol[i] = 0xFF;
		R->Volume[i] = R->Register[i * 2 + 1];
		R->Period[i] = 0;
		SendVolume(R, i);
		OPL_RegMapper(0xB0 | i, 0x00);
		SendFrequency(R, i);
	}
	
	/* Default is SN76489 non-A */
	//R->FeedbackMask = 0x4000;     /* mask for feedback */
	//R->WhitenoiseTaps = 0x03;   /* mask for white noise taps */
	//R->FeedbackInvert = 0; /* feedback invert flag */
	//R->CyclestoREADY = 1; /* assume ready is not active immediately on init. is this correct?*/
	//R->Negate = 0; /* channels are not negated */
	R->Stereo = stereo; /* depends on init */
	R->StereoMask = 0xFF; /* all channels enabled */

	//R->RNG = R->FeedbackMask;
	//R->Output[3] = R->RNG & 1;

	return;
}


void start_sn76496_opl(UINT8 ChipID, int clock, int stereo)
{
	sn76496_state_opl *chip;
	sn76496_state_opl *chip2;

	if (ChipID >= MAX_CHIPS)
		return;
	
	chip = &SN76496Data[ChipID];
	//if (SN76496_init(device,chip,stereo) != 0)
	//	fatalerror("Error creating SN76496 chip");
	SN76496_init(clock & 0x7FFFFFFF, chip, stereo);
	if (clock & 0x80000000)
	{
		// Activate special NeoGeoPocket Mode
		chip2 = &SN76496Data[LastChipInit];
		chip2->NgpFlags = 0x80 | 0x00;
		chip->NgpFlags = 0x80 | 0x01;
		chip->NgpChip2 = chip2;
		chip2->NgpChip2 = chip;
	}
	
	//chip->FeedbackMask = feedbackmask;
	//chip->WhitenoiseTaps = noisetaps;
	//chip->FeedbackInvert = feedbackinvert;
	//chip->Negate = negate;
	chip->Stereo = ! stereo;
	sn76496_stereo_opl(ChipID, 0x00, chip->StereoMask);
	
	return;
}
