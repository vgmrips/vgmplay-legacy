// ChipMapper.c - Handles Chip Write (including OPL Hardware Support)

#include <stdio.h>
#include <memory.h>
#include <math.h>
#include "stdbool.h"

//#define DISABLE_HW_SUPPORT	// disable support for OPL hardware
#ifdef __NetBSD__	// Thanks to nextvolume
#warning "Disabling OPL Mapper functionality, current code does not work on NetBSD"
#define DISABLE_HW_SUPPORT	// Current code does not work on NetBSD
#endif

#ifdef WIN32

#include <conio.h>
#include <windows.h>	// for QueryPerformance###

#else

#ifndef DISABLE_HW_SUPPORT
#include <unistd.h>
#ifdef __APPLE__
#include <architecture/i386/io.h>
#else
#include <sys/io.h>
#endif
#endif	// DISABLE_HW_SUPPORT

#include <time.h>
#endif

#ifdef __APPLE__
#define ioperm(x,y,z)
#define outb(x,y)
#define inb(x)
#endif

#include "chips/mamedef.h"

#include "chips/ChipIncl.h"

#ifndef DISABLE_HW_SUPPORT
unsigned char OpenPortTalk(void);
void ClosePortTalk(void);

#ifdef WIN32

void outportb(unsigned short PortAddress, unsigned char byte);
unsigned char inportb(unsigned short PortAddress);

#endif	// WIN32
#endif	// DISABLE_HW_SUPPORT

#include "ChipMapper.h"

#ifndef DISABLE_HW_SUPPORT
INLINE UINT8 OPL_HW_GetStatus(void);
INLINE void OPL_HW_WaitDelay(INT64 StartTime, float Delay);

// SN76496 OPL Translaton
void start_sn76496_opl(UINT8 ChipID, int clock, int stereo);
void sn76496_write_opl(UINT8 ChipID, offs_t offset, UINT8 data);
void sn76496_stereo_opl(UINT8 ChipID, offs_t offset, UINT8 data);
void sn76496_refresh_t6w28_opl(UINT8 ChipID);

// YM2413 OPL Translaton
void start_ym2413_opl(UINT8 ChipID);
void ym2413_w_opl(UINT8 ChipID, offs_t offset, UINT8 data);

// Meka YM2413 OPL Translation
int     FM_OPL_Init             (void *userdata);
void    FM_OPL_Close            (void);
void    FM_OPL_Write            (int Register, int Value);

// AY8910 OPL Translation
void ay8910_write_opl(UINT8 ChipID, UINT8 r, UINT8 v);
void start_ay8910_opl(UINT8 ChipID, int clock, UINT8 chip_type);


extern UINT8 OPL_MODE;
extern UINT8 OPL_CHIPS;

extern bool WINNT_MODE;
extern UINT16 FMPort;
extern UINT8 PlayingMode;
extern bool FMBreakFade;
extern float FinalVol;

#ifdef WIN32
#define INP_9X		_inp
#define OUTP_9X		_outp
#define INP_NT		inportb
#define OUTP_NT		outportb
#endif

// Delays in usec (Port Reads - or microsec)
#define DELAY_OPL2_REG	 3.3f
#define DELAY_OPL2_DATA	23.0f
#define DELAY_OPL3_REG	 0.0f
#define DELAY_OPL3_DATA	 0.28f
#ifdef WIN32
INT64 HWusTime;
#endif

#define YM2413_EC_DEFAULT	0x00
#define YM2413_EC_MEKA		0x01


typedef struct chip_mapping_info
{
	UINT8 ChipType;
	UINT8 ChipID;
	UINT8 RegBase;
	UINT8 ChnBase;
	UINT32 ChipOpt[0x10];
} CHIP_MAP;

UINT8 ChipCount = 0x00;
UINT8 SelChip;
UINT8 ChipArr[0x100];
CHIP_MAP ChipMap[0x10];
bool RegChnUsage[0x20];
UINT8 OPLReg[0x200];
UINT8 OPLRegBak[0x200];
UINT8 OPLRegForce[0x200];
bool SkipMode = false;
bool OpenedFM = false;
#endif	// DISABLE_HW_SUPPORT
UINT8 YM2413_EMU_CORE;

UINT32 OptArr[0x10];

#ifndef WIN32
unsigned char OpenPortTalk(void)
{
#ifndef DISABLE_HW_SUPPORT
	int retval;
	
	retval = ioperm(FMPort, 0x04, 1);
	
	return retval & 0xFF;
#else
	return 0xFF;	// not supported
#endif
}

void ClosePortTalk(void)
{
#ifndef DISABLE_HW_SUPPORT
	ioperm(FMPort, 0x04, 0);
#endif
	
	return;
}

#elif defined(DISABLE_HW_SUPPORT)

unsigned char OpenPortTalk(void)
{
	return 0xFF;	// not supported
}

void ClosePortTalk(void)
{
	return;
}
#endif	// WIN32

void open_fm_option(UINT8 ChipType, UINT8 OptType, UINT32 OptVal)
{
	OptArr[OptType & 0x0F] = OptVal;
	
	return;
}

