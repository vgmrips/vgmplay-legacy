#include "mamedef.h"

void OPL_RegMapper(UINT16 Reg, UINT8 Data);

/* register number to channel number , slot offset */
#define SLOT1 0
#define SLOT2 1

typedef struct{
	UINT32	ar;			/* attack rate                  */
	UINT32	dr;			/* decay rate                   */
	UINT32	rr;			/* release rate                 */
	UINT8	KSR;		/* key scale rate               */
	UINT8	ksl;		/* keyscale level               */
	UINT8	mul;		/* multiple: mul_tab[ML]        */

	/* Phase Generator */
	//UINT32	phase;		/* frequency counter            */
	//UINT32	freq;		/* frequency counter step       */
	UINT8   fb_shift;	/* feedback shift value         */
	//INT32   op1_out[2];	/* slot1 output for feedback    */

	/* Envelope Generator */
	UINT8	eg_type;	/* percussive/nonpercussive mode*/
	//UINT8	state;		/* phase type                   */
	UINT32	TL;			/* total level                  */
	//INT32	volume;		/* envelope counter             */
	UINT32	sl;			/* sustain level: sl_tab[SL]    */

	//UINT32	key;		/* 0 = KEY OFF, >0 = KEY ON     */

	/* LFO */
	UINT32	AMmask;		/* LFO Amplitude Modulation enable mask */
	UINT8	vib;		/* LFO Phase Modulation enable flag (active high)*/

	/* waveform select */
	unsigned int wavetable;
} OPLL_SLOT_OPL;

typedef struct{
	OPLL_SLOT_OPL SLOT[2];
	/* phase generator state */
	//UINT32  block_fnum;	/* block+fnum                   */
	UINT8   fnum_lsb;
	UINT8   fnum_lmsb;
	UINT8   fnum_msb;
	UINT8   block;
	UINT8   keyon;
	UINT8   sus;		/* sus on/off (release speed in percussive mode)*/
	UINT8   fnumlsb_null;
} OPLL_CH_OPL;

/* chip state */
 typedef struct {
	OPLL_CH_OPL	P_CH[9];				/* OPLL chips have 9 channels*/
	UINT8	instvol_r[9];			/* instrument/volume (or volume/volume in percussive mode)*/
	UINT8	rhythm;					/* Rhythm mode                  */

/* instrument settings */
/*
    0-user instrument
    1-15 - fixed instruments
    16 -bass drum settings
    17,18 - other percussion instruments
*/
	UINT8 inst_tab[19][8];

	UINT8 address;					/* address register             */
	UINT8 status;					/* status flag                  */
} YM2413;

static const unsigned char table[19][8] = {
/* MULT  MULT modTL DcDmFb AR/DR AR/DR SL/RR SL/RR */
/*   0     1     2     3     4     5     6    7    */
  {0x49, 0x4c, 0x4c, 0x12, 0x00, 0x00, 0x00, 0x00 },	//0

  {0x61, 0x61, 0x1e, 0x17, 0xf0, 0x78, 0x00, 0x17 },	//1
  {0x13, 0x41, 0x1e, 0x0d, 0xd7, 0xf7, 0x13, 0x13 },	//2
  {0x13, 0x01, 0x99, 0x04, 0xf2, 0xf4, 0x11, 0x23 },	//3
  {0x21, 0x61, 0x1b, 0x07, 0xaf, 0x64, 0x40, 0x27 },	//4

//{0x22, 0x21, 0x1e, 0x09, 0xf0, 0x76, 0x08, 0x28 },    //5
  {0x22, 0x21, 0x1e, 0x06, 0xf0, 0x75, 0x08, 0x18 },	//5

//{0x31, 0x22, 0x16, 0x09, 0x90, 0x7f, 0x00, 0x08 },    //6
  {0x31, 0x22, 0x16, 0x05, 0x90, 0x71, 0x00, 0x13 },	//6

  {0x21, 0x61, 0x1d, 0x07, 0x82, 0x80, 0x10, 0x17 },	//7
  {0x23, 0x21, 0x2d, 0x16, 0xc0, 0x70, 0x07, 0x07 },	//8
  {0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17 },	//9

//{0x61, 0x61, 0x0c, 0x08, 0x85, 0xa0, 0x79, 0x07 },    //A
  {0x61, 0x61, 0x0c, 0x18, 0x85, 0xf0, 0x70, 0x07 },	//A

  {0x23, 0x01, 0x07, 0x11, 0xf0, 0xa4, 0x00, 0x22 },	//B
  {0x97, 0xc1, 0x24, 0x07, 0xff, 0xf8, 0x22, 0x12 },	//C

//{0x61, 0x10, 0x0c, 0x08, 0xf2, 0xc4, 0x40, 0xc8 },    //D
  {0x61, 0x10, 0x0c, 0x05, 0xf2, 0xf4, 0x40, 0x44 },	//D

  {0x01, 0x01, 0x55, 0x03, 0xf3, 0x92, 0xf3, 0xf3 },	//E
  {0x61, 0x41, 0x89, 0x03, 0xf1, 0xf4, 0xf0, 0x13 },	//F

/* drum instruments definitions */
/* MULTI MULTI modTL  xxx  AR/DR AR/DR SL/RR SL/RR */
/*   0     1     2     3     4     5     6    7    */
  {0x01, 0x01, 0x16, 0x00, 0xfd, 0xf8, 0x2f, 0x6d },/* BD(multi verified, modTL verified, mod env - verified(close), carr. env verifed) */
  {0x01, 0x01, 0x00, 0x00, 0xd8, 0xd8, 0xf9, 0xf8 },/* HH(multi verified), SD(multi not used) */
  {0x05, 0x01, 0x00, 0x00, 0xf8, 0xba, 0x49, 0x55 },/* TOM(multi,env verified), TOP CYM(multi verified, env verified) */
};

