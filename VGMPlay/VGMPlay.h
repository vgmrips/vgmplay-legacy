// Header File for structures and constants used within VGMPlay.c

#include "VGMFile.h"

#define VGMPLAY_VER_STR	"0.40.6"
//#define APLHA
//#define BETA
#define VGM_VER_STR		"1.71b"
#define VGM_VER_NUM		0x170

#define CHIP_COUNT	0x29
typedef struct chip_options
{
	bool Disabled;
	UINT8 EmuCore;
	UINT8 ChnCnt;
	// Special Flags:
	//	YM2612:	Bit 0 - DAC Highpass Enable, Bit 1 - SSG-EG Enable
	//	YM-OPN:	Bit 0 - Disable AY8910-Part
	UINT16 SpecialFlags;
	
	// Channel Mute Mask - 1 Channel is represented by 1 bit
	UINT32 ChnMute1;
	// Mask 2 - used by YMF287B for OPL4 Wavetable Synth and by YM2608/YM2610 for PCM
	UINT32 ChnMute2;
	// Mask 3 - used for the AY-part of some OPN-chips
	UINT32 ChnMute3;

	INT16* Panning;
} CHIP_OPTS;
typedef struct chips_options
{
	CHIP_OPTS SN76496;
	CHIP_OPTS YM2413;
	CHIP_OPTS YM2612;
	CHIP_OPTS YM2151;
	CHIP_OPTS SegaPCM;
	CHIP_OPTS RF5C68;
	CHIP_OPTS YM2203;
	CHIP_OPTS YM2608;
	CHIP_OPTS YM2610;
	CHIP_OPTS YM3812;
	CHIP_OPTS YM3526;
	CHIP_OPTS Y8950;
	CHIP_OPTS YMF262;
	CHIP_OPTS YMF278B;
	CHIP_OPTS YMF271;
	CHIP_OPTS YMZ280B;
	CHIP_OPTS RF5C164;
	CHIP_OPTS PWM;
	CHIP_OPTS AY8910;
	CHIP_OPTS GameBoy;
	CHIP_OPTS NES;
	CHIP_OPTS MultiPCM;
	CHIP_OPTS UPD7759;
	CHIP_OPTS OKIM6258;
	CHIP_OPTS OKIM6295;
	CHIP_OPTS K051649;
	CHIP_OPTS K054539;
	CHIP_OPTS HuC6280;
	CHIP_OPTS C140;
	CHIP_OPTS K053260;
	CHIP_OPTS Pokey;
	CHIP_OPTS QSound;
	CHIP_OPTS SCSP;
	CHIP_OPTS WSwan;
	CHIP_OPTS VSU;
	CHIP_OPTS SAA1099;
	CHIP_OPTS ES5503;
	CHIP_OPTS ES5506;
	CHIP_OPTS X1_010;
	CHIP_OPTS C352;
	CHIP_OPTS GA20;
//	CHIP_OPTS OKIM6376;
} CHIPS_OPTION;