void opl_chip_reset(void)
{
#ifndef DISABLE_HW_SUPPORT
	UINT16 Reg;
	float FnlVolBak;
	
	FnlVolBak = FinalVol;
	FinalVol = 1.0f;
	memset(OPLRegForce, 0x01, 0x200);
	
	OPL_HW_WriteReg(0x105, 0x01);	// OPL3 Enable
	OPL_HW_WriteReg(0x001, 0x20);	// Test Register
	OPL_HW_WriteReg(0x002, 0x00);	// Timer 1
	OPL_HW_WriteReg(0x003, 0x00);	// Timer 2
	OPL_HW_WriteReg(0x004, 0x00);	// IRQ Mask Clear
	OPL_HW_WriteReg(0x104, 0x00);	// 4-Op-Mode Disable
	OPL_HW_WriteReg(0x008, 0x00);	// Keyboard Split
	
	// make sure all internal calulations finish sound generation
	for (Reg = 0x00; Reg < 0x09; Reg ++)
	{
		OPL_HW_WriteReg(0x0C0 | Reg, 0x00);	// silence all notes (OPL3)
		OPL_HW_WriteReg(0x1C0 | Reg, 0x00);
	}
	for (Reg = 0x00; Reg < 0x16; Reg ++)
	{
		if ((Reg & 0x07) >= 0x06)
			continue;
		OPL_HW_WriteReg(0x040 | Reg, 0x3F);	// silence all notes (OPL2)
		OPL_HW_WriteReg(0x140 | Reg, 0x3F);
		
		OPL_HW_WriteReg(0x080 | Reg, 0xFF);	// set Sustain/Release Rate to FASTEST
		OPL_HW_WriteReg(0x180 | Reg, 0xFF);
		OPL_HW_WriteReg(0x060 | Reg, 0xFF);
		OPL_HW_WriteReg(0x160 | Reg, 0xFF);
		
		OPL_HW_WriteReg(0x020 | Reg, 0x00);	// NULL the rest
		OPL_HW_WriteReg(0x120 | Reg, 0x00);
		
		OPL_HW_WriteReg(0x0E0 | Reg, 0x00);
		OPL_HW_WriteReg(0x1E0 | Reg, 0x00);
	}
	OPL_HW_WriteReg(0x0BD, 0x00);	// Rhythm Mode
	for (Reg = 0x00; Reg < 0x09; Reg ++)
	{
		OPL_HW_WriteReg(0x0B0 | Reg, 0x00);	// turn all notes off (-> Release Phase)
		OPL_HW_WriteReg(0x1B0 | Reg, 0x00);
		OPL_HW_WriteReg(0x0A0 | Reg, 0x00);
		OPL_HW_WriteReg(0x1A0 | Reg, 0x00);
	}
	
	// although this would be a more proper reset, it sometimes produces clicks
	/*for (Reg = 0x020; Reg <= 0x0FF; Reg ++)
		OPL_HW_WriteReg(Reg, 0x00);
	for (Reg = 0x120; Reg <= 0x1FF; Reg ++)
		OPL_HW_WriteReg(Reg, 0x00);*/
	
	// Now do a proper reset of all other registers.
	for (Reg = 0x040; Reg < 0x0A0; Reg ++)
	{
		if ((Reg & 0x07) >= 0x06 || (Reg & 0x1F) >= 0x18)
			continue;
		OPL_HW_WriteReg(0x000 | Reg, 0x00);
		OPL_HW_WriteReg(0x100 | Reg, 0x00);
	}
	for (Reg = 0x00; Reg < 0x09; Reg ++)
	{
		OPL_HW_WriteReg(0x0C0 | Reg, 0x30);	// must be 30 to make OPL2 VGMs sound on OPL3
		OPL_HW_WriteReg(0x1C0 | Reg, 0x30);	// if they don't send the C0 reg
	}
	
	memset(OPLRegForce, 0x01, 0x200);
	FinalVol = FnlVolBak;
#endif	// DISABLE_HW_SUPPORT
	
	return;
}

void open_real_fm(void)
{
#ifndef DISABLE_HW_SUPPORT
	UINT8 CurChip;
	CHIP_MAP* CurMap;
	UINT8 CurC2;
	bool OldSM;
	
	//SkipMode = false;
	OldSM = SkipMode;
	opl_chip_reset();
	OpenedFM = true;
	
	for (CurChip = 0x00; CurChip < ChipCount; CurChip ++)
	{
		SelChip = CurChip;
		CurMap = ChipMap + SelChip;
		switch(CurMap->ChipType)
		{
		case 0x00:	// SN76496 and T6W28
			if (CurMap->ChipOpt[0x0F] & 0x80000000)
			{
				// Avoid Bugs
				CurMap->RegBase = (CurMap->ChipOpt[0x0F] >> 8) & 0xFF;
				CurMap->ChnBase = (CurMap->ChipOpt[0x0F] >> 0) & 0xFF;
			}
			
			start_sn76496_opl(CurMap->ChipID, CurMap->ChipOpt[0x00], CurMap->ChipOpt[0x05]);
			if (CurMap->ChipOpt[0x00] & 0x80000000)
			{
				// Set up T6W28
				for (CurC2 = 0x00; CurC2 < CurChip; CurC2 ++)
				{
					if (ChipMap[CurC2].ChipType == CurMap->ChipType)
					{
						CurMap->ChipOpt[0x0F] = 0x80000000 |
												(CurMap->RegBase << 8) | (CurMap->ChnBase << 0);
						CurMap->RegBase = ChipMap[CurC2].RegBase;
						CurMap->ChnBase = ChipMap[CurC2].ChnBase;
						sn76496_refresh_t6w28_opl(ChipMap[CurC2].ChipID);
						break;
					}
				}
			}
			break;
		case 0x01:	// YM2413
			switch(YM2413_EMU_CORE)
			{
			case YM2413_EC_DEFAULT:
				start_ym2413_opl(CurMap->ChipID);
				break;
			case YM2413_EC_MEKA:
				FM_OPL_Init(NULL);
				break;
			}
			break;
		case 0x09:	// YM3812
			break;
		case 0x0A:	// YM3526
			break;
		case 0x0B:	// Y8950
			break;
		case 0x0C:	// YMF262
			break;
		case 0x0D:	// YMF278B
			break;
		case 0x12:	// AY8910
			start_ay8910_opl(CurMap->ChipID, CurMap->ChipOpt[0x00], CurMap->ChipOpt[0x01]);
			break;
		}
	}
#endif	// DISABLE_HW_SUPPORT

	return;
}

