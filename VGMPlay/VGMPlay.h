// Header File for structures and constants used within VGMPlay.c

#include "VGMFile.h"
#include <zlib.h>

#define VGMPLAY_VER_STR	"0.40.5"
//#define APLHA
//#define BETA
#define VGM_VER_STR		"1.70"
#define VGM_VER_NUM		0x170

#define CHIP_COUNT	0x21
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
//	CHIP_OPTS OKIM6376;
} CHIPS_OPTION;

// Function Prototypes
INLINE UINT16 ReadLE16(const UINT8* Data);
INLINE UINT16 ReadBE16(const UINT8* Data);
INLINE UINT32 ReadLE24(const UINT8* Data);
INLINE UINT32 ReadLE32(const UINT8* Data);
INLINE int gzgetLE16(gzFile hFile, UINT16* RetValue);
INLINE int gzgetLE32(gzFile hFile, UINT32* RetValue);
static UINT32 gcd(UINT32 x, UINT32 y);

static void ReadVGMHeader(gzFile hFile, VGM_HEADER* RetVGMHead);
static UINT8 ReadGD3Tag(gzFile hFile, UINT32 GD3Offset, GD3_TAG* RetGD3Tag);
static void ReadChipExtraData32(UINT32 StartOffset, VGMX_CHP_EXTRA32* ChpExtra);
static void ReadChipExtraData16(UINT32 StartOffset, VGMX_CHP_EXTRA16* ChpExtra);
static wchar_t* MakeEmptyWStr(void);
static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos);
INLINE UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator);
static UINT16 GetChipVolume(VGM_HEADER* FileHead, UINT8 ChipID, UINT8 ChipNum, UINT8 ChipCnt);

static void RestartPlaying(void);
static void Chips_GeneralActions(UINT8 Mode);

INLINE INT32 SampleVGM2Pbk_I(INT32 SampleVal);	// inline functions
INLINE INT32 SamplePbk2VGM_I(INT32 SampleVal);
static UINT8 StartThread(void);
static UINT8 StopThread(void);
#if defined(WIN32) && defined(MIXER_MUTING)
static bool GetMixerControl(void);
#endif
static bool SetMuteControl(bool mute);

static void InterpretFile(UINT32 SampleCount);
static void AddPCMData(UINT8 Type, UINT32 DataSize, const UINT8* Data);
static bool DecompressDataBlk(VGM_PCM_DATA* Bank, UINT32 DataSize, const UINT8* Data);
static UINT8 GetDACFromPCMBank(void);
static UINT8* GetPointerFromPCMBank(UINT8 Type, UINT32 DataPos);
static void ReadPCMTable(UINT32 DataSize, const UINT8* Data);
static void InterpretVGM(UINT32 SampleCount);
#ifdef ADDITIONAL_FORMATS
extern void InterpretOther(UINT32 SampleCount);
#endif

static void GeneralChipLists(void);
INLINE INT16 Limit2Short(INT32 Value);
static void null_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
static void dual_opl2_stereo(UINT8 ChipID, stream_sample_t **outputs, int samples);
static INT32 RecalcFadeVolume(void);

#ifdef WIN32
DWORD WINAPI PlayingThread(void* Arg);
#else
UINT64 TimeSpec2Int64(const struct timespec* ts);
void* PlayingThread(void* Arg);
#endif

unsigned char OpenPortTalk(void);
void ClosePortTalk(void);

typedef void (*strm_func)(UINT8 ChipID, stream_sample_t **outputs, int samples);