static const unsigned char SLOT2OPL[6 * 3] = 
{	0x00, 0x03, 0x01, 0x04, 0x02, 0x05,
	0x08, 0x0B, 0x09, 0x0C, 0x0A, 0x0D,
	0x10, 0x13, 0x11, 0x14, 0x12, 0x15};


#define MAX_CHIPS	0x10
static YM2413 YM2413Data[MAX_CHIPS];


/* set multi,am,vib,EG-TYP,KSR,mul */
INLINE void set_mul(YM2413 *chip,int slot,int v)
{
	OPLL_CH_OPL   *CH   = &chip->P_CH[slot/2];
	OPLL_SLOT_OPL *SLOT = &CH->SLOT[slot&1];

	SLOT->mul     = v & 0x0F;
	SLOT->KSR     = v & 0x10;
	SLOT->eg_type = v & 0x20;
	SLOT->vib     = v & 0x40;
	SLOT->AMmask  = v & 0x80;
	
	OPL_RegMapper(0x20 | SLOT2OPL[slot], 
					SLOT->AMmask | SLOT->vib | SLOT->eg_type | SLOT->KSR | SLOT->mul);
}

/* set ksl, tl */
INLINE void set_ksl_tl(YM2413 *chip,int chan,int v)
{
	OPLL_CH_OPL   *CH   = &chip->P_CH[chan];
	OPLL_SLOT_OPL *SLOT = &CH->SLOT[SLOT1];
	
/* modulator */
	// OPLL KSL (0/1.5/3/6) -> OPL KSL (0/3/1.5/6)
	SLOT->ksl = ((v & 0x40) << 1) | ((v & 0x80) >> 1);	// swap bits 6<->7
	SLOT->TL = v & 0x3F;
	
	OPL_RegMapper(0x40 | SLOT2OPL[chan * 2 + SLOT1], SLOT->ksl | SLOT->TL);
}

/* set ksl , waveforms, feedback */
INLINE void set_ksl_wave_fb(YM2413 *chip,int chan,int v)
{
	OPLL_CH_OPL   *CH   = &chip->P_CH[chan];
	OPLL_SLOT_OPL *SLOT;
	
/* modulator */
	SLOT = &CH->SLOT[SLOT1];
	SLOT->wavetable = (v & 0x08) >> 3;
	SLOT->fb_shift  = (v & 0x07) << 1;
	
	OPL_RegMapper(0xE0 | SLOT2OPL[chan * 2 + SLOT1], SLOT->wavetable);
	OPL_RegMapper(0xC0 | chan, SLOT->fb_shift | 0x30);
	
/*carrier*/
	SLOT = &CH->SLOT[SLOT2];
	SLOT->ksl = v & 0xC0;
	SLOT->wavetable = (v & 0x10) >> 4;
	
	OPL_RegMapper(0xE0 | SLOT2OPL[chan * 2 + SLOT2], SLOT->wavetable);
	OPL_RegMapper(0x40 | SLOT2OPL[chan * 2 + SLOT2], SLOT->ksl | SLOT->TL);
}