void setup_real_fm(UINT8 ChipType, UINT8 ChipID)
{
#ifndef DISABLE_HW_SUPPORT
	CHIP_MAP* CurMap;
	UINT8 CurChip;
	UINT8 CurSet;
	UINT8 CurChn;
	bool ExitLoop;
	
	SelChip = ChipCount;
	ChipArr[(ChipType << 1) | (ChipID & 0x01)] = SelChip;
	CurMap = ChipMap + SelChip;
	CurMap->ChipType = ChipType;
	CurMap->ChipID = ChipID & 0x7F;
	
	switch(ChipType)
	{
	case 0x00:	// SN76496 and T6W28
	case 0x12:	// AY8910
		ExitLoop = false;
		for (CurSet = 0x00; CurSet < 0x02; CurSet ++)
		{
			for (CurChn = 0x00; CurChn < 0x09; CurChn ++)
			{
				if (! RegChnUsage[(CurSet << 4) | CurChn])
				{
					ExitLoop = true;
					break;
				}
			}
			if (ExitLoop)
				break;
		}
		CurSet %= 0x02;
		CurChn %= 0x09;
		
		CurMap->RegBase = CurSet;
		CurMap->ChnBase = CurChn;
		memcpy(CurMap->ChipOpt, OptArr, 0x10 * sizeof(UINT32));
		CurMap->ChipOpt[0x0F] = 0x00;
		if (ChipType == 0x00)
		{
			for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
			{
				RegChnUsage[(CurMap->RegBase << 4) | (CurMap->ChnBase + CurChn)] = true;
			}
		}
		else
		{
			for (CurChn = 0x00; CurChn < 0x03; CurChn ++)
			{
				RegChnUsage[(CurMap->RegBase << 4) | (CurMap->ChnBase + CurChn)] = true;
			}
		}
		break;
	case 0x01:	// YM2413
		CurMap->RegBase = 0x00;
		CurMap->ChnBase = 0x00;
		
		for (CurChip = 0x00; CurChip < SelChip; CurChip ++)
		{
			if (ChipMap[CurChip].ChipType == 0x01)
			{
				CurMap->RegBase = 0x01;
				break;
			}
			
			ChipMap[CurChip].RegBase = 0x01;
		}
		for (CurChn = 0x00; CurChn < 0x09; CurChn ++)
		{
			RegChnUsage[(CurMap->RegBase << 4) | CurChn] = true;
		}
		break;
	case 0x09:	// YM3812
		for (CurSet = 0x00; CurSet < 0x02; CurSet ++)
		{
			if (! RegChnUsage[(CurSet << 4) | 0x00])
				break;
		}
		CurSet %= 0x02;
		CurMap->RegBase = CurSet;
		CurMap->ChnBase = 0x00;
		break;
	case 0x0A:	// YM3526
		for (CurSet = 0x00; CurSet < 0x02; CurSet ++)
		{
			if (! RegChnUsage[(CurSet << 4) | 0x00])
				break;
		}
		CurSet %= 0x02;
		CurMap->RegBase = CurSet;
		CurMap->ChnBase = 0x00;
		break;
	case 0x0B:	// Y8950
		for (CurSet = 0x00; CurSet < 0x02; CurSet ++)
		{
			if (! RegChnUsage[(CurSet << 4) | 0x00])
				break;
		}
		CurSet %= 0x02;
		CurMap->RegBase = CurSet;
		CurMap->ChnBase = 0x00;
		break;
	case 0x0C:	// YMF262
		CurMap->RegBase = 0x00;
		CurMap->ChnBase = 0x00;
		break;
	case 0x0D:	// YMF278B
		CurMap->RegBase = 0x00;
		CurMap->ChnBase = 0x00;
		break;
	}
	ChipCount ++;
#endif	// DISABLE_HW_SUPPORT
	
	return;
}

void close_real_fm(void)
{
#ifndef DISABLE_HW_SUPPORT
	UINT8 CurChip;
	UINT8 CurChn;
	UINT8 CurOp;
	UINT16 Reg;
	UINT8 RegOp;
	bool SoftFade;
	
	SoftFade = (FMBreakFade && FinalVol > 0.01f);
	for (CurChip = 0x00; CurChip < 0x02; CurChip ++)
	{
		for (CurChn = 0x00; CurChn < 0x09; CurChn ++)
		{
			// Make sure that the ReleaseRate takes effect ...
			for (CurOp = 0x00; CurOp < 0x02; CurOp ++)
			{
				if (! CurOp && ! (OPLReg[(CurChip << 8) | (0xC0 | CurChn)] & 0x01))
					continue;
				
				RegOp = (CurChn / 0x03) * 0x08 | (CurChn % 0x03) + (CurOp * 0x03);
				Reg = (CurChip << 8) | 0x80 | RegOp;
				if (SoftFade)
				{
					if ((OPLReg[Reg] & 0x0F) < 0x03)	// Force a soft fading
						OPL_HW_WriteReg(Reg, (OPLReg[Reg] & 0xF0) | 0x04);
				}
				else
				{
					// stop sound immediately after Note-Off
					OPL_HW_WriteReg(Reg, (OPLReg[Reg] & 0xF0) | 0x0F);
				}
			}
			
			// ... and turn off all notes.
			if (SoftFade)
			{
				// soft way (turn off and let fade)
				Reg = (CurChip << 8) | (0xB0 | CurChn);
				OPL_HW_WriteReg(Reg, OPLReg[Reg] & ~0x20);
			}
			else
			{
				// hard way (turn off and set frequency to zero)
				Reg = (CurChip << 8) | (0xA0 | CurChn);
				OPL_HW_WriteReg(Reg | 0x00, 0x00);	// A0 - Frequency LSB
				OPL_HW_WriteReg(Reg | 0x10, 0x00);	// B0 - Frequency MSB / Block / Key On
			}
		}
		
		// Set Values that are compatible with Windows' FM Driver
		OPL_HW_WriteReg(0x104, 0x00);	// 4-Op-Mode Disable
		OPL_HW_WriteReg(0x001, 0x00);	// Wave Select Disable
		OPL_HW_WriteReg(0x002, 0x00);	// Timer 1
		OPL_HW_WriteReg(0x003, 0x00);	// Timer 2
		OPL_HW_WriteReg(0x004, 0x00);	// IRQ mask clear
		OPL_HW_WriteReg(0x008, 0x00);	// Keyboard Split
		OPL_HW_WriteReg(0x0BD, 0xC0);	// Rhythm Mode
	}
	OpenedFM = false;
	ChipCount = 0x00;
	memset(RegChnUsage, 0x00, sizeof(bool) * 0x20);
#endif	// DISABLE_HW_SUPPORT
	
	return;
}

void chip_reg_write(UINT8 ChipType, UINT8 ChipID,
					UINT8 Port, UINT8 Offset, UINT8 Data)
{
#ifndef DISABLE_HW_SUPPORT
	bool ModeFM;
	UINT8 CurChip;
	
	switch(PlayingMode)
	{
	case 0x00:
		ModeFM = false;
		break;
	case 0x01:
		ModeFM = true;
		break;
	case 0x02:
		ModeFM = false;
		for (CurChip = 0x00; CurChip < ChipCount; CurChip ++)
		{
			if (ChipMap[CurChip].ChipType == ChipType &&
				ChipMap[CurChip].ChipID == ChipID)
			{
				ModeFM = true;
				break;
			}
		}
		break;
	}
	
	if (! ModeFM)
	{
#endif	// DISABLE_HW_SUPPORT
		switch(ChipType)
		{
		case 0x00:	// SN76496
			sn764xx_w(ChipID, Port, Data);
			break;
		case 0x01:	// YM2413
			ym2413_w(ChipID, 0x00, Offset);
			ym2413_w(ChipID, 0x01, Data);
			break;
		case 0x02:	// YM2612
			ym2612_w(ChipID, (Port << 1) | 0x00, Offset);
			ym2612_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x03:	// YM2151
			ym2151_w(ChipID, 0x00, Offset);
			ym2151_w(ChipID, 0x01, Data);
			break;
		case 0x04:	// SegaPCM
			break;
		case 0x05:	// RF5C68
			rf5c68_w(ChipID, Offset, Data);
			break;
		case 0x06:	// YM2203
			ym2203_w(ChipID, 0x00, Offset);
			ym2203_w(ChipID, 0x01, Data);
			break;
		case 0x07:	// YM2608
			ym2608_w(ChipID, (Port << 1) | 0x00, Offset);
			ym2608_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x08:	// YM2610/YM2610B
			ym2610_w(ChipID, (Port << 1) | 0x00, Offset);
			ym2610_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x09:	// YM3812
			ym3812_w(ChipID, 0x00, Offset);
			ym3812_w(ChipID, 0x01, Data);
			break;
		case 0x0A:	// YM3526
			ym3526_w(ChipID, 0x00, Offset);
			ym3526_w(ChipID, 0x01, Data);
			break;
		case 0x0B:	// Y8950
			y8950_w(ChipID, 0x00, Offset);
			y8950_w(ChipID, 0x01, Data);
			break;
		case 0x0C:	// YMF262
			ymf262_w(ChipID, (Port << 1) | 0x00, Offset);
			ymf262_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x0D:	// YMF278B
			ymf278b_w(ChipID, (Port << 1) | 0x00, Offset);
			ymf278b_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x0E:	// YMF271
			ymf271_w(ChipID, (Port << 1) | 0x00, Offset);
			ymf271_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x0F:	// YMZ280B
			ymz280b_w(ChipID, 0x00, Offset);
			ymz280b_w(ChipID, 0x01, Data);
			break;
		case 0x10:	// RF5C164
			rf5c164_w(ChipID, Offset, Data);
			break;
		case 0x11:	// PWM
			pwm_chn_w(ChipID, Port, (Offset << 8) | (Data << 0));
			break;
		case 0x12:	// AY8910
			ayxx_w(ChipID, 0x00, Offset);
			ayxx_w(ChipID, 0x01, Data);
			break;
		case 0x13:	// GameBoy
			gb_sound_w(ChipID, Offset, Data);
			break;
		case 0x14:	// NES APU
			nes_w(ChipID, Offset, Data);
			break;
		case 0x15:	// MultiPCM
			multipcm_w(ChipID, Offset, Data);
			break;
		case 0x16:	// UPD7759
			upd7759_write(ChipID, Offset, Data);
			break;
		case 0x17:	// OKIM6258
			okim6258_write(ChipID, Offset, Data);
			break;
		case 0x18:	// OKIM6295
			okim6295_w(ChipID, Offset, Data);
			break;
		case 0x19:	// K051649 / SCC1
			k051649_w(ChipID, (Port << 1) | 0x00, Offset);
			k051649_w(ChipID, (Port << 1) | 0x01, Data);
			break;
		case 0x1A:	// K054539
			k054539_w(ChipID, (Port << 8) | (Offset << 0), Data);
			break;
		case 0x1B:	// HuC6280
			c6280_w(ChipID, Offset, Data);
			break;
		case 0x1C:	// C140
			c140_w(ChipID, (Port << 8) | (Offset << 0), Data);
			break;
		case 0x1D:	// K053260
			k053260_w(ChipID, Offset, Data);
			break;
		case 0x1E:	// Pokey
			pokey_w(ChipID, Offset, Data);
			break;
		case 0x1F:	// QSound
			qsound_w(ChipID, 0x00, Port);	// Data MSB
			qsound_w(ChipID, 0x01, Offset);	// Data LSB
			qsound_w(ChipID, 0x02, Data);	// Register
			break;
		case 0x20:	// YMF292/SCSP
			scsp_w(ChipID, (Port << 8) | (Offset << 0), Data);
			break;
		case 0x21:	// WonderSwan
			ws_audio_port_write(ChipID, 0x80 | Offset, Data);
			break;
		case 0x22:	// VSU
			VSU_Write(ChipID, (Port << 8) | (Offset << 0), Data);
			break;
		case 0x23:	// SAA1099
			saa1099_control_w(ChipID, 0, Offset);
			saa1099_data_w(ChipID, 0, Data);
			break;
		case 0x24:	// ES5503
			es5503_w(ChipID, Offset, Data);
			break;
		case 0x25:	// ES5506
			if (Port & 0x80)
				es550x_w16(ChipID, Port & 0x7F, (Offset << 8) | (Data << 0));
			else
				es550x_w(ChipID, Port, Data);
			break;
		case 0x26:	// X1-010
			seta_sound_w(ChipID, (Port << 8) | (Offset << 0), Data);
			break;
		case 0x27:	// C352
			c352_w(ChipID, Port, (Offset << 8) | (Data << 0));
			break;
		case 0x28:	// GA20
			irem_ga20_w(ChipID, Offset, Data);
			break;
//		case 0x##:	// OKIM6376
//			break;
		}
#ifndef DISABLE_HW_SUPPORT
	}
	else
	{
		if (! OpenedFM)
			return;
		
		SelChip = ChipArr[(ChipType << 1) | (ChipID & 0x01)];
		switch(ChipType)
		{
		case 0x00:	// SN76496
			switch(Port)
			{
			case 0x00:
				sn76496_write_opl(ChipID, 0x00, Data);
				break;
			case 0x01:
				sn76496_stereo_opl(ChipID, 0x00, Data);
				break;
			}
			break;
		case 0x01:	// YM2413
			switch(YM2413_EMU_CORE)
			{
			case YM2413_EC_DEFAULT:
				ym2413_w_opl(ChipID, 0x00, Offset);
				ym2413_w_opl(ChipID, 0x01, Data);
				break;
			case YM2413_EC_MEKA:
				FM_OPL_Write(Offset, Data);
				break;
			}
			break;
		case 0x09:	// YM3812
			if ((Offset & 0xF0) == 0xC0 && ! (Data & 0x30))
				Data |= 0x30;
			else if ((Offset & 0xF0) == 0xE0)
				Data &= 0xF3;
			OPL_RegMapper((ChipID << 8) | Offset, Data);
			break;
		case 0x0A:	// YM3526
			if ((Offset & 0xF0) == 0xC0 && ! (Data & 0x30))
				Data |= 0x30;
			else if ((Offset & 0xF0) == 0xE0)
				Data &= 0xF0;
			OPL_RegMapper((ChipID << 8) | Offset, Data);
			break;
		case 0x0B:	// Y8950
			if (Offset == 0x07 || (Offset >= 0x09 && Offset <= 0x17))
				break;
			if ((Offset & 0xF0) == 0xC0 && ! (Data & 0x30))
				Data |= 0x30;
			else if ((Offset & 0xF0) == 0xE0)
				Data &= 0xF0;
			OPL_RegMapper((ChipID << 8) | Offset, Data);
			break;
		case 0x0C:	// YMF262
			OPL_RegMapper((Port << 8) | Offset, Data);
			break;
		case 0x12:	// AY8910
			ay8910_write_opl(ChipID, Offset, Data);
			break;
		}
	}
#endif	// DISABLE_HW_SUPPORT
	return;
}