/* set attack rate & decay rate  */
INLINE void set_ar_dr(YM2413 *chip,int slot,int v)
{
	OPLL_CH_OPL   *CH   = &chip->P_CH[slot/2];
	OPLL_SLOT_OPL *SLOT = &CH->SLOT[slot&1];
	
	SLOT->ar = v & 0xF0;
	SLOT->dr = v & 0x0F;
	
	OPL_RegMapper(0x60 | SLOT2OPL[slot], SLOT->ar | SLOT->dr);
}

/* set sustain level & release rate */
INLINE void set_sl_rr(YM2413 *chip,int slot,int v)
{
	OPLL_CH_OPL   *CH   = &chip->P_CH[slot/2];
	OPLL_SLOT_OPL *SLOT = &CH->SLOT[slot&1];
	
	SLOT->sl  = v & 0xF0;
	SLOT->rr  = v & 0x0F;
	
	OPL_RegMapper(0x80 | SLOT2OPL[slot], SLOT->sl | SLOT->rr);
}

static void load_instrument(YM2413 *chip, UINT32 chan, UINT32 slot, UINT8* inst )
{
	set_mul			(chip, slot,   inst[0]);
	set_mul			(chip, slot+1, inst[1]);
	set_ksl_tl		(chip, chan,   inst[2]);
	//set_ksl_wave_fb	(chip, chan,   inst[3]);
	set_ar_dr		(chip, slot,   inst[4]);
	set_ar_dr		(chip, slot+1, inst[5]);
	set_sl_rr		(chip, slot,   inst[6]);
	set_sl_rr		(chip, slot+1, inst[7]);
	set_ksl_wave_fb	(chip, chan,   inst[3]);	// called last to avoid a 'click'
}
static void update_instrument_zero(YM2413 *chip, UINT8 r )
{
	UINT8* inst = &chip->inst_tab[0][0]; /* point to user instrument */
	UINT32 chan;
	UINT32 chan_max;

	chan_max = 9;
	if (chip->rhythm & 0x20)
		chan_max=6;

	switch(r)
	{
	case 0:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_mul			(chip, chan*2, inst[0]);
			}
		}
        break;
	case 1:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_mul			(chip, chan*2+1,inst[1]);
			}
		}
        break;
	case 2:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ksl_tl		(chip, chan,   inst[2]);
			}
		}
        break;
	case 3:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ksl_wave_fb	(chip, chan,   inst[3]);
			}
		}
        break;
	case 4:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ar_dr		(chip, chan*2, inst[4]);
			}
		}
        break;
	case 5:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ar_dr		(chip, chan*2+1,inst[5]);
			}
		}
        break;
	case 6:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_sl_rr		(chip, chan*2, inst[6]);
			}
		}
        break;
	case 7:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_sl_rr		(chip, chan*2+1,inst[7]);
			}
		}
        break;
    }
}