void OPL_Hardware_Detecton(void)
{
#ifndef DISABLE_HW_SUPPORT
	UINT8 Status1;
	UINT8 Status2;
#ifdef WIN32
	//LARGE_INTEGER TempQud1;
	//LARGE_INTEGER TempQud2;
	LARGE_INTEGER TempQudFreq;
	//__int64 TempQudMid;
	
	HWusTime = 0;
	if (! WINNT_MODE)
		Status1 = 0x00;
	else
#endif
		Status1 = OpenPortTalk();
	
	if (Status1)
	{
		OPL_MODE = 0x00;
		printf("Error opening FM Port! Permission denied!\n");
		goto FinishDetection;
	}
	
	OPL_MODE = 0x02;	// must be set to activate OPL2-Delays
	
	// OPL2 Detection
	OPL_HW_WriteReg(0x04, 0x60);
	OPL_HW_WriteReg(0x04, 0x80);
	Status1 = OPL_HW_GetStatus();
	Status1 &= 0xE0;
	
	OPL_HW_WriteReg(0x02, 0xFF);
	OPL_HW_WriteReg(0x04, 0x21);
	OPL_HW_WaitDelay(0, 80);
	
	Status2 = OPL_HW_GetStatus();
	Status2 &= 0xE0;
	
	OPL_HW_WriteReg(0x04, 0x60);
	OPL_HW_WriteReg(0x04, 0x80);
	
	if (! ((Status1 == 0x00) && (Status2 == 0xC0)))
	{
		// Detection failed
		OPL_MODE = 0x00;
		printf("No OPL Chip detected!\n");
		goto FinishDetection;
	}
	
	// OPL3 Detection
	Status1 = OPL_HW_GetStatus();
	Status1 &= 0x06;
	if (! Status1)
	{
		OPL_MODE = 0x03;
		OPL_CHIPS = 0x01;
		printf("OPL 3 Chip found.\n");
		goto FinishDetection;
	}
	
	// OPL2 Dual Chip Detection
	OPL_HW_WriteReg(0x104, 0x60);
	OPL_HW_WriteReg(0x104, 0x80);
	Status1 = OPL_HW_GetStatus();
	Status1 &= 0xE0;
	
	OPL_HW_WriteReg(0x102, 0xFF);
	OPL_HW_WriteReg(0x104, 0x21);
	OPL_HW_WaitDelay(0, 80);
	
	Status2 = OPL_HW_GetStatus();
	Status2 &= 0xE0;
	
	OPL_HW_WriteReg(0x104, 0x60);
	OPL_HW_WriteReg(0x104, 0x80);
	
	if ((Status1 == 0x00) && (Status2 == 0xC0))
	{
		OPL_CHIPS = 0x02;
		printf("Dual OPL 2 Chip found.\n");
	}
	else
	{
		OPL_CHIPS = 0x01;
		printf("OPL 2 Chip found.\n");
	}
	
FinishDetection:
#ifdef WIN32
	// note CPU time needed for 1 us
	QueryPerformanceFrequency(&TempQudFreq);
	HWusTime = TempQudFreq.QuadPart / 1000000;
	
	/*QueryPerformanceCounter(&TempQud1);
	OPL_HW_GetStatus();
	QueryPerformanceCounter(&TempQud2);
	TempQudMid = TempQud2.QuadPart - TempQud1.QuadPart;
	
	QueryPerformanceCounter(&TempQud1);
	OPL_HW_GetStatus();
	QueryPerformanceCounter(&TempQud2);
	TempQudMid += TempQud2.QuadPart - TempQud1.QuadPart;
	
	HWusTime = TempQudMid / 2;
	printf("Port Read Cycles: %I64u\tMSec Cycles: %I64u\n", HWusTime,
			TempQudFreq.QuadPart / 1000);
	printf("us per ms: %I64u\n", TempQudFreq.QuadPart / (HWusTime * 1000));
	HWusTime = TempQudFreq.QuadPart / 1000000;*/
	
	if (WINNT_MODE)
#endif
		ClosePortTalk();
#endif	// DISABLE_HW_SUPPORT
	
	return;
}

#ifndef DISABLE_HW_SUPPORT
INLINE UINT8 OPL_HW_GetStatus(void)
{
	UINT8 RetStatus;
	
#ifdef WIN32
	switch(WINNT_MODE)
	{
	case false:	// Windows 95/98/ME
		RetStatus = INP_9X(FMPort);
		break;
	case true:	// Windows NT/2000/XP/...
		RetStatus = INP_NT(FMPort);
		break;
	}
#else
	RetStatus = inb(FMPort);
#endif
	
	return RetStatus;
}

INLINE void OPL_HW_WaitDelay(INT64 StartTime, float Delay)
{
#ifdef WIN32
	LARGE_INTEGER CurTime;
	INT64 EndTime;
	UINT16 CurUS;
	
	// waits Delay us
	if (HWusTime)
	{
		OPL_HW_GetStatus();	// read once, just to be safe
		EndTime = (INT64)(StartTime + HWusTime * Delay);
		do
		{
			QueryPerformanceCounter(&CurTime);
		} while(CurTime.QuadPart < EndTime);
	}
	else if (Delay >= 1.0f)
	{
		for (CurUS = 0x00; CurUS < Delay; CurUS ++)
			OPL_HW_GetStatus();
	}
	else
	{
		OPL_HW_GetStatus();	// read once, just to be safe
	}

#else

	struct timespec NanoTime;
	
	OPL_HW_GetStatus();	// read once, then wait should work
	if (Delay >= 1.0f)
		Delay -= 1.0f;
	
	// waits Delay us
	NanoTime.tv_sec = 0;
	// xx-- nsec should be 1000 * Delay, but somehow the resulting Delay is too short --xx
	NanoTime.tv_nsec = 1000 * Delay;
	nanosleep(&NanoTime, NULL);
#endif
	
	return;
}
#endif	// DISABLE_HW_SUPPORT