/* write a value v to register r on chip chip */
static void OPLLWriteReg2OPL(YM2413 *chip, int r, int v)
{
	OPLL_CH_OPL *CH;
	OPLL_SLOT_OPL *SLOT;
	UINT8 *inst;
	int chan;
	int slot;
	UINT8 fnln_old;

	/* adjust bus to 8 bits */
	r &= 0xff;
	v &= 0xff;

	switch(r&0xf0)
	{
	case 0x00:	/* 00-0f:control */
	{
		switch(r&0x0f)
		{
		case 0x00:	/* AM/VIB/EGTYP/KSR/MULTI (modulator) */
		case 0x01:	/* AM/VIB/EGTYP/KSR/MULTI (carrier) */
		case 0x02:	/* Key Scale Level, Total Level (modulator) */
		case 0x03:	/* Key Scale Level, carrier waveform, modulator waveform, Feedback */
		case 0x04:	/* Attack, Decay (modulator) */
		case 0x05:	/* Attack, Decay (carrier) */
		case 0x06:	/* Sustain, Release (modulator) */
		case 0x07:	/* Sustain, Release (carrier) */
			chip->inst_tab[0][r & 0x07] = v;
			update_instrument_zero(chip,r&7);
		break;

		case 0x0e:	/* x, x, r,bd,sd,tom,tc,hh */
		{
			if(v&0x20)
			{
				if ((chip->rhythm&0x20)==0)
				/*rhythm off to on*/
				{
					//logerror("YM2413: Rhythm mode enable\n");

	/* Load instrument settings for channel seven(chan=6 since we're zero based). (Bass drum) */
					chan = 6;
					inst = &chip->inst_tab[16][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

	/* Load instrument settings for channel eight. (High hat and snare drum) */
					chan = 7;
					inst = &chip->inst_tab[17][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

					CH   = &chip->P_CH[chan];
					SLOT = &CH->SLOT[SLOT1]; /* modulator envelope is HH */
					SLOT->TL  = (chip->instvol_r[chan] >> 4) << 2; /* 7 bits TL (bit 6 = always 0) */
					OPL_RegMapper(0x40 | SLOT2OPL[slot], SLOT->ksl | SLOT->TL);

	/* Load instrument settings for channel nine. (Tom-tom and top cymbal) */
					chan = 8;
					inst = &chip->inst_tab[18][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

					CH   = &chip->P_CH[chan];
					SLOT = &CH->SLOT[SLOT1]; /* modulator envelope is TOM */
					SLOT->TL  = (chip->instvol_r[chan] >> 4) << 2; /* 7 bits TL (bit 6 = always 0) */
					OPL_RegMapper(0x40 | SLOT2OPL[slot], SLOT->ksl | SLOT->TL);
				}
			}
			else
			{
				if (chip->rhythm&0x20)
				/*rhythm on to off*/
				{
					//logerror("YM2413: Rhythm mode disable\n");
	/* Load instrument settings for channel seven(chan=6 since we're zero based).*/
					chan = 6;
					inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

	/* Load instrument settings for channel eight.*/
					chan = 7;
					inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

	/* Load instrument settings for channel nine.*/
					chan = 8;
					inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);
				}
			}
			chip->rhythm  = v&0x3f;
			OPL_RegMapper(0xBD, v & 0xFF);
		}
		break;
		}
	}
	break;

	case 0x10:
	case 0x20:
	{
		chan = r&0x0f;

		if (chan >= 9)
			chan -= 9;	/* verified on real YM2413 */

		CH = &chip->P_CH[chan];

		// ((FM_OPL_Regs[0x10] & 0x00ff) | ((FM_OPL_Regs[0x20] & 0x01) << 8)) << 1;
		if(r&0x10)
		{	/* 10-18: FNUM 0-7 */
			CH->fnum_lsb = (v & 0x7F) << 1;
			if (! CH->fnumlsb_null)
				OPL_RegMapper(0xA0 | chan, CH->fnum_lsb);
			if (CH->fnum_lmsb != ((v & 0x80) >> 7))
			{
				CH->fnum_lmsb = (v & 0x80) >> 7;
				OPL_RegMapper(0xB0 | chan, CH->fnum_lmsb | CH->fnum_msb |
								CH->block | CH->keyon | CH->sus);
			}
		}
		else
		{	/* 20-28: suson, keyon, block, FNUM 8 */
			//block_fnum = ((v&0x0f)<<8) | (CH->block_fnum&0xff);
			CH->fnum_msb = (v & 0x01) << 1;
			CH->block = (v & 0x0E) << 1;
			CH->keyon = (v & 0x10) << 1;
			
			// fixes a behaviour that causes a rumble because of Reg A0 being not 0
			fnln_old = CH->fnumlsb_null;
			CH->fnumlsb_null = (! CH->fnum_msb && ! CH->block);
			if (CH->fnumlsb_null != fnln_old)
			{
				OPL_RegMapper(0xA0 | chan, CH->fnumlsb_null ? 0x00 : CH->fnum_lsb);
			}

			CH->sus = (v & 0x20) << 1;	// actually n/a on real OPL
			
			OPL_RegMapper(0xB0 | chan, CH->fnum_lmsb | CH->fnum_msb |
							CH->block | CH->keyon | CH->sus);
		}
	}
	break;

	case 0x30:	/* inst 4 MSBs, VOL 4 LSBs */
	{
		UINT8 old_instvol;

		chan = r&0x0f;

		if (chan >= 9)
			chan -= 9;	/* verified on real YM2413 */

		old_instvol = chip->instvol_r[chan];
		chip->instvol_r[chan] = v;	/* store for later use */

		CH   = &chip->P_CH[chan];
		SLOT = &CH->SLOT[SLOT2]; /* carrier */
		SLOT->TL  = (chip->instvol_r[chan] & 0x0F) << 2;
		//SLOT->TL  = (chip->instvol_r[chan] & 0x0F) << 0;
		OPL_RegMapper(0x40 | SLOT2OPL[chan * 2 + SLOT2], SLOT->ksl | SLOT->TL);

		/*check wether we are in rhythm mode and handle instrument/volume register accordingly*/
		if ((chan>=6) && (chip->rhythm&0x20))
		{
			/* we're in rhythm mode*/

			if (chan>=7) /* only for channel 7 and 8 (channel 6 is handled in usual way)*/
			{
				SLOT = &CH->SLOT[SLOT1]; /* modulator envelope is HH(chan=7) or TOM(chan=8) */
				SLOT->TL  = (chip->instvol_r[chan] >> 4) << 2;
				OPL_RegMapper(0x40 | SLOT2OPL[chan * 2 + SLOT1], SLOT->ksl | SLOT->TL);
			}
		}
		else
		{
			if ( (old_instvol&0xf0) == (v&0xf0) )
				return;

			inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
			slot = chan*2;

			load_instrument(chip, chan, slot, inst);

		/*#if 0
			logerror("YM2413: chan#%02i inst=%02i:  (r=%2x, v=%2x)\n",chan,v>>4,r,v);
			logerror("  0:%2x  1:%2x\n",inst[0],inst[1]);	logerror("  2:%2x  3:%2x\n",inst[2],inst[3]);
			logerror("  4:%2x  5:%2x\n",inst[4],inst[5]);	logerror("  6:%2x  7:%2x\n",inst[6],inst[7]);
		#endif*/
		}
	}
	break;

	default:
	break;
	}
}

static void OPLLWrite(YM2413 *chip,int a,int v)
{
	if( !(a&1) )
	{	/* address port */
		chip->address = v & 0xff;
	}
	else
	{	/* data port */
		OPLLWriteReg2OPL(chip,chip->address,v);
	}
}

static void OPLLResetChip(YM2413 *chip)
{
	int c,s;
	int i;

	// setup instruments table
	for (i=0; i<19; i++)
	{
		for (c=0; c<8; c++)
		{
			chip->inst_tab[i][c] = table[i][c];
		}
	}


	// reset with register write
	OPLLWriteReg2OPL(chip,0x0f,0); //test reg
	for(i = 0x3f ; i >= 0x10 ; i-- )
		OPLLWriteReg2OPL(chip,i,0x00);

	chip->rhythm = 0;
	OPLLWriteReg2OPL(chip,0x0E,0x00);
	// reset operator parameters
	for( c = 0 ; c < 9 ; c++ )
	{
		OPLL_CH_OPL *CH = &chip->P_CH[c];
		
		chip->instvol_r[c] = 0;
		
		CH->fnum_lsb = 0;
		CH->fnum_msb = 0;
		CH->block = 0;
		CH->keyon = 0;
		CH->sus = 0;
		CH->fnumlsb_null = 0;
		for(s = 0 ; s < 2 ; s++ )
		{
			CH->SLOT[s].ar = 0;
			CH->SLOT[s].dr = 0;
			CH->SLOT[s].rr = 0;
			CH->SLOT[s].KSR = 0;
			CH->SLOT[s].ksl = 0;
			CH->SLOT[s].mul = 0;
			
			CH->SLOT[s].fb_shift = 0;
			
			CH->SLOT[s].eg_type = 0;
			CH->SLOT[s].TL = 0;
			CH->SLOT[s].sl = 0;
			
			CH->SLOT[s].AMmask = 0;
			CH->SLOT[s].vib = 0;
			
			// wave table
			CH->SLOT[s].wavetable = 0;
		}
	}
}

void start_ym2413_opl(UINT8 ChipID)
{
	YM2413 *chip;
	
	if (ChipID >= MAX_CHIPS)
		return;
	
	chip = &YM2413Data[ChipID];
	
	OPLLResetChip(chip);
	
	return;
}

void ym2413_w_opl(UINT8 ChipID, offs_t offset, UINT8 data)
{
	YM2413 *chip = &YM2413Data[ChipID];
	OPLLWrite(chip, offset & 1, data);
}