void OPL_HW_WriteReg(UINT16 Reg, UINT8 Data)
{
#ifndef DISABLE_HW_SUPPORT
	UINT16 Port;
#ifdef WIN32
	LARGE_INTEGER StartTime;
#endif
	UINT8 DataOld;
	UINT8 OpNum;
	UINT8 TempVol;
	float TempSng;
	
	Reg &= 0x01FF;
	
	if (SkipMode)
	{
		OPLReg[Reg] = Data;
		return;
	}
	
	// Register = Rhythm Control and RhmythmOn-Bit changed
	// -> set/reset Modulator Volume of Channels 7 and 8 (Volume for HH/CYM)
	if (Reg == 0x0BD && (OPLReg[Reg] ^ Data) & 0x20 && FinalVol != 1.0f)
	{
		OPLReg[Reg] = Data;
		OPLRegForce[Reg] = 0x01;
		
		OPL_HW_WriteReg(0x51, OPLReg[0x51]);	// Ch 7 Mod TL
		OPL_HW_WriteReg(0x52, OPLReg[0x52]);	// Ch 8 Mod TL
	}
	
	DataOld = Data;
	if ((Reg & 0xE0) == 0x40)
	{
		OpNum = (Reg & 0x07) / 0x03;	// Modulator 0x00, Carrier 0x01
		TempVol = ((Reg & 0x18) >> 3) * 0x03 + ((Reg & 0x07) % 0x03);
		
		if ((OPLReg[(Reg & 0x100) | 0xC0 | TempVol] & 0x01))
			OpNum = 0x01;	// Additive Syntheses - affects final volume
		if (! (Reg & 0x100) && TempVol >= 0x07 && (OPLReg[0xBD] & 0x20))
			OpNum = 0x01;	// used as Volume for Rhythm: Hi-Hat / Cymbal
		if (OpNum == 0x01 && FinalVol != 1.0f)
		{
			TempVol = Data & 0x3F;
			TempSng = (float)pow(2.0, -TempVol / 8.0);
			TempSng *= FinalVol;
			if (TempSng > 0.0f)
				TempSng = (float)(-8.0 * log(TempSng) / log(2.0));
			else
				TempSng = 64.0f;
			if (TempSng < 0.0f)
				TempVol = 0x00;
			else if (TempSng >= 64.0f)
				TempVol = 0x3F;
			else
				TempVol = (UINT8)TempSng;
			
			Data = (Data & 0xC0) | TempVol;
		}
	}
	if (Data == DataOld && Data == OPLReg[Reg] && ! OPLRegForce[Reg])
		return;	// only write neccessary registers (avoid spamming)
	OPLReg[Reg] = DataOld;
	OPLRegForce[Reg] = (Data != DataOld) ? 0x01 : 0x00;	// force next write
	
	Port = (Reg & 0x100) ? (FMPort + 0x02) : (FMPort + 0x00);
	
	// 1. Set Register
	// 2. wait some time (short delay)
	// 3. Write Data
	// 4. wait some time (long delay)
#ifdef WIN32
	QueryPerformanceCounter(&StartTime);
	if (! WINNT_MODE)	// Windows 95/98/ME
		OUTP_9X(Port + 0x00, Reg & 0xFF);
	else				// Windows NT/2000/XP/...
		OUTP_NT(Port + 0x00, Reg & 0xFF);
	switch(OPL_MODE)
	{
	case 0x02:
		OPL_HW_WaitDelay(StartTime.QuadPart, DELAY_OPL2_REG);
		break;
	case 0x03:
		OPL_HW_WaitDelay(StartTime.QuadPart, DELAY_OPL3_REG);
		break;
	}
#else
	outb(Reg & 0xFF, Port + 0x00);
	switch(OPL_MODE)
	{
	case 0x02:
		OPL_HW_WaitDelay(0, DELAY_OPL2_REG);
		break;
	case 0x03:
		OPL_HW_WaitDelay(0, DELAY_OPL3_REG);
		break;
	}
#endif	// WIN32
	
#ifdef WIN32
	QueryPerformanceCounter(&StartTime);
	if (! WINNT_MODE)	// Windows 95/98/ME
		OUTP_9X(Port + 0x01, Data);
	else				// Windows NT/2000/XP/...
		OUTP_NT(Port + 0x01, Data);
	switch(OPL_MODE)
	{
	case 0x02:
		OPL_HW_WaitDelay(StartTime.QuadPart, DELAY_OPL2_DATA);
		break;
	case 0x03:
		OPL_HW_WaitDelay(StartTime.QuadPart, DELAY_OPL3_DATA);
		break;
	}
#else
	outb(Data, Port + 0x01);
	switch(OPL_MODE)
	{
	case 0x02:
		OPL_HW_WaitDelay(0, DELAY_OPL2_DATA);
		break;
	case 0x03:
		OPL_HW_WaitDelay(0, DELAY_OPL3_DATA);
		break;
	}
#endif	// WIN32
#endif	// DISABLE_HW_SUPPORT
	
	return;
}

void OPL_RegMapper(UINT16 Reg, UINT8 Data)
{
#ifndef DISABLE_HW_SUPPORT
	UINT16 NewReg;
	UINT8 RegType;
	UINT8 Grp;
	UINT8 Chn;
	UINT8 Slot;
	UINT8 RegOp;
	
	switch(Reg & 0xE0)
	{
	case 0x00:
		RegType = 0x00;
		break;
	case 0x20:
	case 0x40:
	case 0x60:
	case 0x80:
	case 0xE0:
		if ((Reg & 0x07) < 0x06 && ! ((Reg & 0x18) == 0x18))
			RegType = 0x01;
		else
			RegType = 0x00;
		break;
	case 0xA0:
		if ((Reg & 0x0F) < 0x09)
			RegType = 0x02;
		else
			RegType = 0x00;
		break;
	case 0xC0:
		if ((Reg & 0x1F) < 0x09)
			RegType = 0x02;
		else
			RegType = 0x00;
		break;
	}
	
	Grp = (Reg & 0x100) >> 8;
	switch(RegType)
	{
	case 0x01:
		Chn = ((Reg & 0x18) >> 3) * 0x03 + ((Reg & 0x07) % 0x03);
		Slot = (Reg & 0x07) / 0x03;
		break;
	case 0x02:
		Chn = Reg & 0x0F;
		break;
	}
	
	Grp += ChipMap[SelChip].RegBase;	Grp %= 0x02;
	Chn += ChipMap[SelChip].ChnBase;	Chn %= 0x09;
	
	switch(RegType)
	{
	case 0x00:
		NewReg = (Grp << 8) | (Reg & 0xFF);
		break;
	case 0x01:
		RegOp = (Chn / 0x03) * 0x08 | ((Slot * 0x03) + (Chn % 0x03));
		NewReg = (Grp << 8) | (Reg & 0xE0) | RegOp;
		break;
	case 0x02:
		NewReg = (Grp << 8) | (Reg & 0xF0) | Chn;
		break;
	}
	
	OPL_HW_WriteReg(NewReg, Data);
#endif	// DISABLE_HW_SUPPORT
	
	return;
}

void RefreshVolume()
{
#ifndef DISABLE_HW_SUPPORT
	UINT8 CurChip;
	UINT8 CurChn;
	UINT16 RegVal;
	
	for (CurChip = 0x00; CurChip < 0x02; CurChip ++)
	{
		for (CurChn = 0x00; CurChn < 0x09; CurChn ++)
		{
			RegVal = (CurChip << 8) | 0x40 | (CurChn / 0x03) * 0x08 | (CurChn % 0x03);
			OPL_HW_WriteReg(RegVal + 0x00, OPLReg[RegVal + 0x00]);
			OPL_HW_WriteReg(RegVal + 0x03, OPLReg[RegVal + 0x03]);
		}
	}
	
	return;
#endif
}

void StartSkipping(void)
{
#ifndef DISABLE_HW_SUPPORT
	if (SkipMode)
		return;
	
	SkipMode = true;
	memcpy(OPLRegBak, OPLReg, 0x200);
#endif
	
	return;
}

void StopSkipping(void)
{
#ifndef DISABLE_HW_SUPPORT
	UINT16 Reg;
	UINT16 OpReg;
	UINT8 RRBuffer[0x40];
	
	if (! SkipMode)
		return;
	
	SkipMode = false;
	
	// At first, turn all notes off that need it
	memcpy(RRBuffer + 0x00, &OPLReg[0x080], 0x20);
	memcpy(RRBuffer + 0x20, &OPLReg[0x180], 0x20);
	for (Reg = 0xB0; Reg < 0xB9; Reg ++)
	{
		OpReg = Reg & 0x0F;
		OpReg = (OpReg / 3) * 0x08 + (OpReg % 3);
		if (! (OPLReg[0x100 | Reg] & 0x20))
		{
			OPL_HW_WriteReg(0x180 + OpReg, (OPLReg[0x180 + OpReg] & 0xF0) | 0x0F);
			OPL_HW_WriteReg(0x183 + OpReg, (OPLReg[0x183 + OpReg] & 0xF0) | 0x0F);
			OPLRegForce[0x180 + OpReg] |= 0x01;
			OPLRegForce[0x183 + OpReg] |= 0x01;
			
			OPLRegForce[0x100 | Reg] |= (OPLReg[0x100 | Reg] != OPLRegBak[0x100 | Reg]);
			OPL_HW_WriteReg(0x100 | Reg, OPLReg[0x100 | Reg]);
		}
		if (! (OPLReg[0x000 | Reg] & 0x20))
		{
			OPL_HW_WriteReg(0x080 + OpReg, (OPLReg[0x080 + OpReg] & 0xF0) | 0x0F);
			OPL_HW_WriteReg(0x083 + OpReg, (OPLReg[0x083 + OpReg] & 0xF0) | 0x0F);
			OPLRegForce[0x080 + OpReg] |= 0x01;
			OPLRegForce[0x083 + OpReg] |= 0x01;
			
			OPLRegForce[0x000 | Reg] |= (OPLReg[0x000 | Reg] != OPLRegBak[0x000 | Reg]);
			OPL_HW_WriteReg(0x000 | Reg, OPLReg[0x000 | Reg]);
		}
	}
	memcpy(&OPLReg[0x080], RRBuffer + 0x00, 0x20);
	memcpy(&OPLReg[0x180], RRBuffer + 0x20, 0x20);
	
	// Now the actual save restore.
	Reg = 0x105;	// OPL3 Enable/Disable - this must be the very first thing sent
	OPLRegForce[Reg] |= (OPLReg[Reg] != OPLRegBak[Reg]);
	OPL_HW_WriteReg(Reg, OPLReg[Reg]);
	
	// Registers 0x00 to 0x1F and 0x100 to 0x11F MUST be sent first
	for (Reg = 0x00; Reg < 0x20; Reg ++)
	{
		// Write Port 1 first, so that Port 0 writes override them, if OPL3 mode is disabled
		OPLRegForce[0x100 | Reg] |= (OPLReg[0x100 | Reg] != OPLRegBak[0x100 | Reg]);
		OPL_HW_WriteReg(0x100 | Reg, OPLReg[0x100 | Reg]);
		OPLRegForce[0x000 | Reg] |= (OPLReg[0x000 | Reg] != OPLRegBak[0x000 | Reg]);
		OPL_HW_WriteReg(0x000 | Reg, OPLReg[0x000 | Reg]);
	}
	
	Reg = 0x200;
	do
	{
		Reg --;
		OPLRegForce[Reg] |= (OPLReg[Reg] != OPLRegBak[Reg]);
		OPL_HW_WriteReg(Reg, OPLReg[Reg]);
		
		if ((Reg & 0xFF) == 0xC0)
			Reg -= 0x20;	// Skip the frequency-registers
		if ((Reg & 0xFF) == 0x20)
			Reg -= 0x20;	// Skip the registers that are already sent
	} while(Reg);
	
	for (Reg = 0xA0; Reg < 0xC0; Reg ++)
	{
		// Writing to BA/BB on my YMF744 is like writing to B8/B9, so I need to filter such
		// writes out.
		if ((Reg & 0x0F) >= 0x09)
			continue;
		OPLRegForce[0x100 | Reg] |= (OPLReg[0x100 | Reg] != OPLRegBak[0x100 | Reg]);
		OPL_HW_WriteReg(0x100 | Reg, OPLReg[0x100 | Reg]);
		OPLRegForce[0x000 | Reg] |= (OPLReg[0x000 | Reg] != OPLRegBak[0x000 | Reg]);
		OPL_HW_WriteReg(0x000 | Reg, OPLReg[0x000 | Reg]);
	}
	
	Reg = 0x0BD;	// Rhythm Register / Vibrato/Tremolo Depth
	OPLRegForce[Reg] |= (OPLReg[Reg] != OPLRegBak[Reg]);
	OPL_HW_WriteReg(Reg, OPLReg[Reg]);
#endif
	
	return;
}

void ym2413opl_set_emu_core(UINT8 Emulator)
{
	YM2413_EMU_CORE = (Emulator < 0x02) ? Emulator : 0x00;
	
	return;
}
