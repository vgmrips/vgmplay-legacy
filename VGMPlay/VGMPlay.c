// VGMPlay.c: C Source File of the Main Executable
//

// Line Size:	96 Chars
// Tab Size:	4 Spaces

/*3456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
0000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999*/
// TODO: Callback "ChangeSampleRate" to fix YM2203's AY8910

// Mixer Muting ON:
//		Mixer's FM Volume is set to 0 or Mute	-> absolutely muted
//		(sometimes it can take some time to get the Mixer Control under Windows)
// Mixer Muting OFF:
//		FM Volume is set to 0 through commands	-> very very low volume level ~0.4%
//		(faster way)
//#define MIXER_MUTING

// These defines enable additional features.
//	ADDITIONAL_FORMATS enables CMF and DRO support.
//	CONSOLE_MODE switches between VGMPlay and in_vgm mode.
//	in_vgm mode can also be used for custom players.
//
//#define ADDITIONAL_FORMATS
//#define CONSOLE_MODE
//#define VGM_BIG_ENDIAN

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "stdbool.h"
#include <math.h>	// for pow()

#ifdef WIN32
#ifndef NO_WCHAR_FILENAMES
// includes for manual wide-character ZLib open
#include <fcntl.h>	// for _O_RDONLY and _O_BINARY
#include <io.h>		// for _wopen and _close
#endif

#include <conio.h>	// for _inp()
#include <windows.h>
#else	//ifdef UNIX
#include <limits.h>		// for PATH_MAX
#include <pthread.h>	// for pthread functions

// (suitable?) Apple substitute for clock_gettime()
#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME	0
#define CLOCK_MONOTONIC	0
int clock_gettime(int clk_id, struct timespec *t)
{
	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);
	uint64_t time;
	time = mach_absolute_time();
	double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
	double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
	t->tv_sec = seconds;
	t->tv_nsec = nseconds;
	return 0;
}
#else
#include <time.h>		// for clock_gettime()
#endif

#include <unistd.h>		// for usleep()

#define MAX_PATH	PATH_MAX
#define	Sleep(msec)	usleep(msec * 1000)

#undef MIXER_MUTING		// I didn't get the Mixer Control to work under Linux

#ifdef MIXER_MUTING
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#endif
#endif	// WIN32/UNIX

#include <zlib.h>

#include "chips/mamedef.h"

// integer types for fast integer calculation
// the bit number is unused (it's an orientation)
#define FUINT8	unsigned int
#define FUINT16	unsigned int

#include "VGMPlay.h"
#include "VGMPlay_Intf.h"
#ifdef CONSOLE_MODE
#include "Stream.h"
#endif

#include "chips/ChipIncl.h"

unsigned char OpenPortTalk(void);
void ClosePortTalk(void);

#include "ChipMapper.h"

typedef void (*strm_func)(UINT8 ChipID, stream_sample_t **outputs, int samples);

typedef struct chip_audio_attributes CAUD_ATTR;
struct chip_audio_attributes
{
	UINT32 SmpRate;
	UINT16 Volume;
	UINT8 ChipType;
	UINT8 ChipID;		// 0 - 1st chip, 1 - 2nd chip, etc.
	// Resampler Type:
	//	00 - Old
	//	01 - Upsampling
	//	02 - Copy
	//	03 - Downsampling
	UINT8 Resampler;
	strm_func StreamUpdate;
	UINT32 SmpP;		// Current Sample (Playback Rate)
	UINT32 SmpLast;		// Sample Number Last
	UINT32 SmpNext;		// Sample Number Next
	WAVE_32BS LSmpl;	// Last Sample
	WAVE_32BS NSmpl;	// Next Sample
	CAUD_ATTR* Paired;
};

typedef struct chip_audio_struct
{
	CAUD_ATTR SN76496;
	CAUD_ATTR YM2413;
	CAUD_ATTR YM2612;
	CAUD_ATTR YM2151;
	CAUD_ATTR SegaPCM;
	CAUD_ATTR RF5C68;
	CAUD_ATTR YM2203;
	CAUD_ATTR YM2608;
	CAUD_ATTR YM2610;
	CAUD_ATTR YM3812;
	CAUD_ATTR YM3526;
	CAUD_ATTR Y8950;
	CAUD_ATTR YMF262;
	CAUD_ATTR YMF278B;
	CAUD_ATTR YMF271;
	CAUD_ATTR YMZ280B;
	CAUD_ATTR RF5C164;
	CAUD_ATTR PWM;
	CAUD_ATTR AY8910;
	CAUD_ATTR GameBoy;
	CAUD_ATTR NES;
	CAUD_ATTR MultiPCM;
	CAUD_ATTR UPD7759;
	CAUD_ATTR OKIM6258;
	CAUD_ATTR OKIM6295;
	CAUD_ATTR K051649;
	CAUD_ATTR K054539;
	CAUD_ATTR HuC6280;
	CAUD_ATTR C140;
	CAUD_ATTR K053260;
	CAUD_ATTR Pokey;
	CAUD_ATTR QSound;
	CAUD_ATTR SCSP;
	CAUD_ATTR WSwan;
	CAUD_ATTR VSU;
	CAUD_ATTR SAA1099;
	CAUD_ATTR ES5503;
	CAUD_ATTR ES5506;
	CAUD_ATTR X1_010;
	CAUD_ATTR C352;
	CAUD_ATTR GA20;
//	CAUD_ATTR OKIM6376;
} CHIP_AUDIO;

typedef struct chip_aud_list CA_LIST;
struct chip_aud_list
{
	CAUD_ATTR* CAud;
	CHIP_OPTS* COpts;
	CA_LIST* next;
};

typedef struct daccontrol_data
{
	bool Enable;
	UINT8 Bank;
} DACCTRL_DATA;

typedef struct pcmbank_table
{
	UINT8 ComprType;
	UINT8 CmpSubType;
	UINT8 BitDec;
	UINT8 BitCmp;
	UINT16 EntryCount;
	void* Entries;
} PCMBANK_TBL;


// Function Prototypes (prototypes in comments are defined in VGMPlay_Intf.h)
//void VGMPlay_Init(void);
//void VGMPlay_Init2(void);
//void VGMPlay_Deinit(void);
//char* FindFile(const char* FileName)

INLINE UINT16 ReadLE16(const UINT8* Data);
INLINE UINT16 ReadBE16(const UINT8* Data);
INLINE UINT32 ReadLE24(const UINT8* Data);
INLINE UINT32 ReadLE32(const UINT8* Data);
INLINE int gzgetLE16(gzFile hFile, UINT16* RetValue);
INLINE int gzgetLE32(gzFile hFile, UINT32* RetValue);
static UINT32 gcd(UINT32 x, UINT32 y);
//void PlayVGM(void);
//void StopVGM(void);
//void RestartVGM(void);
//void PauseVGM(bool Pause);
//void SeekVGM(bool Relative, INT32 PlayBkSamples);
//void RefreshMuting(void);
//void RefreshPanning(void);
//void RefreshPlaybackOptions(void);

//UINT32 GetGZFileLength(const char* FileName);
//UINT32 GetGZFileLengthW(const wchar_t* FileName);
static UINT32 GetGZFileLength_Internal(FILE* hFile);
//bool OpenVGMFile(const char* FileName);
static bool OpenVGMFile_Internal(gzFile hFile, UINT32 FileSize);
static void ReadVGMHeader(gzFile hFile, VGM_HEADER* RetVGMHead);
static UINT8 ReadGD3Tag(gzFile hFile, UINT32 GD3Offset, GD3_TAG* RetGD3Tag);
static void ReadChipExtraData32(UINT32 StartOffset, VGMX_CHP_EXTRA32* ChpExtra);
static void ReadChipExtraData16(UINT32 StartOffset, VGMX_CHP_EXTRA16* ChpExtra);
//void CloseVGMFile(void);
//void FreeGD3Tag(GD3_TAG* TagData);
static wchar_t* MakeEmptyWStr(void);
static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos);
//UINT32 GetVGMFileInfo(const char* FileName, VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag);
static UINT32 GetVGMFileInfo_Internal(gzFile hFile, UINT32 FileSize,
									  VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag);
INLINE UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator);
//UINT32 CalcSampleMSec(UINT64 Value, UINT8 Mode);
//UINT32 CalcSampleMSecExt(UINT64 Value, UINT8 Mode, VGM_HEADER* FileHead);
//const char* GetChipName(UINT8 ChipID);
//const char* GetAccurateChipName(UINT8 ChipID, UINT8 SubType);
//UINT32 GetChipClock(VGM_HEADER* FileHead, UINT8 ChipID, UINT8* RetSubType);
static UINT16 GetChipVolume(VGM_HEADER* FileHead, UINT8 ChipID, UINT8 ChipNum, UINT8 ChipCnt);

static void RestartPlaying(void);
static void Chips_GeneralActions(UINT8 Mode);

INLINE INT32 SampleVGM2Pbk_I(INT32 SampleVal);	// inline functions
INLINE INT32 SamplePbk2VGM_I(INT32 SampleVal);
//INT32 SampleVGM2Playback(INT32 SampleVal);		// non-inline functions
//INT32 SamplePlayback2VGM(INT32 SampleVal);
static UINT8 StartThread(void);
static UINT8 StopThread(void);
#if defined(WIN32) && defined(MIXER_MUTING)
static bool GetMixerControl(void);
#endif
static bool SetMuteControl(bool mute);

static void InterpretFile(UINT32 SampleCount);
static void AddPCMData(UINT8 Type, UINT32 DataSize, const UINT8* Data);
//INLINE FUINT16 ReadBits(UINT8* Data, UINT32* Pos, FUINT8* BitPos, FUINT8 BitsToRead);
static bool DecompressDataBlk(VGM_PCM_DATA* Bank, UINT32 DataSize, const UINT8* Data);
static UINT8 GetDACFromPCMBank(void);
static UINT8* GetPointerFromPCMBank(UINT8 Type, UINT32 DataPos);
static void ReadPCMTable(UINT32 DataSize, const UINT8* Data);
static void InterpretVGM(UINT32 SampleCount);
#ifdef ADDITIONAL_FORMATS
extern void InterpretOther(UINT32 SampleCount);
#endif

static void GeneralChipLists(void);
static void SetupResampler(CAUD_ATTR* CAA);
static void ChangeChipSampleRate(void* DataPtr, UINT32 NewSmplRate);

INLINE INT16 Limit2Short(INT32 Value);
static void null_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
static void dual_opl2_stereo(UINT8 ChipID, stream_sample_t **outputs, int samples);
static void ResampleChipStream(CA_LIST* CLst, WAVE_32BS* RetSample, UINT32 Length);
static INT32 RecalcFadeVolume(void);
//UINT32 FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize)

#ifdef WIN32
DWORD WINAPI PlayingThread(void* Arg);
#else
UINT64 TimeSpec2Int64(const struct timespec* ts);
void* PlayingThread(void* Arg);
#endif


// Options Variables
UINT32 SampleRate;	// Note: also used by some sound cores to determinate the chip sample rate

UINT32 VGMMaxLoop;
UINT32 VGMPbRate;	// in Hz, ignored if this value or VGM's lngRate Header value is 0
#ifdef ADDITIONAL_FORMATS
extern UINT32 CMFMaxLoop;
#endif
UINT32 FadeTime;
UINT32 PauseTime;	// current Pause Time

float VolumeLevel;
bool SurroundSound;
bool FadeRAWLog;
//bool FullBufFill;	// Fill Buffer until it's full
bool PauseEmulate;

bool DoubleSSGVol;

UINT8 ResampleMode;	// 00 - HQ both, 01 - LQ downsampling, 02 - LQ both
UINT8 CHIP_SAMPLING_MODE;
INT32 CHIP_SAMPLE_RATE;

UINT16 FMPort;
bool FMForce;
//bool FMAccurate;
bool FMBreakFade;
float FMVol;

CHIPS_OPTION ChipOpts[0x02];

UINT8 OPL_MODE;
UINT8 OPL_CHIPS;
#ifdef WIN32
bool WINNT_MODE;
#endif

stream_sample_t* DUMMYBUF[0x02] = {NULL, NULL};

char* AppPaths[8];

bool AutoStopSkip;

UINT8 FileMode;
VGM_HEADER VGMHead;
VGM_HDR_EXTRA VGMHeadX;
VGM_EXTRA VGMH_Extra;
UINT32 VGMDataLen;
UINT8* VGMData;
GD3_TAG VGMTag;

#define PCM_BANK_COUNT	0x40
VGM_PCM_BANK PCMBank[PCM_BANK_COUNT];
PCMBANK_TBL PCMTbl;
UINT8 DacCtrlUsed;
UINT8 DacCtrlUsg[0xFF];
DACCTRL_DATA DacCtrl[0xFF];

#ifdef WIN32
HANDLE hPlayThread;
#else
pthread_t hPlayThread;
#endif
bool PlayThreadOpen;
volatile bool PauseThread;
static volatile bool CloseThread;
bool ThreadPauseEnable;
volatile bool ThreadPauseConfrm;
bool ThreadNoWait;	// don't reset the timer

CHIP_AUDIO ChipAudio[0x02];
CAUD_ATTR CA_Paired[0x02][0x03];
float MasterVol;

CA_LIST ChipListBuffer[0x200];
CA_LIST* ChipListAll;	// all chips needed for playback (in general)
CA_LIST* ChipListPause;	// all chips needed for EmulateWhilePaused
//CA_LIST* ChipListOpt;	// ChipListAll minus muted chips
CA_LIST* CurChipList;	// pointer to Pause or All [Opt]

#define SMPL_BUFSIZE	0x100
static INT32* StreamBufs[0x02];

#ifdef MIXER_MUTING

#ifdef WIN32
HMIXER hmixer;
MIXERCONTROL mixctrl;
#else
int hmixer;
UINT16 mixer_vol;
#endif

#else	//#ifndef MIXER_MUTING
float VolumeBak;
#endif

UINT32 VGMPos;
INT32 VGMSmplPos;
INT32 VGMSmplPlayed;
INT32 VGMSampleRate;
static UINT32 VGMPbRateMul;
static UINT32 VGMPbRateDiv;
static UINT32 VGMSmplRateMul;
static UINT32 VGMSmplRateDiv;
static UINT32 PauseSmpls;
bool VGMEnd;
bool EndPlay;
bool PausePlay;
bool FadePlay;
bool ForceVGMExec;
UINT8 PlayingMode;
bool UseFM;
UINT32 PlayingTime;
UINT32 FadeStart;
UINT32 VGMMaxLoopM;
UINT32 VGMCurLoop;
float VolumeLevelM;
float FinalVol;
bool ResetPBTimer;

static bool Interpreting;

#ifdef CONSOLE_MODE
extern bool ErrorHappened;
extern UINT8 CmdList[0x100];
#endif

UINT8 IsVGMInit;
UINT16 Last95Drum;	// for optvgm debugging
UINT16 Last95Max;	// for optvgm debugging
UINT32 Last95Freq;	// for optvgm debugging

void VGMPlay_Init(void)
{
	UINT8 CurChip;
	UINT8 CurCSet;
	UINT8 CurChn;
	CHIP_OPTS* TempCOpt;
	CAUD_ATTR* TempCAud;
	
	SampleRate = 44100;
	FadeTime = 5000;
	PauseTime = 0;
	
	FadeRAWLog = false;
	VolumeLevel = 1.0f;
	//FullBufFill = false;
	FMPort = 0x0000;
	FMForce = false;
	//FMAccurate = false;
	FMBreakFade = false;
	FMVol = 0.0f;
	SurroundSound = false;
	VGMMaxLoop = 0x02;
	VGMPbRate = 0;
#ifdef ADDITIONAL_FORMATS
	CMFMaxLoop = 0x01;
#endif
	ResampleMode = 0x00;
	CHIP_SAMPLING_MODE = 0x00;
	CHIP_SAMPLE_RATE = 0x00000000;
	PauseEmulate = false;
	DoubleSSGVol = false;
	
	for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
	{
		TempCAud = (CAUD_ATTR*)&ChipAudio[CurCSet];
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCAud ++)
		{
			TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet] + CurChip;
			
			TempCOpt->Disabled = false;
			TempCOpt->EmuCore = 0x00;
			TempCOpt->SpecialFlags = 0x00;
			TempCOpt->ChnCnt = 0x00;
			TempCOpt->ChnMute1 = 0x00;
			TempCOpt->ChnMute2 = 0x00;
			TempCOpt->ChnMute3 = 0x00;
			TempCOpt->Panning = NULL;
			
			// Set up some important fields to prevent in_vgm from crashing
			// when clicking on Muting checkboxes after init.
			TempCAud->ChipType = 0xFF;
			TempCAud->ChipID = CurCSet;
			TempCAud->Paired = NULL;
		}
		ChipOpts[CurCSet].GameBoy.SpecialFlags = 0x0003;
		// default options, 0x8000 skips the option write and keeps NSFPlay's default values
		// TODO: Is this really necessary??
		ChipOpts[CurCSet].NES.SpecialFlags = 0x8000 |
										(0x00 << 12) | (0x3B << 4) | (0x01 << 2) | (0x03 << 0);
		
		TempCAud = CA_Paired[CurCSet];
		for (CurChip = 0x00; CurChip < 0x03; CurChip ++, TempCAud ++)
		{
			TempCAud->ChipType = 0xFF;
			TempCAud->ChipID = CurCSet;
			TempCAud->Paired = NULL;
		}
		
		// currently the only chips with Panning support are
		// SN76496 and YM2413, it should be not a problem that it's hardcoded.
		TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet].SN76496;
		TempCOpt->ChnCnt = 0x04;
		TempCOpt->Panning = (INT16*)malloc(sizeof(INT16) * TempCOpt->ChnCnt);
		for (CurChn = 0x00; CurChn < TempCOpt->ChnCnt; CurChn ++)
			TempCOpt->Panning[CurChn] = 0x00;
		
		TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet].YM2413;
		TempCOpt->ChnCnt = 0x0E;	// 0x09 + 0x05
		TempCOpt->Panning = (INT16*)malloc(sizeof(INT16) * TempCOpt->ChnCnt);
		for (CurChn = 0x00; CurChn < TempCOpt->ChnCnt; CurChn ++)
			TempCOpt->Panning[CurChn] = 0x00;
	}
	
	for (CurChn = 0; CurChn < 8; CurChn ++)
		AppPaths[CurChn] = NULL;
	AppPaths[0] = "";
	
	FileMode = 0xFF;
	
	PausePlay = false;
	
#ifdef _DEBUG
	if (sizeof(CHIP_AUDIO) != sizeof(CAUD_ATTR) * CHIP_COUNT)
	{
		printf("Fatal Error! ChipAudio structure invalid!\n");
		getchar();
		exit(-1);
	}
	if (sizeof(CHIPS_OPTION) != sizeof(CHIP_OPTS) * CHIP_COUNT)
	{
		printf("Fatal Error! ChipOpts structure invalid!\n");
		getchar();
		exit(-1);
	}
#endif
	
	return;
}

void VGMPlay_Init2(void)
{
	// has to be called after the configuration is loaded
	
	if (FMPort)
	{
#if defined(WIN32) && defined(_MSC_VER)
		__try
		{
			// should work well with WinXP Compatibility Mode
			_inp(FMPort);
			WINNT_MODE = false;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			WINNT_MODE = true;
		}
#endif
		
		if (! OPL_MODE)	// OPL not forced
			OPL_Hardware_Detecton();
		if (! OPL_MODE)	// no OPL chip found
			FMPort = 0x0000;	// disable FM
	}
	if (FMPort)
	{
		// prepare FM Hardware Access and open MIDI Mixer
#ifdef WIN32
#ifdef MIXER_MUTING
		mixerOpen(&hmixer, 0x00, 0x00, 0x00, 0x00);
		GetMixerControl();
#endif
		
		if (WINNT_MODE)
		{
			if (OpenPortTalk())
				return;
		}
#else	//#ifndef WIN32
#ifdef MIXER_MUTING
		hmixer = open("/dev/mixer", O_RDWR);
#endif
		
		if (OpenPortTalk())
			return;
#endif
	}
	
	StreamBufs[0x00] = (INT32*)malloc(SMPL_BUFSIZE * sizeof(INT32));
	StreamBufs[0x01] = (INT32*)malloc(SMPL_BUFSIZE * sizeof(INT32));
	
	if (CHIP_SAMPLE_RATE <= 0)
		CHIP_SAMPLE_RATE = SampleRate;
	PlayingMode = 0xFF;
	
	return;
}

void VGMPlay_Deinit(void)
{
	UINT8 CurChip;
	UINT8 CurCSet;
	CHIP_OPTS* TempCOpt;
	
	if (FMPort)
	{
#ifdef MIXER_MUTING
#ifdef WIN32
		mixerClose(hmixer);
#else
		close(hmixer);
#endif
#endif
	}
	
	free(StreamBufs[0x00]);	StreamBufs[0x00] = NULL;
	free(StreamBufs[0x01]);	StreamBufs[0x01] = NULL;
	
	for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
	{
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
		{
			TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet] + CurChip;
			
			if (TempCOpt->Panning != NULL)
			{
				free(TempCOpt->Panning);	TempCOpt->Panning = NULL;
			}
		}
	}
	
	return;
}

// Note: Caller must free the returned string.
char* FindFile(const char* FileName)
{
	char* FullName;
	char** CurPath;
	UINT32 NameLen;
	UINT32 PathLen;
	UINT32 FullLen;
	FILE* hFile;
	
	NameLen = strlen(FileName);
	//printf("Find File: %s\n", FileName);
	
	// go to end of the list + get size of largest path
	// (The first entry has the lowest priority.)
	PathLen = 0;
	CurPath = AppPaths;
	while(*CurPath != NULL)
	{
		FullLen = strlen(*CurPath);
		if (FullLen > PathLen)
			PathLen = FullLen;
		CurPath ++;
	}
	CurPath --;
	FullLen = PathLen + NameLen;
	FullName = (char*)malloc(FullLen + 1);
	
	while(CurPath >= AppPaths)
	{
		strcpy(FullName, *CurPath);
		strcat(FullName, FileName);
		
		//printf("Trying path: %s\n", FullName);
		hFile = fopen(FullName, "r");
		if (hFile != NULL)
			break;
		
		CurPath --;
	}
	
	if (hFile != NULL)
	{
		fclose(hFile);
		//printf("Success!\n");
		return FullName;	// The caller has to free the string.
	}
	else
	{
		free(FullName);
		//printf("Fail!\n");
		return NULL;
	}
}

char* FindFile_List(const char** FileNameList)
{
	char* FullName;
	const char** CurFile;
	char** CurPath;
	char* PathPtr;
	UINT32 NameLen;
	UINT32 PathLen;
	UINT32 FullLen;
	FILE* hFile;
	
	NameLen = 0;
	CurFile = FileNameList;
	while(*CurFile != NULL)
	{
		FullLen = strlen(*CurFile);
		if (FullLen > NameLen)
			NameLen = FullLen;
		CurFile ++;
	}
	
	// go to end of the list + get size of largest path
	// (The first entry has the lowest priority.)
	PathLen = 0;
	CurPath = AppPaths;
	while(*CurPath != NULL)
	{
		FullLen = strlen(*CurPath);
		if (FullLen > PathLen)
			PathLen = FullLen;
		CurPath ++;
	}
	CurPath --;
	FullLen = PathLen + NameLen;
	FullName = (char*)malloc(FullLen + 1);
	
	hFile = NULL;
	while(CurPath >= AppPaths)
	{
		strcpy(FullName, *CurPath);
		PathPtr = FullName + strlen(FullName);
		CurFile = FileNameList;
		while(*CurFile != NULL)
		{
			strcpy(PathPtr, *CurFile);
			
			//printf("Trying path: %s\n", FullName);
			hFile = fopen(FullName, "r");
			if (hFile != NULL)
				break;
			
			CurFile ++;
		}
		if (hFile != NULL)
			break;
		
		CurPath --;
	}
	
	if (hFile != NULL)
	{
		fclose(hFile);
		//printf("Success!\n");
		return FullName;	// The caller has to free the string.
	}
	else
	{
		free(FullName);
		//printf("Fail!\n");
		return NULL;
	}
}


INLINE UINT16 ReadLE16(const UINT8* Data)
{
	// read 16-Bit Word (Little Endian/Intel Byte Order)
#ifndef VGM_BIG_ENDIAN
	return *(UINT16*)Data;
#else
	return (Data[0x01] << 8) | (Data[0x00] << 0);
#endif
}

INLINE UINT16 ReadBE16(const UINT8* Data)
{
	// read 16-Bit Word (Big Endian/Motorola Byte Order)
#ifndef VGM_BIG_ENDIAN
	return (Data[0x00] << 8) | (Data[0x01] << 0);
#else
	return *(UINT16*)Data;
#endif
}

INLINE UINT32 ReadLE24(const UINT8* Data)
{
	// read 24-Bit Word (Little Endian/Intel Byte Order)
#ifndef VGM_BIG_ENDIAN
	return	(*(UINT32*)Data) & 0x00FFFFFF;
#else
	return	(Data[0x02] << 16) | (Data[0x01] <<  8) | (Data[0x00] <<  0);
#endif
}

INLINE UINT32 ReadLE32(const UINT8* Data)
{
	// read 32-Bit Word (Little Endian/Intel Byte Order)
#ifndef VGM_BIG_ENDIAN
	return	*(UINT32*)Data;
#else
	return	(Data[0x03] << 24) | (Data[0x02] << 16) |
			(Data[0x01] <<  8) | (Data[0x00] <<  0);
#endif
}

INLINE int gzgetLE16(gzFile hFile, UINT16* RetValue)
{
#ifndef VGM_BIG_ENDIAN
	return gzread(hFile, RetValue, 0x02);
#else
	int RetVal;
	UINT8 Data[0x02];
	
	RetVal = gzread(hFile, Data, 0x02);
	*RetValue =	(Data[0x01] << 8) | (Data[0x00] << 0);
	return RetVal;
#endif
}

INLINE int gzgetLE32(gzFile hFile, UINT32* RetValue)
{
#ifndef VGM_BIG_ENDIAN
	return gzread(hFile, RetValue, 0x04);
#else
	int RetVal;
	UINT8 Data[0x04];
	
	RetVal = gzread(hFile, Data, 0x04);
	*RetValue =	(Data[0x03] << 24) | (Data[0x02] << 16) |
				(Data[0x01] <<  8) | (Data[0x00] <<  0);
	return RetVal;
#endif
}

static UINT32 gcd(UINT32 x, UINT32 y)
{
	UINT32 shift;
	UINT32 diff;
	
	// Thanks to Wikipedia for this algorithm
	// http://en.wikipedia.org/wiki/Binary_GCD_algorithm
	if (! x || ! y)
		return x | y;
	
	for (shift = 0; ((x | y) & 1) == 0; shift ++)
	{
		x >>= 1;
		y >>= 1;
	}
	
	while((x & 1) == 0)
		x >>= 1;
	
	do
	{
		while((y & 1) == 0)
			y >>= 1;
		
		if (x < y)
		{
			y -= x;
		}
		else
		{
			diff = x - y;
			x = y;
			y = diff;
		}
		y >>= 1;
	} while(y);
	
	return x << shift;
}

void PlayVGM(void)
{
	UINT8 CurChip;
	UINT8 FMVal;
	INT32 TempSLng;
	
	if (PlayingMode != 0xFF)
		return;
	
	//PausePlay = false;
	FadePlay = false;
	MasterVol = 1.0f;
	ForceVGMExec = false;
	AutoStopSkip = false;
	FadeStart = 0;
	ForceVGMExec = true;
	PauseThread = true;
	
	// FM Check
	FMVal = 0x00;
	if (FMPort)
	{
		// check usage of devices that use the FM port, and ones that don't use it
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
		{
			if (! GetChipClock(&VGMHead, CurChip, NULL))
				continue;
			
			// supported chips are:
			//	SN76496 (0x00, special behaviour), YM2413 (0x01)
			//	YM3812 (0x09), YM3526 (0x0A), Y8950 (0x0B), YMF262 (0x0C)
			if (CurChip == 0x00 || CurChip == 0x12)
									// The SN76496 and AY8910 have a special state because of
				FMVal |= 0x04;		// bad FM emulation and fast software emulation.
			else if (CurChip == 0x01 || (CurChip >= 0x09 && CurChip <= 0x0C))
				FMVal |= 0x02;
			else
				FMVal |= 0x01;
		}
		
		if (! FMForce)
		{
			if (FMVal & 0x01)	// one or more software emulators used?
				FMVal &= ~0x02;	// use only software emulation
			
			if (FMVal == 0x04)
				FMVal = 0x01;	// FM SN76496 emulaton is bad
			else if ((FMVal & 0x05) == 0x05)
				FMVal &= ~0x04;	// prefer software SN76496 instead of unsynced output
		}
		FMVal = (FMVal & ~0x04) | ((FMVal & 0x04) >> 1);
	}
	switch(FMVal)
	{
	case 0x00:
	case 0x01:
		PlayingMode = 0x00;	// Normal Mode
		break;
	case 0x02:
		PlayingMode = 0x01;	// FM only Mode
		break;
	case 0x03:
		PlayingMode = 0x02;	// Normal/FM mixed Mode (NOT in sync, Hardware is a lot faster)
		//PlayingMode = 0x00;	// Force Normal Mode until I get them in sync - Mixed Mode
								// sounds terrible
		break;
	}
	UseFM = (PlayingMode > 0x00);
	
	if (VGMHead.bytVolumeModifier <= VOLUME_MODIF_WRAP)
		TempSLng = VGMHead.bytVolumeModifier;
	else if (VGMHead.bytVolumeModifier == (VOLUME_MODIF_WRAP + 0x01))
		TempSLng = VOLUME_MODIF_WRAP - 0x100;
	else
		TempSLng = VGMHead.bytVolumeModifier - 0x100;
	VolumeLevelM = (float)(VolumeLevel * pow(2.0, TempSLng / (double)0x20));
	if (UseFM)
	{
		if (FMVol > 0.0f)
			VolumeLevelM = FMVol;
		else if (FMVol < 0.0f)
			VolumeLevelM *= -FMVol;
	}
	FinalVol = VolumeLevelM;
	
	if (! VGMMaxLoop)
	{
		VGMMaxLoopM = 0x00;
	}
	else
	{
		TempSLng = (VGMMaxLoop * VGMHead.bytLoopModifier + 0x08) / 0x10 - VGMHead.bytLoopBase;
		VGMMaxLoopM = (TempSLng >= 0x01) ? TempSLng : 0x01;
	}
	
	if (! VGMPbRate || ! VGMHead.lngRate)
	{
		VGMPbRateMul = 1;
		VGMPbRateDiv = 1;
	}
	else
	{
		// I prefer small Multiplers and Dividers, as they're used very often
		TempSLng = gcd(VGMHead.lngRate, VGMPbRate);
		VGMPbRateMul = VGMHead.lngRate / TempSLng;
		VGMPbRateDiv = VGMPbRate / TempSLng;
	}
	VGMSmplRateMul = SampleRate * VGMPbRateMul;
	VGMSmplRateDiv = VGMSampleRate * VGMPbRateDiv;
	// same as above - to speed up the VGM <-> Playback calculation
	TempSLng = gcd(VGMSmplRateMul, VGMSmplRateDiv);
	VGMSmplRateMul /= TempSLng;
	VGMSmplRateDiv /= TempSLng;
	
	PlayingTime = 0;
	EndPlay = false;
	
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	VGMSmplPlayed = 0;
	VGMEnd = false;
	VGMCurLoop = 0x00;
	PauseSmpls = (PauseTime * SampleRate + 500) / 1000;
	if (VGMPos >= VGMHead.lngEOFOffset)
		VGMEnd = true;
	
#ifdef CONSOLE_MODE
	memset(CmdList, 0x00, 0x100 * sizeof(UINT8));
#endif
	
	if (! PauseEmulate)
	{
		switch(PlayingMode)
		{
		case 0x00:
			//PauseStream(PausePlay);
			break;
		case 0x01:
			//PauseThread = PausePlay;
			SetMuteControl(PausePlay);
			break;
		case 0x02:
			//PauseStream(PausePlay);
			SetMuteControl(PausePlay);
			break;
		}
	}
	
	Chips_GeneralActions(0x00);	// Start chips
	// also does Reset (0x01), Muting Mask (0x10) and Panning (0x20)
	
	if (UseFM)
	{
		// TODO: get FirstInit working
		//if (! FirstInit)
		{
			StartSkipping();	// don't apply OPL Reset to make Track changes smooth
			AutoStopSkip = true;
		}
		open_real_fm();
	}
	
	switch(PlayingMode)
	{
	case 0x00:	// the application controls the playback thread
		break;
	case 0x01:	// FM Playback needs an independent thread
		ResetPBTimer = false;
		if (StartThread())
		{
			printf("Error starting Playing Thread!\n");
			return;
		}
#ifdef CONSOLE_MODE
		PauseStream(true);
#endif
		break;
	case 0x02:	// like Mode 0x00, but Hardware is also controlled (not synced)
		break;
	}
	
	Last95Drum = 0xFFFF;
	Last95Freq = 0;
	Last95Max = 0xFFFF;
	IsVGMInit = true;
	Interpreting = false;
	InterpretFile(0);
	IsVGMInit = false;
	
	PauseThread = false;
	AutoStopSkip = true;
	ForceVGMExec = false;
	
	return;
}

void StopVGM(void)
{
	if (PlayingMode == 0xFF)
		return;
	
	if (! PauseEmulate)
	{
		if (UseFM && PausePlay)
			SetMuteControl(false);
	}
	
	switch(PlayingMode)
	{
	case 0x00:
		break;
	case 0x01:
		StopThread();
		/*if (SmoothTrackChange)
		{
			if (ThreadPauseEnable)
			{
				ThreadPauseConfrm = false;
				PauseThread = true;
				while(! ThreadPauseConfrm)
					Sleep(1);	// Wait until the Thread is finished
			}
		}*/
		break;
	case 0x02:
		break;
	}
	
	Chips_GeneralActions(0x02);	// Stop chips
	if (UseFM)
		close_real_fm();
	PlayingMode = 0xFF;
	
	return;
}

void RestartVGM(void)
{
	if (PlayingMode == 0xFF || ! VGMSmplPlayed)
		return;
	
	RestartPlaying();
	
	return;
}

void PauseVGM(bool Pause)
{
	if (PlayingMode == 0xFF || Pause == PausePlay)
		return;
	
	// now uses a temporary variable to avoid gaps (especially with DAC Stream Controller)
	if (! PauseEmulate)
	{
		if (Pause && ThreadPauseEnable)
		{
			ThreadPauseConfrm = false;
			PauseThread = true;
		}
		switch(PlayingMode)
		{
		case 0x00:
#ifdef CONSOLE_MODE
			PauseStream(Pause);
#endif
			break;
		case 0x01:
			ThreadNoWait = false;
			SetMuteControl(Pause);
			break;
		case 0x02:
#ifdef CONSOLE_MODE
			PauseStream(Pause);
#endif
			SetMuteControl(Pause);
			break;
		}
		if (Pause && ThreadPauseEnable)
		{
			while(! ThreadPauseConfrm)
				Sleep(1);	// Wait until the Thread is finished
		}
		PauseThread = Pause;
	}
	PausePlay = Pause;
	
	return;
}

void SeekVGM(bool Relative, INT32 PlayBkSamples)
{
	INT32 Samples;
	UINT32 LoopSmpls;
	
	if (PlayingMode == 0xFF || (Relative && ! PlayBkSamples))
		return;
	
	LoopSmpls = VGMCurLoop * SampleVGM2Pbk_I(VGMHead.lngLoopSamples);
	if (! Relative)
		Samples = PlayBkSamples - (LoopSmpls + VGMSmplPlayed);
	else
		Samples = PlayBkSamples;
	
	ThreadNoWait = false;
	PauseThread = true;
	if (UseFM)
		StartSkipping();
	if (Samples < 0)
	{
		Samples = LoopSmpls + VGMSmplPlayed + Samples;
		if (Samples < 0)
			Samples = 0;
		RestartPlaying();
	}
	
	ForceVGMExec = true;
	InterpretFile(Samples);
	ForceVGMExec = false;
#ifdef CONSOLE_MODE
	if (FadePlay && FadeStart)
		FadeStart += Samples;
#endif
	if (UseFM)
		StopSkipping();
	PauseThread = PausePlay;
	
	return;
}

void RefreshMuting(void)
{
	Chips_GeneralActions(0x10);	// set muting mask
	
	return;
}

void RefreshPanning(void)
{
	Chips_GeneralActions(0x20);	// set panning
	
	return;
}

void RefreshPlaybackOptions(void)
{
	INT32 TempVol;
	UINT8 CurChip;
	CHIP_OPTS* TempCOpt1;
	CHIP_OPTS* TempCOpt2;
	
	if (VGMHead.bytVolumeModifier <= VOLUME_MODIF_WRAP)
		TempVol = VGMHead.bytVolumeModifier;
	else if (VGMHead.bytVolumeModifier == (VOLUME_MODIF_WRAP + 0x01))
		TempVol = VOLUME_MODIF_WRAP - 0x100;
	else
		TempVol = VGMHead.bytVolumeModifier - 0x100;
	VolumeLevelM = (float)(VolumeLevel * pow(2.0, TempVol / (double)0x20));
	if (UseFM)
	{
		if (FMVol > 0.0f)
			VolumeLevelM = FMVol;
		else if (FMVol < 0.0f)
			VolumeLevelM *= -FMVol;
	}
	FinalVol = VolumeLevelM * MasterVol * MasterVol;
	
	//PauseSmpls = (PauseTime * SampleRate + 500) / 1000;
	
	if (PlayingMode == 0xFF)
	{
		TempCOpt1 = (CHIP_OPTS*)&ChipOpts[0x00];
		TempCOpt2 = (CHIP_OPTS*)&ChipOpts[0x01];
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCOpt1 ++, TempCOpt2 ++)
		{
			TempCOpt2->EmuCore = TempCOpt1->EmuCore;
			TempCOpt2->SpecialFlags = TempCOpt1->SpecialFlags;
		}
	}
	
	return;
}


UINT32 GetGZFileLength(const char* FileName)
{
	FILE* hFile;
	UINT32 FileSize;
	
	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
		return 0xFFFFFFFF;
	
	FileSize = GetGZFileLength_Internal(hFile);
	
	fclose(hFile);
	return FileSize;
}

#ifndef NO_WCHAR_FILENAMES
UINT32 GetGZFileLengthW(const wchar_t* FileName)
{
	FILE* hFile;
	UINT32 FileSize;
	
	hFile = _wfopen(FileName, L"rb");
	if (hFile == NULL)
		return 0xFFFFFFFF;
	
	FileSize = GetGZFileLength_Internal(hFile);
	
	fclose(hFile);
	return FileSize;
}
#endif

static UINT32 GetGZFileLength_Internal(FILE* hFile)
{
	UINT32 FileSize;
	UINT16 gzHead;
	size_t RetVal;
	
	RetVal = fread(&gzHead, 0x02, 0x01, hFile);
	if (RetVal >= 1)
	{
		gzHead = ReadBE16((UINT8*)&gzHead);
		if (gzHead != 0x1F8B)
		{
			RetVal = 0;	// no .gz signature - treat as normal file
		}
		else
		{
			// .gz File
			fseek(hFile, -4, SEEK_END);
			// Note: In the error case it falls back to fseek/ftell.
			RetVal = fread(&FileSize, 0x04, 0x01, hFile);
#ifdef VGM_BIG_ENDIAN
			FileSize = ReadLE32((UINT8*)&FileSize);
#endif
		}
	}
	if (RetVal <= 0)
	{
		// normal file
		fseek(hFile, 0x00, SEEK_END);
		FileSize = ftell(hFile);
	}
	
	return FileSize;
}

bool OpenVGMFile(const char* FileName)
{
	gzFile hFile;
	UINT32 FileSize;
	bool RetVal;
	
	FileSize = GetGZFileLength(FileName);
	
	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return false;
	
	RetVal = OpenVGMFile_Internal(hFile, FileSize);
	
	gzclose(hFile);
	return RetVal;
}

#ifndef NO_WCHAR_FILENAMES
bool OpenVGMFileW(const wchar_t* FileName)
{
	gzFile hFile;
	UINT32 FileSize;
	bool RetVal;
#if ZLIB_VERNUM < 0x1270
	int fDesc;
	
	FileSize = GetGZFileLengthW(FileName);
	
	fDesc = _wopen(FileName, _O_RDONLY | _O_BINARY);
	hFile = gzdopen(fDesc, "rb");
	if (hFile == NULL)
	{
		_close(fDesc);
		return false;
	}
#else
	FileSize = GetGZFileLengthW(FileName);
	
	hFile = gzopen_w(FileName, "rb");
	if (hFile == NULL)
		return false;
#endif
	
	RetVal = OpenVGMFile_Internal(hFile, FileSize);
	
	gzclose(hFile);
	return RetVal;
}
#endif

static bool OpenVGMFile_Internal(gzFile hFile, UINT32 FileSize)
{
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT32 HdrLimit;
	
	//gzseek(hFile, 0x00, SEEK_SET);
	gzrewind(hFile);
	gzgetLE32(hFile, &fccHeader);
	if (fccHeader != FCC_VGM)
		return false;
	
	if (FileMode != 0xFF)
		CloseVGMFile();
	
	FileMode = 0x00;
	VGMDataLen = FileSize;
	
	gzseek(hFile, 0x00, SEEK_SET);
	//gzrewind(hFile);
	ReadVGMHeader(hFile, &VGMHead);
	if (VGMHead.fccVGM != FCC_VGM)
	{
		printf("VGM signature matched on the first read, but not on the second one!\n");
		printf("This is a known zlib bug where gzseek fails. Please install a fixed zlib.\n");
		return false;
	}
	
	VGMSampleRate = 44100;
	if (! VGMDataLen)
		VGMDataLen = VGMHead.lngEOFOffset;
	if (! VGMHead.lngEOFOffset || VGMHead.lngEOFOffset > VGMDataLen)
	{
		printf("Warning! Invalid EOF Offset 0x%02X! (should be: 0x%02X)\n",
				VGMHead.lngEOFOffset, VGMDataLen);
		VGMHead.lngEOFOffset = VGMDataLen;
	}
	if (VGMHead.lngLoopOffset && ! VGMHead.lngLoopSamples)
	{
		// 0-Sample-Loops causes the program to hangs in the playback routine
		printf("Warning! Ignored Zero-Sample-Loop!\n");
		VGMHead.lngLoopOffset = 0x00000000;
	}
	if (VGMHead.lngDataOffset < 0x00000040)
	{
		printf("Warning! Invalid Data Offset 0x%02X!\n", VGMHead.lngDataOffset);
		VGMHead.lngDataOffset = 0x00000040;
	}
	
	memset(&VGMHeadX, 0x00, sizeof(VGM_HDR_EXTRA));
	memset(&VGMH_Extra, 0x00, sizeof(VGM_EXTRA));
	
	// Read Data
	VGMDataLen = VGMHead.lngEOFOffset;
	VGMData = (UINT8*)malloc(VGMDataLen);
	if (VGMData == NULL)
		return false;
	//gzseek(hFile, 0x00, SEEK_SET);
	gzrewind(hFile);
	gzread(hFile, VGMData, VGMDataLen);
	
	// Read Extra Header Data
	if (VGMHead.lngExtraOffset)
	{
		UINT32* TempPtr;
		
		CurPos = VGMHead.lngExtraOffset;
		TempPtr = (UINT32*)&VGMHeadX;
		// Read Header Size
		VGMHeadX.DataSize = ReadLE32(&VGMData[CurPos]);
		if (VGMHeadX.DataSize > sizeof(VGM_HDR_EXTRA))
			VGMHeadX.DataSize = sizeof(VGM_HDR_EXTRA);
		HdrLimit = CurPos + VGMHeadX.DataSize;
		CurPos += 0x04;
		TempPtr ++;
		
		// Read all relative offsets of this header and make them absolute.
		for (; CurPos < HdrLimit; CurPos += 0x04, TempPtr ++)
		{
			*TempPtr = ReadLE32(&VGMData[CurPos]);
			if (*TempPtr)
				*TempPtr += CurPos;
		}
		
		ReadChipExtraData32(VGMHeadX.Chp2ClkOffset, &VGMH_Extra.Clocks);
		ReadChipExtraData16(VGMHeadX.ChpVolOffset, &VGMH_Extra.Volumes);
	}
	
	// Read GD3 Tag
	HdrLimit = ReadGD3Tag(hFile, VGMHead.lngGD3Offset, &VGMTag);
	if (HdrLimit == 0x10)
	{
		VGMHead.lngGD3Offset = 0x00000000;
		//return false;
	}
	if (! VGMHead.lngGD3Offset)
	{
		// replace all NULL pointers with empty strings
		VGMTag.strTrackNameE = MakeEmptyWStr();
		VGMTag.strTrackNameJ = MakeEmptyWStr();
		VGMTag.strGameNameE = MakeEmptyWStr();
		VGMTag.strGameNameJ = MakeEmptyWStr();
		VGMTag.strSystemNameE = MakeEmptyWStr();
		VGMTag.strSystemNameJ = MakeEmptyWStr();
		VGMTag.strAuthorNameE = MakeEmptyWStr();
		VGMTag.strAuthorNameJ = MakeEmptyWStr();
		VGMTag.strReleaseDate = MakeEmptyWStr();
		VGMTag.strCreator = MakeEmptyWStr();
		VGMTag.strNotes = MakeEmptyWStr();
	}
	
	return true;
}

static void ReadVGMHeader(gzFile hFile, VGM_HEADER* RetVGMHead)
{
	VGM_HEADER CurHead;
	UINT32 CurPos;
	UINT32 HdrLimit;
	
	gzread(hFile, &CurHead, sizeof(VGM_HEADER));
#ifdef VGM_BIG_ENDIAN
	{
		UINT8* TempPtr;
		
		// Warning: Lots of pointer casting ahead!
		for (CurPos = 0x00; CurPos < sizeof(VGM_HEADER); CurPos += 0x04)
		{
			TempPtr = (UINT8*)&CurHead + CurPos;
			switch(CurPos)
			{
			case 0x28:
				// 0x28	[16-bit] SN76496 Feedback Mask
				// 0x2A	[ 8-bit] SN76496 Shift Register Width
				// 0x2B	[ 8-bit] SN76496 Flags
				*(UINT16*)TempPtr = ReadLE16(TempPtr);
				break;
			case 0x78:	// 78-7B [8-bit] AY8910 Type/Flags
			case 0x7C:	// 7C-7F [8-bit] Volume/Loop Modifiers
			case 0x94:	// 94-97 [8-bit] various flags
				break;
			default:
				// everything else is 32-bit
				*(UINT32*)TempPtr = ReadLE32(TempPtr);
				break;
			}
		}
	}
#endif
	
	// Header preperations
	if (CurHead.lngVersion < 0x00000101)
	{
		CurHead.lngRate = 0;
	}
	if (CurHead.lngVersion < 0x00000110)
	{
		CurHead.shtPSG_Feedback = 0x0000;
		CurHead.bytPSG_SRWidth = 0x00;
		CurHead.lngHzYM2612 = CurHead.lngHzYM2413;
		CurHead.lngHzYM2151 = CurHead.lngHzYM2413;
	}
	if (CurHead.lngVersion < 0x00000150)
	{
		CurHead.lngDataOffset = 0x00000000;
	// If I would aim to be very strict, I would uncomment these few lines,
	// but I sometimes use v1.51 Flags with v1.50 for better compatibility.
	// (Some hyper-strict players refuse to play v1.51 files, even if there's
	//  no new chip used.)
	//}
	//if (CurHead.lngVersion < 0x00000151)
	//{
		CurHead.bytPSG_Flags = 0x00;
		CurHead.lngHzSPCM = 0x0000;
		CurHead.lngSPCMIntf = 0x00000000;
		// all others are zeroed by memset
	}
	
	if (CurHead.lngHzPSG)
	{
		if (! CurHead.shtPSG_Feedback)
			CurHead.shtPSG_Feedback = 0x0009;
		if (! CurHead.bytPSG_SRWidth)
			CurHead.bytPSG_SRWidth = 0x10;
	}
	
	// relative -> absolute addresses
	if (CurHead.lngEOFOffset)
		CurHead.lngEOFOffset += 0x00000004;
	if (CurHead.lngGD3Offset)
		CurHead.lngGD3Offset += 0x00000014;
	if (CurHead.lngLoopOffset)
		CurHead.lngLoopOffset += 0x0000001C;
	
	if (CurHead.lngVersion < 0x00000150)
		CurHead.lngDataOffset = 0x0000000C;
	//if (CurHead.lngDataOffset < 0x0000000C)
	//	CurHead.lngDataOffset = 0x0000000C;
	if (CurHead.lngDataOffset)
		CurHead.lngDataOffset += 0x00000034;
	
	CurPos = CurHead.lngDataOffset;
	// should actually check v1.51 (first real usage of DataOffset)
	// v1.50 is checked to support things like the Volume Modifiers in v1.50 files
	if (CurHead.lngVersion < 0x00000150 /*0x00000151*/)
		CurPos = 0x40;
	if (! CurPos)
		CurPos = 0x40;
	HdrLimit = sizeof(VGM_HEADER);
	if (HdrLimit > CurPos)
		memset((UINT8*)&CurHead + CurPos, 0x00, HdrLimit - CurPos);
	
	if (! CurHead.bytLoopModifier)
		CurHead.bytLoopModifier = 0x10;
	
	if (CurHead.lngExtraOffset)
	{
		CurHead.lngExtraOffset += 0xBC;
		
		CurPos = CurHead.lngExtraOffset;
		if (CurPos < HdrLimit)
			memset((UINT8*)&CurHead + CurPos, 0x00, HdrLimit - CurPos);
	}
	
	if (CurHead.lngGD3Offset >= CurHead.lngEOFOffset)
		CurHead.lngGD3Offset = 0x00;
	if (CurHead.lngLoopOffset >= CurHead.lngEOFOffset)
		CurHead.lngLoopOffset = 0x00;
	if (CurHead.lngDataOffset >= CurHead.lngEOFOffset)
		CurHead.lngDataOffset = 0x40;
	if (CurHead.lngExtraOffset >= CurHead.lngEOFOffset)
		CurHead.lngExtraOffset = 0x00;
	
	*RetVGMHead = CurHead;
	return;
}

static UINT8 ReadGD3Tag(gzFile hFile, UINT32 GD3Offset, GD3_TAG* RetGD3Tag)
{
	UINT32 CurPos;
	UINT32 TempLng;
	UINT8 ResVal;
	
	ResVal = 0x00;
	
	// Read GD3 Tag
	if (GD3Offset)
	{
		gzseek(hFile, GD3Offset, SEEK_SET);
		gzgetLE32(hFile, &TempLng);
		if (TempLng != FCC_GD3)
		{
			GD3Offset = 0x00000000;
			ResVal = 0x10;	// invalid GD3 offset
		}
	}
	
	if (RetGD3Tag == NULL)
		return ResVal;
	
	if (! GD3Offset)
	{
		RetGD3Tag->fccGD3 = 0x00000000;
		RetGD3Tag->lngVersion = 0x00000000;
		RetGD3Tag->lngTagLength = 0x00000000;
		RetGD3Tag->strTrackNameE = NULL;
		RetGD3Tag->strTrackNameJ = NULL;
		RetGD3Tag->strGameNameE = NULL;
		RetGD3Tag->strGameNameJ = NULL;
		RetGD3Tag->strSystemNameE = NULL;
		RetGD3Tag->strSystemNameJ = NULL;
		RetGD3Tag->strAuthorNameE = NULL;
		RetGD3Tag->strAuthorNameJ = NULL;
		RetGD3Tag->strReleaseDate = NULL;
		RetGD3Tag->strCreator = NULL;
		RetGD3Tag->strNotes = NULL;
	}
	else
	{
		//CurPos = GD3Offset;
		//gzseek(hFile, CurPos, SEEK_SET);
		//CurPos += gzgetLE32(hFile, &RetGD3Tag->fccGD3);
		
		CurPos = GD3Offset + 0x04;		// Save some back seeking, yay!
		RetGD3Tag->fccGD3 = TempLng;	// (That costs lots of CPU in .gz files.)
		CurPos += gzgetLE32(hFile, &RetGD3Tag->lngVersion);
		CurPos += gzgetLE32(hFile, &RetGD3Tag->lngTagLength);
		
		TempLng = CurPos + RetGD3Tag->lngTagLength;
		RetGD3Tag->strTrackNameE =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strTrackNameJ =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strGameNameE =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strGameNameJ =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strSystemNameE =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strSystemNameJ =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strAuthorNameE =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strAuthorNameJ =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strReleaseDate =	ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strCreator =		ReadWStrFromFile(hFile, &CurPos, TempLng);
		RetGD3Tag->strNotes =		ReadWStrFromFile(hFile, &CurPos, TempLng);
	}
	
	return ResVal;
}

static void ReadChipExtraData32(UINT32 StartOffset, VGMX_CHP_EXTRA32* ChpExtra)
{
	UINT32 CurPos;
	UINT8 CurChp;
	VGMX_CHIP_DATA32* TempCD;
	
	if (! StartOffset || StartOffset >= VGMDataLen)
	{
		ChpExtra->ChipCnt = 0x00;
		ChpExtra->CCData = NULL;
		return;
	}
	
	CurPos = StartOffset;
	ChpExtra->ChipCnt = VGMData[CurPos];
	if (ChpExtra->ChipCnt)
		ChpExtra->CCData = (VGMX_CHIP_DATA32*)malloc(sizeof(VGMX_CHIP_DATA32) *
													ChpExtra->ChipCnt);
	else
		ChpExtra->CCData = NULL;
	CurPos ++;
	
	for (CurChp = 0x00; CurChp < ChpExtra->ChipCnt; CurChp ++)
	{
		TempCD = &ChpExtra->CCData[CurChp];
		TempCD->Type = VGMData[CurPos + 0x00];
		TempCD->Data = ReadLE32(&VGMData[CurPos + 0x01]);
		CurPos += 0x05;
	}
	
	return;
}

static void ReadChipExtraData16(UINT32 StartOffset, VGMX_CHP_EXTRA16* ChpExtra)
{
	UINT32 CurPos;
	UINT8 CurChp;
	VGMX_CHIP_DATA16* TempCD;
	
	if (! StartOffset || StartOffset >= VGMDataLen)
	{
		ChpExtra->ChipCnt = 0x00;
		ChpExtra->CCData = NULL;
		return;
	}
	
	CurPos = StartOffset;
	ChpExtra->ChipCnt = VGMData[CurPos];
	if (ChpExtra->ChipCnt)
		ChpExtra->CCData = (VGMX_CHIP_DATA16*)malloc(sizeof(VGMX_CHIP_DATA16) *
													ChpExtra->ChipCnt);
	else
		ChpExtra->CCData = NULL;
	CurPos ++;
	
	for (CurChp = 0x00; CurChp < ChpExtra->ChipCnt; CurChp ++)
	{
		TempCD = &ChpExtra->CCData[CurChp];
		TempCD->Type = VGMData[CurPos + 0x00];
		TempCD->Flags = VGMData[CurPos + 0x01];
		TempCD->Data = ReadLE16(&VGMData[CurPos + 0x02]);
		CurPos += 0x04;
	}
	
	return;
}

void CloseVGMFile(void)
{
	if (FileMode == 0xFF)
		return;
	
	VGMHead.fccVGM = 0x00;
	free(VGMH_Extra.Clocks.CCData);		VGMH_Extra.Clocks.CCData = NULL;
	free(VGMH_Extra.Volumes.CCData);	VGMH_Extra.Volumes.CCData = NULL;
	free(VGMData);	VGMData = NULL;
	
	if (FileMode == 0x00)
	FreeGD3Tag(&VGMTag);
	
	FileMode = 0xFF;
	
	return;
}

void FreeGD3Tag(GD3_TAG* TagData)
{
	if (TagData == NULL)
		return;
	
	TagData->fccGD3 = 0x00;
	free(TagData->strTrackNameE);	TagData->strTrackNameE = NULL;
	free(TagData->strTrackNameJ);	TagData->strTrackNameJ = NULL;
	free(TagData->strGameNameE);	TagData->strGameNameE = NULL;
	free(TagData->strGameNameJ);	TagData->strGameNameJ = NULL;
	free(TagData->strSystemNameE);	TagData->strSystemNameE = NULL;
	free(TagData->strSystemNameJ);	TagData->strSystemNameJ = NULL;
	free(TagData->strAuthorNameE);	TagData->strAuthorNameE = NULL;
	free(TagData->strAuthorNameJ);	TagData->strAuthorNameJ = NULL;
	free(TagData->strReleaseDate);	TagData->strReleaseDate = NULL;
	free(TagData->strCreator);		TagData->strCreator = NULL;
	free(TagData->strNotes);		TagData->strNotes = NULL;
	
	return;
}

static wchar_t* MakeEmptyWStr(void)
{
	wchar_t* Str;
	
	Str = (wchar_t*)malloc(0x01 * sizeof(wchar_t));
	Str[0x00] = L'\0';
	
	return Str;
}

static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos)
{
	// Note: Works with Windows (16-bit wchar_t) as well as Linux (32-bit wchar_t)
	UINT32 CurPos;
	wchar_t* TextStr;
	wchar_t* TempStr;
	UINT32 StrLen;
	UINT16 UnicodeChr;
	
	CurPos = *FilePos;
	if (CurPos >= EOFPos)
		return NULL;
	TextStr = (wchar_t*)malloc((EOFPos - CurPos) / 0x02 * sizeof(wchar_t));
	if (TextStr == NULL)
		return NULL;
	
	if (gztell(hFile) != CurPos)
		gzseek(hFile, CurPos, SEEK_SET);
	TempStr = TextStr - 1;
	StrLen = 0x00;
	do
	{
		TempStr ++;
		gzgetLE16(hFile, &UnicodeChr);
		*TempStr = (wchar_t)UnicodeChr;
		CurPos += 0x02;
		StrLen ++;
		if (CurPos >= EOFPos)
		{
			*TempStr = L'\0';
			break;
		}
	} while(*TempStr != L'\0');
	
	TextStr = (wchar_t*)realloc(TextStr, StrLen * sizeof(wchar_t));
	*FilePos = CurPos;
	
	return TextStr;
}

UINT32 GetVGMFileInfo(const char* FileName, VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag)
{
	gzFile hFile;
	UINT32 FileSize;
	UINT32 RetVal;
	
	FileSize = GetGZFileLength(FileName);
	
	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return 0x00;
	
	RetVal = GetVGMFileInfo_Internal(hFile, FileSize, RetVGMHead, RetGD3Tag);
	
	gzclose(hFile);
	return RetVal;
}

#ifndef NO_WCHAR_FILENAMES
UINT32 GetVGMFileInfoW(const wchar_t* FileName, VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag)
{
	gzFile hFile;
	UINT32 FileSize;
	UINT32 RetVal;
#if ZLIB_VERNUM < 0x1270
	int fDesc;
	
	FileSize = GetGZFileLengthW(FileName);
	
	fDesc = _wopen(FileName, _O_RDONLY | _O_BINARY);
	hFile = gzdopen(fDesc, "rb");
	if (hFile == NULL)
	{
		_close(fDesc);
		return 0x00;
	}
#else
	FileSize = GetGZFileLengthW(FileName);
	
	hFile = gzopen_w(FileName, "rb");
	if (hFile == NULL)
		return 0x00;
#endif
	
	RetVal = GetVGMFileInfo_Internal(hFile, FileSize, RetVGMHead, RetGD3Tag);
	
	gzclose(hFile);
	return RetVal;
}
#endif

static UINT32 GetVGMFileInfo_Internal(gzFile hFile, UINT32 FileSize,
									  VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag)
{
	// this is a copy-and-paste from OpenVGM, just a little stripped
	UINT32 fccHeader;
	UINT32 TempLng;
	VGM_HEADER TempHead;
	
	//gzseek(hFile, 0x00, SEEK_SET);
	gzrewind(hFile);
	gzgetLE32(hFile, &fccHeader);
	if (fccHeader != FCC_VGM)
		return 0x00;
	
	if (RetVGMHead == NULL && RetGD3Tag == NULL)
		return FileSize;
	
	//gzseek(hFile, 0x00, SEEK_SET);
	gzrewind(hFile);
	ReadVGMHeader(hFile, &TempHead);
	
	if (! TempHead.lngEOFOffset || TempHead.lngEOFOffset > FileSize)
		TempHead.lngEOFOffset = FileSize;
	if (TempHead.lngDataOffset < 0x00000040)
		TempHead.lngDataOffset = 0x00000040;
	
	/*if (TempHead.lngGD3Offset)
	{
		gzseek(hFile, TempHead.lngGD3Offset, SEEK_SET);
		gzgetLE32(hFile, &fccHeader);
		if (fccHeader != FCC_GD3)
			TempHead.lngGD3Offset = 0x00000000;
			//return 0x00;
	}*/
	
	if (RetVGMHead != NULL)
		*RetVGMHead = TempHead;
	
	// Read GD3 Tag
	if (RetGD3Tag != NULL)
		TempLng = ReadGD3Tag(hFile, TempHead.lngGD3Offset, RetGD3Tag);
	
	return FileSize;
}

INLINE UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator)
{
	return (UINT32)((Number * Numerator + Denominator / 2) / Denominator);
}

UINT32 CalcSampleMSec(UINT64 Value, UINT8 Mode)
{
	// Mode:
	//	Bit 0 (01):	Calculation Mode
	//				0 - Sample2MSec
	//				1 - MSec2Sample
	//	Bit 1 (02):	Calculation Samlpe Rate
	//				0 - current playback rate
	//				1 - 44.1 KHz (VGM native)
	UINT32 SmplRate;
	UINT32 PbMul;
	UINT32 PbDiv;
	UINT32 RetVal;
	
	if (! (Mode & 0x02))
	{
		SmplRate = SampleRate;
		PbMul = 1;
		PbDiv = 1;
	}
	else
	{
		SmplRate = VGMSampleRate;
		PbMul = VGMPbRateMul;
		PbDiv = VGMPbRateDiv;
	}
	
	switch(Mode & 0x01)
	{
	case 0x00:
		RetVal = MulDivRound(Value, (UINT64)1000 * PbMul, (UINT64)SmplRate * PbDiv);
		break;
	case 0x01:
		RetVal = MulDivRound(Value, (UINT64)SmplRate * PbDiv, (UINT64)1000 * PbMul);
		break;
	}
	
	return RetVal;
}

UINT32 CalcSampleMSecExt(UINT64 Value, UINT8 Mode, VGM_HEADER* FileHead)
{
	// Note: This function was NOT tested with non-VGM formats!
	
	// Mode: see function above
	UINT32 SmplRate;
	UINT32 PbMul;
	UINT32 PbDiv;
	UINT32 RetVal;
	
	if (! (Mode & 0x02))
	{
		SmplRate = SampleRate;
		PbMul = 1;
		PbDiv = 1;
	}
	else
	{
		// TODO: make it work for non-VGM formats
		// (i.e. get VGMSampleRate information from FileHead)
		//
		// But currently GetVGMFileInfo doesn't support them, it doesn't matter either way
		SmplRate = 44100;
		if (! VGMPbRate || ! FileHead->lngRate)
		{
			PbMul = 1;
			PbDiv = 1;
		}
		else
		{
			PbMul = FileHead->lngRate;
			PbDiv = VGMPbRate;
		}
	}
	
	switch(Mode & 0x01)
	{
	case 0x00:
		RetVal = MulDivRound(Value, 1000 * PbMul, SmplRate * PbDiv);
		break;
	case 0x01:
		RetVal = MulDivRound(Value, SmplRate * PbDiv, 1000 * PbMul);
		break;
	}
	
	return RetVal;
}

static UINT32 EncryptChipName(void* DstBuf, const void* SrcBuf, UINT32 Length)
{
	// using nineko's awesome encryption algorithm
	// http://forums.sonicretro.org/index.php?showtopic=25300
	// based on C code by sasuke
	const UINT8* SrcPos;
	UINT8* DstPos;
	UINT32 CurPos;
	UINT8 CryptShift;	// Src Bit/Dst Byte
	UINT8 PlainShift;	// Src Byte/Dst Bit
	
	if (Length & 0x07)
		return 0x00;	// Length MUST be a multiple of 8
	
	SrcPos = (const UINT8*)SrcBuf;
	DstPos = (UINT8*)DstBuf;
	for (CurPos = 0; CurPos < Length; CurPos += 8, SrcPos += 8, DstPos += 8)
	{
		for (CryptShift = 0; CryptShift < 8; CryptShift ++)
		{
			DstPos[CryptShift] = 0x00;
			for (PlainShift = 0; PlainShift < 8; PlainShift ++)
			{
				if (SrcPos[PlainShift] & (1 << CryptShift))
					DstPos[CryptShift] |= (1 << PlainShift);
			}
		}
	}
	
	return Length;
}

const char* GetChipName(UINT8 ChipID)
{
	const char* CHIP_STRS[CHIP_COUNT] = 
	{	"SN76496", "YM2413", "YM2612", "YM2151", "SegaPCM", "RF5C68", "YM2203", "YM2608",
		"YM2610", "YM3812", "YM3526", "Y8950", "YMF262", "YMF278B", "YMF271", "YMZ280B",
		"RF5C164", "PWM", "AY8910", "GameBoy", "NES APU", "MultiPCM", "uPD7759", "OKIM6258",
		"OKIM6295", "K051649", "K054539", "HuC6280", "C140", "K053260", "Pokey", "QSound",
		"SCSP", "WSwan", "VSU", "SAA1099", "ES5503", "ES5506", "X1-010", "C352",
		"GA20"};
	
	/*if (ChipID == 0x21)
	{
		static char TempStr[0x08];
		UINT32 TempData[2];
		
		//EncryptChipName(TempData, "WSwan", 0x08);
		TempData[0] = 0x1015170F;
		TempData[1] = 0x001F1C07;
		EncryptChipName(TempStr, TempData, 0x08);
		return TempStr;	// "WSwan"
	}*/
	
	if (ChipID < CHIP_COUNT)
		return CHIP_STRS[ChipID];
	else
		return NULL;
}

const char* GetAccurateChipName(UINT8 ChipID, UINT8 SubType)
{
	const char* RetStr;
	
	if ((ChipID & 0x7F) >= CHIP_COUNT)
		return NULL;
	
	RetStr = NULL;
	switch(ChipID & 0x7F)
	{
	case 0x00:
		if (! (ChipID & 0x80))
		{
			switch(SubType)
			{
			case 0x01:
				RetStr = "SN76489";
				break;
			case 0x02:
				RetStr = "SN76489A";
				break;
			case 0x03:
				RetStr = "SN76494";
				break;
			case 0x04:
				RetStr = "SN76496";
				break;
			case 0x05:
				RetStr = "SN94624";
				break;
			case 0x06:
				RetStr = "NCR7496";
				break;
			case 0x07:
				RetStr = "SEGA PSG";
				break;
			default:
				RetStr = "SN76496";
				break;
			}
		}
		else
		{
			RetStr = "T6W28";
		}
		break;
	case 0x01:
		if (ChipID & 0x80)
			RetStr = "VRC7";
		break;
	case 0x04:
		RetStr = "Sega PCM";
		break;
	case 0x08:
		if (! (ChipID & 0x80))
			RetStr = "YM2610";
		else
			RetStr = "YM2610B";
		break;
	case 0x12:	// AY8910
		switch(SubType)
		{
		case 0x00:
			RetStr = "AY-3-8910";
			break;
		case 0x01:
			RetStr = "AY-3-8912";
			break;
		case 0x02:
			RetStr = "AY-3-8913";
			break;
		case 0x03:
			RetStr = "AY8930";
			break;
		case 0x04:
			RetStr = "AY-3-8914";
			break;
		case 0x10:
			RetStr = "YM2149";
			break;
		case 0x11:
			RetStr = "YM3439";
			break;
		case 0x12:
			RetStr = "YMZ284";
			break;
		case 0x13:
			RetStr = "YMZ294";
			break;
		}
		break;
	case 0x13:
		RetStr = "GB DMG";
		break;
	case 0x14:
		if (! (ChipID & 0x80))
			RetStr = "NES APU";
		else
			RetStr = "NES APU + FDS";
		break;
	case 0x1C:
		switch(SubType)
		{
		case 0x00:
		case 0x01:
			RetStr = "C140";
			break;
		case 0x02:
			RetStr = "C140 (219)";
			break;
		}
		break;
	case 0x21:
		RetStr = "WonderSwan";
		break;
	case 0x22:
		RetStr = "VSU-VUE";
		break;
	case 0x25:
		if (! (ChipID & 0x80))
			RetStr = "ES5505";
		else
			RetStr = "ES5506";
		break;
	case 0x28:
		RetStr = "Irem GA20";
		break;
	}
	// catch all default-cases
	if (RetStr == NULL)
		RetStr = GetChipName(ChipID & 0x7F);
	
	return RetStr;
}

UINT32 GetChipClock(VGM_HEADER* FileHead, UINT8 ChipID, UINT8* RetSubType)
{
	UINT32 Clock;
	UINT8 SubType;
	UINT8 CurChp;
	bool AllowBit31;
	
	SubType = 0x00;
	AllowBit31 = 0x00;
	switch(ChipID & 0x7F)
	{
	case 0x00:
		Clock = FileHead->lngHzPSG;
		AllowBit31 = 0x01;	// T6W28 Mode
		if (RetSubType != NULL && ! (Clock & 0x80000000))	// The T6W28 is handles differently.
		{
			switch(FileHead->bytPSG_SRWidth)
			{
			case 0x0F:	//  0x4000
				if (FileHead->bytPSG_Flags & 0x08)	// Clock Divider == 1?
					SubType = 0x05;	// SN94624
				else
					SubType = 0x01;	// SN76489
				break;
			case 0x10:	//  0x8000
				if (FileHead->shtPSG_Feedback == 0x0009)
					SubType = 0x07;	// SEGA PSG
				else if (FileHead->shtPSG_Feedback == 0x0022)
					SubType = 0x06;	// NCR7496
				break;
			case 0x11:	// 0x10000
				if (FileHead->bytPSG_Flags & 0x08)	// Clock Divider == 1?
					SubType = 0x03;	// SN76494
				else
					SubType = 0x02;	// SN76489A/SN76496
				break;
			}
			/*
								 FbMask  Noise Taps  Negate Stereo Dv Freq0		Fb	SR	Flags
				01 SN76489		 0x4000, 0x01, 0x02, TRUE,  FALSE, 8, TRUE		03	0F	07 (02|04|00|01) [unverified]
				02 SN76489A		0x10000, 0x04, 0x08, FALSE, FALSE, 8, TRUE		0C	11	05 (00|04|00|01)
				03 SN76494		0x10000, 0x04, 0x08, FALSE, FALSE, 1, TRUE		0C	11	0D (00|04|08|01)
				04 SN76496		0x10000, 0x04, 0x08, FALSE, FALSE, 8, TRUE		0C	11	05 (00|04|00|01) [same as SN76489A]
				05 SN94624		 0x4000, 0x01, 0x02, TRUE,  FALSE, 1, TRUE		03	0F	0F (02|04|08|01) [unverified, SN76489A without /8]
				06 NCR7496		 0x8000, 0x02, 0x20, FALSE, FALSE, 8, TRUE		22	10	05 (00|04|00|01) [unverified]
				07 GameGear PSG	 0x8000, 0x01, 0x08, TRUE,  TRUE,  8, FALSE		09	10	02 (02|00|00|00)
				07 SEGA VDP PSG	 0x8000, 0x01, 0x08, TRUE,  FALSE, 8, FALSE		09	10	06 (02|04|00|00)
				01 U8106		 0x4000, 0x01, 0x02, TRUE,  FALSE, 8, TRUE		03	0F	07 (02|04|00|01) [unverified, same as SN76489]
				02 Y2404		0x10000, 0x04, 0x08, FALSE, FALSE; 8, TRUE		0C	11	05 (00|04|00|01) [unverified, same as SN76489A]
				-- T6W28		 0x4000, 0x01, 0x04, ????,  FALSE, 8, ????		05	0F	?? (??|??|00|01) [It IS stereo, but not in GameGear way].
			*/
		}
		break;
	case 0x01:
		Clock = FileHead->lngHzYM2413;
		AllowBit31 = 0x01;	// VRC7 Mode
		break;
	case 0x02:
		Clock = FileHead->lngHzYM2612;
		break;
	case 0x03:
		Clock = FileHead->lngHzYM2151;
		break;
	case 0x04:
		Clock = FileHead->lngHzSPCM;
		break;
	case 0x05:
		if (ChipID & 0x80)
			return 0;
		Clock = FileHead->lngHzRF5C68;
		break;
	case 0x06:
		Clock = FileHead->lngHzYM2203;
		break;
	case 0x07:
		Clock = FileHead->lngHzYM2608;
		break;
	case 0x08:
		Clock = FileHead->lngHzYM2610;
		AllowBit31 = 0x01;	// YM2610B Mode
		break;
	case 0x09:
		Clock = FileHead->lngHzYM3812;
		AllowBit31 = 0x01;	// Dual OPL2, panned to the L/R speakers
		break;
	case 0x0A:
		Clock = FileHead->lngHzYM3526;
		break;
	case 0x0B:
		Clock = FileHead->lngHzY8950;
		break;
	case 0x0C:
		Clock = FileHead->lngHzYMF262;
		break;
	case 0x0D:
		Clock = FileHead->lngHzYMF278B;
		break;
	case 0x0E:
		Clock = FileHead->lngHzYMF271;
		break;
	case 0x0F:
		Clock = FileHead->lngHzYMZ280B;
		break;
	case 0x10:
		if (ChipID & 0x80)
			return 0;
		Clock = FileHead->lngHzRF5C164;
		AllowBit31 = 0x01;	// hack for Cosmic Fantasy Stories
		break;
	case 0x11:
		if (ChipID & 0x80)
			return 0;
		Clock = FileHead->lngHzPWM;
		break;
	case 0x12:
		Clock = FileHead->lngHzAY8910;
		SubType = FileHead->bytAYType;
		break;
	case 0x13:
		Clock = FileHead->lngHzGBDMG;
		break;
	case 0x14:
		Clock = FileHead->lngHzNESAPU;
		AllowBit31 = 0x01;	// FDS Enable
		break;
	case 0x15:
		Clock = FileHead->lngHzMultiPCM;
		break;
	case 0x16:
		Clock = FileHead->lngHzUPD7759;
		AllowBit31 = 0x01;	// Master/Slave Bit
		break;
	case 0x17:
		Clock = FileHead->lngHzOKIM6258;
		break;
	case 0x18:
		Clock = FileHead->lngHzOKIM6295;
		AllowBit31 = 0x01;	// Pin 7 State
		break;
	case 0x19:
		Clock = FileHead->lngHzK051649;
		break;
	case 0x1A:
		Clock = FileHead->lngHzK054539;
		break;
	case 0x1B:
		Clock = FileHead->lngHzHuC6280;
		break;
	case 0x1C:
		Clock = FileHead->lngHzC140;
		SubType = FileHead->bytC140Type;
		break;
	case 0x1D:
		Clock = FileHead->lngHzK053260;
		break;
	case 0x1E:
		Clock = FileHead->lngHzPokey;
		break;
	case 0x1F:
		if (ChipID & 0x80)
			return 0;
		Clock = FileHead->lngHzQSound;
		break;
	case 0x20:
		Clock = FileHead->lngHzSCSP;
		break;
	case 0x21:
		Clock = FileHead->lngHzWSwan;
		break;
	case 0x22:
		Clock = FileHead->lngHzVSU;
		break;
	case 0x23:
		Clock = FileHead->lngHzSAA1099;
		break;
	case 0x24:
		Clock = FileHead->lngHzES5503;
		break;
	case 0x25:
		Clock = FileHead->lngHzES5506;
		AllowBit31 = 0x01;	// ES5505/5506 switch
		break;
	case 0x26:
		Clock = FileHead->lngHzX1_010;
		break;
	case 0x27:
		Clock = FileHead->lngHzC352;
		break;
	case 0x28:
		Clock = FileHead->lngHzGA20;
		break;
	default:
		return 0;
	}
	if (ChipID & 0x80)
	{
		VGMX_CHP_EXTRA32* TempCX;
		
		if (! (Clock & 0x40000000))
			return 0;
		
		ChipID &= 0x7F;
		TempCX = &VGMH_Extra.Clocks;
		for (CurChp = 0x00; CurChp < TempCX->ChipCnt; CurChp ++)
		{
			if (TempCX->CCData[CurChp].Type == ChipID)
			{
				if (TempCX->CCData[CurChp].Data)
					Clock = TempCX->CCData[CurChp].Data;
				break;
			}
		}
	}
	
	if (RetSubType != NULL)
		*RetSubType = SubType;
	if (AllowBit31)
		return Clock & 0xBFFFFFFF;
	else
		return Clock & 0x3FFFFFFF;
}

static UINT16 GetChipVolume(VGM_HEADER* FileHead, UINT8 ChipID, UINT8 ChipNum, UINT8 ChipCnt)
{
	// ChipID: ID of Chip
	//		Bit 7 - Is Paired Chip
	// ChipNum: chip number (0 - first chip, 1 - second chip)
	// ChipCnt: chip volume divider (number of used chips)
	const UINT16 CHIP_VOLS[CHIP_COUNT] =
	{	0x80, 0x200/*0x155*/, 0x100, 0x100, 0x180, 0xB0, 0x100, 0x80,	// 00-07
		0x80, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x98,			// 08-0F
		0x80, 0xE0/*0xCD*/, 0x100, 0xC0, 0x100, 0x40, 0x11E, 0x1C0,		// 10-17
		0x100/*110*/, 0xA0, 0x100, 0x100, 0x100, 0xB3, 0x100, 0x100,	// 18-1F
		0x20, 0x100, 0x100, 0x100, 0x40, 0x20, 0x100, 0x40,			// 20-27
		0x280};
	UINT16 Volume;
	UINT8 CurChp;
	VGMX_CHP_EXTRA16* TempCX;
	VGMX_CHIP_DATA16* TempCD;
	
	Volume = CHIP_VOLS[ChipID & 0x7F];
	switch(ChipID)
	{
	case 0x00:	// SN76496
		// if T6W28, set Volume Divider to 01
		if (GetChipClock(&VGMHead, (ChipID << 7) | ChipID, NULL) & 0x80000000)
		{
			// The T6W28 consists of 2 "half" chips.
			ChipNum = 0x01;
			ChipCnt = 0x01;
		}
		break;
	case 0x18:	// OKIM6295
		// CP System 1 patch
		if (VGMTag.strSystemNameE != NULL && ! wcsncmp(VGMTag.strSystemNameE, L"CP", 0x02))
			Volume = 110;
		break;
	case 0x86:	// YM2203's AY
		Volume /= 2;
		break;
	case 0x87:	// YM2608's AY
		// The YM2608 outputs twice as loud as the YM2203 here.
		//Volume *= 1;
		break;
	case 0x88:	// YM2610's AY
		//Volume *= 1;
		break;
	}
	if (ChipCnt > 1)
		Volume /= ChipCnt;
	
	TempCX = &VGMH_Extra.Volumes;
	TempCD = TempCX->CCData;
	for (CurChp = 0x00; CurChp < TempCX->ChipCnt; CurChp ++, TempCD ++)
	{
		if (TempCD->Type == ChipID && (TempCD->Flags & 0x01) == ChipNum)
		{
			// Bit 15 - absolute/relative volume
			//	0 - absolute
			//	1 - relative (0x0100 = 1.0, 0x80 = 0.5, etc.)
			if (TempCD->Data & 0x8000)
				Volume = (Volume * (TempCD->Data & 0x7FFF) + 0x80) >> 8;
			else
			{
				Volume = TempCD->Data;
				if ((ChipID & 0x80) && DoubleSSGVol)
					Volume *= 2;
			}
			break;
		}
	}
	
	return Volume;
}


static void RestartPlaying(void)
{
	bool OldPThread;
	
	OldPThread = PauseThread;
	if (ThreadPauseEnable)
	{
		ThreadNoWait = false;
		ThreadPauseConfrm = false;
		PauseThread = true;
		while(! ThreadPauseConfrm)
			Sleep(1);	// Wait until the Thread is finished
	}
	Interpreting = true;	// Avoid any Thread-Call
	
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	VGMSmplPlayed = 0;
	VGMEnd = false;
	EndPlay = false;
	VGMCurLoop = 0x00;
	PauseSmpls = (PauseTime * SampleRate + 500) / 1000;
	
	Chips_GeneralActions(0x01);	// Reset Chips
	// also does Muting Mask (0x10) and Panning (0x20)
	
	if (UseFM)
	{
		open_real_fm();	// reset OPL chip and reload settings
		StartSkipping();
		AutoStopSkip = true;
	}
	
	Last95Drum = 0xFFFF;
	Last95Freq = 0;
	Interpreting = false;
	ForceVGMExec = true;
	IsVGMInit = true;
	InterpretFile(0);
	IsVGMInit = false;
	ForceVGMExec = false;
#ifndef CONSOLE_MODE
	FadePlay = false;
	MasterVol = 1.0f;
	FadeStart = 0;
	FinalVol = VolumeLevelM;
	PlayingTime = 0;
#endif
	PauseThread = OldPThread;
	
	return;
}

static void Chips_GeneralActions(UINT8 Mode)
{
	UINT32 AbsVol;
	//UINT16 ChipVol;
	CAUD_ATTR* CAA;
	CHIP_OPTS* COpt;
	UINT8 ChipCnt;
	UINT8 CurChip;
	UINT8 CurCSet;	// Chip Set
	UINT32 MaskVal;
	UINT32 ChipClk;
	
	switch(Mode)
	{
	case 0x00:	// Start Chips
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
			CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
			for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
			{
				CAA->SmpRate = 0x00;
				CAA->Volume = 0x00;
				CAA->ChipType = 0xFF;
				CAA->ChipID = CurCSet;
				CAA->Resampler = 0x00;
				CAA->StreamUpdate = &null_update;
				CAA->Paired = NULL;
			}
			CAA = CA_Paired[CurCSet];
			for (CurChip = 0x00; CurChip < 0x03; CurChip ++, CAA ++)
			{
				CAA->SmpRate = 0x00;
				CAA->Volume = 0x00;
				CAA->ChipType = 0xFF;
				CAA->ChipID = CurCSet;
				CAA->Resampler = 0x00;
				CAA->StreamUpdate = &null_update;
				CAA->Paired = NULL;
			}
		}
		
		// Initialize Sound Chips
		AbsVol = 0x00;
		if (VGMHead.lngHzPSG)
		{
			//ChipVol = UseFM ? 0x00 : 0x80;
			sn764xx_set_emu_core(ChipOpts[0x00].SN76496.EmuCore);
			ChipOpts[0x01].SN76496.EmuCore = ChipOpts[0x00].SN76496.EmuCore;
			
			ChipCnt = (VGMHead.lngHzPSG & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].SN76496;
				CAA->ChipType = 0x00;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				ChipClk &= ~0x80000000;
				ChipClk |= VGMHead.lngHzPSG & ((CurChip & 0x01) << 31);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_sn764xx(CurChip, ChipClk,
														VGMHead.bytPSG_SRWidth,
														VGMHead.shtPSG_Feedback,
														(VGMHead.bytPSG_Flags & 0x02) >> 1,
														(VGMHead.bytPSG_Flags & 0x04) >> 2,
														(VGMHead.bytPSG_Flags & 0x08) >> 3,
														(VGMHead.bytPSG_Flags & 0x01) >> 0);
					CAA->StreamUpdate = &sn764xx_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					if (! CurChip || ! (ChipClk & 0x80000000))
						AbsVol += CAA->Volume;
				}
				else
				{
					open_fm_option(CAA->ChipType, 0x00, ChipClk);
					open_fm_option(CAA->ChipType, 0x01, VGMHead.bytPSG_SRWidth);
					open_fm_option(CAA->ChipType, 0x02, VGMHead.shtPSG_Feedback);
					open_fm_option(CAA->ChipType, 0x04, (VGMHead.bytPSG_Flags & 0x02) >> 1);
					open_fm_option(CAA->ChipType, 0x05, (VGMHead.bytPSG_Flags & 0x04) >> 2);
					open_fm_option(CAA->ChipType, 0x06, (VGMHead.bytPSG_Flags & 0x08) >> 3);
					open_fm_option(CAA->ChipType, 0x07, (VGMHead.bytPSG_Flags & 0x01) >> 0);
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
			if (VGMHead.lngHzPSG & 0x80000000)
				ChipCnt = 0x01;
		}
		if (VGMHead.lngHzYM2413)
		{
			//ChipVol = UseFM ? 0x00 : 0x200/*0x155*/;
			if (! UseFM)
				ym2413_set_emu_core(ChipOpts[0x00].YM2413.EmuCore);
			else
				ym2413opl_set_emu_core(ChipOpts[0x00].YM2413.EmuCore);
			ChipOpts[0x01].YM2413.EmuCore = ChipOpts[0x00].YM2413.EmuCore;
			
			ChipCnt = (VGMHead.lngHzYM2413 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM2413;
				CAA->ChipType = 0x01;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_ym2413(CurChip, ChipClk);
					CAA->StreamUpdate = &ym2413_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					// WHY has this chip such a low volume???
					//AbsVol += (CAA->Volume + 1) * 3 / 4;
					AbsVol += CAA->Volume / 2;
				}
				else
				{
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
		}
		if (VGMHead.lngHzYM2612)
		{
			//ChipVol = 0x100;
			ym2612_set_emu_core(ChipOpts[0x00].YM2612.EmuCore);
			ym2612_set_options((UINT8)ChipOpts[0x00].YM2612.SpecialFlags);
			ChipOpts[0x01].YM2612.EmuCore = ChipOpts[0x00].YM2612.EmuCore;
			ChipOpts[0x01].YM2612.SpecialFlags = ChipOpts[0x00].YM2612.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzYM2612 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM2612;
				CAA->ChipType = 0x02;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ym2612(CurChip, ChipClk);
				CAA->StreamUpdate = &ym2612_stream_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzYM2151)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzYM2151 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM2151;
				CAA->ChipType = 0x03;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ym2151(CurChip, ChipClk);
				CAA->StreamUpdate = &ym2151_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzSPCM)
		{
			//ChipVol = 0x180;
			ChipCnt = (VGMHead.lngHzSPCM & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].SegaPCM;
				CAA->ChipType = 0x04;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_segapcm(CurChip, ChipClk, VGMHead.lngSPCMIntf);
				CAA->StreamUpdate = &SEGAPCM_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzRF5C68)
		{
			//ChipVol = 0xB0;	// that's right according to MAME, but it's almost too loud
			ChipCnt = 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].RF5C68;
				CAA->ChipType = 0x05;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_rf5c68(CurChip, ChipClk);
				CAA->StreamUpdate = &rf5c68_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzYM2203)
		{
			//ChipVol = 0x100;
			ym2203_set_ay_emu_core(ChipOpts[0x00].YM2203.EmuCore);
			ChipOpts[0x01].YM2203.EmuCore = ChipOpts[0x00].YM2203.EmuCore;
			ChipOpts[0x01].YM2203.SpecialFlags = ChipOpts[0x00].YM2203.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzYM2203 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM2203;
				COpt = &ChipOpts[CurChip].YM2203;
				CAA->ChipType = 0x06;
				CAA->Paired = &CA_Paired[CurChip][0x00];
				CAA->Paired->ChipType = 0x80 | CAA->ChipType;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ym2203(CurChip, ChipClk, COpt->SpecialFlags & 0x01,
													VGMHead.bytAYFlagYM2203,
													&CAA->Paired->SmpRate);
				CAA->StreamUpdate = &ym2203_stream_update;
				CAA->Paired->StreamUpdate = &ym2203_stream_update_ay;
				ym2203_set_srchg_cb(CurChip, &ChangeChipSampleRate, CAA, CAA->Paired);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				CAA->Paired->Volume = GetChipVolume(&VGMHead, CAA->Paired->ChipType,
													CurChip, ChipCnt);
				AbsVol += CAA->Volume + CAA->Paired->Volume;
			}
		}
		if (VGMHead.lngHzYM2608)
		{
			//ChipVol = 0x80;
			ym2608_set_ay_emu_core(ChipOpts[0x00].YM2608.EmuCore);
			ChipOpts[0x01].YM2608.EmuCore = ChipOpts[0x00].YM2608.EmuCore;
			ChipOpts[0x01].YM2608.SpecialFlags = ChipOpts[0x00].YM2608.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzYM2608 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM2608;
				COpt = &ChipOpts[CurChip].YM2608;
				CAA->ChipType = 0x07;
				CAA->Paired = &CA_Paired[CurChip][0x01];
				CAA->Paired->ChipType = 0x80 | CAA->ChipType;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ym2608(CurChip, ChipClk, COpt->SpecialFlags & 0x01,
													VGMHead.bytAYFlagYM2608,
													&CAA->Paired->SmpRate);
				CAA->StreamUpdate = &ym2608_stream_update;
				CAA->Paired->StreamUpdate = &ym2608_stream_update_ay;
				ym2608_set_srchg_cb(CurChip, &ChangeChipSampleRate, CAA, CAA->Paired);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				CAA->Paired->Volume = GetChipVolume(&VGMHead, CAA->Paired->ChipType,
													CurChip, ChipCnt);
				AbsVol += CAA->Volume + CAA->Paired->Volume;
				//CAA->Volume = ChipVol;
				//CAA->Paired->Volume = ChipVol * 2;
			}
		}
		if (VGMHead.lngHzYM2610)
		{
			//ChipVol = 0x80;
			ym2610_set_ay_emu_core(ChipOpts[0x00].YM2610.EmuCore);
			ChipOpts[0x01].YM2610.EmuCore = ChipOpts[0x00].YM2610.EmuCore;
			ChipOpts[0x01].YM2610.SpecialFlags = ChipOpts[0x00].YM2610.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzYM2610 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM2610;
				COpt = &ChipOpts[CurChip].YM2610;
				CAA->ChipType = 0x08;
				CAA->Paired = &CA_Paired[CurChip][0x02];
				CAA->Paired->ChipType = 0x80 | CAA->ChipType;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ym2610(CurChip, ChipClk, COpt->SpecialFlags & 0x01,
													&CAA->Paired->SmpRate);
				CAA->StreamUpdate = (ChipClk & 0x80000000) ? ym2610b_stream_update :
															ym2610_stream_update;
				CAA->Paired->StreamUpdate = &ym2610_stream_update_ay;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				CAA->Paired->Volume = GetChipVolume(&VGMHead, CAA->Paired->ChipType,
													CurChip, ChipCnt);
				AbsVol += CAA->Volume + CAA->Paired->Volume;
				//CAA->Volume = ChipVol;
				//CAA->Paired->Volume = ChipVol * 2;
			}
		}
		if (VGMHead.lngHzYM3812)
		{
			//ChipVol = UseFM ? 0x00 : 0x100;
			ym3812_set_emu_core(ChipOpts[0x00].YM3812.EmuCore);
			ChipOpts[0x01].YM3812.EmuCore = ChipOpts[0x00].YM3812.EmuCore;
			
			ChipCnt = (VGMHead.lngHzYM3812 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM3812;
				CAA->ChipType = 0x09;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_ym3812(CurChip, ChipClk);
					CAA->StreamUpdate = (ChipClk & 0x80000000) ? dual_opl2_stereo :
																ym3812_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					if (! CurChip || ! (ChipClk & 0x80000000))
						AbsVol += CAA->Volume * 2;
				}
				else
				{
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
		}
		if (VGMHead.lngHzYM3526)
		{
			//ChipVol = UseFM ? 0x00 : 0x100;
			ChipCnt = (VGMHead.lngHzYM3526 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YM3526;
				CAA->ChipType = 0x0A;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_ym3526(CurChip, ChipClk);
					CAA->StreamUpdate = &ym3526_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					AbsVol += CAA->Volume * 2;
				}
				else
				{
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
		}
		if (VGMHead.lngHzY8950)
		{
			//ChipVol = UseFM ? 0x00 : 0x100;
			ChipCnt = (VGMHead.lngHzY8950 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].Y8950;
				CAA->ChipType = 0x0B;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_y8950(CurChip, ChipClk);
					CAA->StreamUpdate = &y8950_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					AbsVol += CAA->Volume * 2;
				}
				else
				{
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
		}
		if (VGMHead.lngHzYMF262)
		{
			//ChipVol = UseFM ? 0x00 : 0x100;
			ymf262_set_emu_core(ChipOpts[0x00].YMF262.EmuCore);
			ChipOpts[0x01].YMF262.EmuCore = ChipOpts[0x00].YMF262.EmuCore;
			
			ChipCnt = (VGMHead.lngHzYMF262 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YMF262;
				CAA->ChipType = 0x0C;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_ymf262(CurChip, ChipClk);
					CAA->StreamUpdate = &ymf262_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					AbsVol += CAA->Volume * 2;
				}
				else
				{
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
		}
		if (VGMHead.lngHzYMF278B)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzYMF278B & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YMF278B;
				CAA->ChipType = 0x0D;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ymf278b(CurChip, ChipClk);
				CAA->StreamUpdate = &ymf278b_pcm_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;	//good as long as it only uses WaveTable Synth
			}
		}
		if (VGMHead.lngHzYMF271)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzYMF271 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YMF271;
				CAA->ChipType = 0x0E;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ymf271(CurChip, ChipClk);
				CAA->StreamUpdate = &ymf271_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzYMZ280B)
		{
			//ChipVol = 0x98;
			ChipCnt = (VGMHead.lngHzYMZ280B & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].YMZ280B;
				CAA->ChipType = 0x0F;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_ymz280b(CurChip, ChipClk);
				CAA->StreamUpdate = &ymz280b_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += (CAA->Volume * 0x20 / 0x13);
			}
		}
		if (VGMHead.lngHzRF5C164)
		{
			//ChipVol = 0x80;
			ChipCnt = 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].RF5C164;
				CAA->ChipType = 0x10;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_rf5c164(CurChip, ChipClk);
				CAA->StreamUpdate = &rf5c164_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 2;
			}
		}
		if (VGMHead.lngHzPWM)
		{
			//ChipVol = 0xE0;	// 0xCD
			ChipCnt = 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].PWM;
				CAA->ChipType = 0x11;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_pwm(CurChip, ChipClk);
				CAA->StreamUpdate = &pwm_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzAY8910)
		{
			//ChipVol = 0x100;
			ayxx_set_emu_core(ChipOpts[0x00].AY8910.EmuCore);
			ChipOpts[0x01].AY8910.EmuCore = ChipOpts[0x00].AY8910.EmuCore;
			
			ChipCnt = (VGMHead.lngHzAY8910 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].AY8910;
				CAA->ChipType = 0x12;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				if (! UseFM)
				{
					CAA->SmpRate = device_start_ayxx(CurChip, ChipClk,
													VGMHead.bytAYType, VGMHead.bytAYFlag);
					CAA->StreamUpdate = &ayxx_stream_update;
					
					CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
					AbsVol += CAA->Volume * 2;
				}
				else
				{
					open_fm_option(CAA->ChipType, 0x00, ChipClk);
					open_fm_option(CAA->ChipType, 0x01, VGMHead.bytAYType);
					open_fm_option(CAA->ChipType, 0x02, VGMHead.bytAYFlag);
					setup_real_fm(CAA->ChipType, CurChip);
					
					CAA->SmpRate = 0x00000000;
					CAA->Volume = 0x0000;
				}
			}
		}
		if (VGMHead.lngHzGBDMG)
		{
			//ChipVol = 0xC0;
			gameboy_sound_set_options((UINT8)ChipOpts[0x00].GameBoy.SpecialFlags);
			ChipOpts[0x01].GameBoy.SpecialFlags = ChipOpts[0x00].GameBoy.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzGBDMG & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].GameBoy;
				CAA->ChipType = 0x13;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_gameboy_sound(CurChip, ChipClk);
				CAA->StreamUpdate = &gameboy_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 2;
			}
		}
		if (VGMHead.lngHzNESAPU)
		{
			//ChipVol = 0x100;
			nes_set_emu_core(ChipOpts[0x00].NES.EmuCore);
			nes_set_options(ChipOpts[0x00].NES.SpecialFlags);
			ChipOpts[0x01].NES.EmuCore = ChipOpts[0x00].NES.EmuCore;
			ChipOpts[0x01].NES.SpecialFlags = ChipOpts[0x00].NES.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzNESAPU & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].NES;
				CAA->ChipType = 0x14;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_nes(CurChip, ChipClk);
				CAA->StreamUpdate = &nes_stream_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 2;
			}
		}
		if (VGMHead.lngHzMultiPCM)
		{
			//ChipVol = 0x40;
			ChipCnt = (VGMHead.lngHzMultiPCM & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].MultiPCM;
				CAA->ChipType = 0x15;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_multipcm(CurChip, ChipClk);
				CAA->StreamUpdate = &MultiPCM_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 4;
			}
		}
		if (VGMHead.lngHzUPD7759)
		{
			//ChipVol = 0x11E;
			ChipCnt = (VGMHead.lngHzUPD7759 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].UPD7759;
				CAA->ChipType = 0x16;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_upd7759(CurChip, ChipClk);
				CAA->StreamUpdate = &upd7759_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzOKIM6258)
		{
			//ChipVol = 0x1C0;
			okim6258_set_options(ChipOpts[0x00].OKIM6258.SpecialFlags);
			ChipOpts[0x01].OKIM6258.SpecialFlags = ChipOpts[0x00].OKIM6258.SpecialFlags;
			
			ChipCnt = (VGMHead.lngHzOKIM6258 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].OKIM6258;
				CAA->ChipType = 0x17;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_okim6258(CurChip, ChipClk,
													(VGMHead.bytOKI6258Flags & 0x03) >> 0,
													(VGMHead.bytOKI6258Flags & 0x04) >> 2,
													(VGMHead.bytOKI6258Flags & 0x08) >> 3);
				CAA->StreamUpdate = &okim6258_update;
				okim6258_set_srchg_cb(CurChip, &ChangeChipSampleRate, CAA);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 2;
			}
		}
		if (VGMHead.lngHzOKIM6295)
		{
			/*// Use the System Tag to decide between normal and CP System volume level.
			// I know, this is very hackish, but ATM there's no better solution.
			if (VGMTag.strSystemNameE != NULL && ! wcsncmp(VGMTag.strSystemNameE, L"CP", 0x02))
				ChipVol = 110;
			else
				ChipVol = 0x100;*/
			ChipCnt = (VGMHead.lngHzOKIM6295 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].OKIM6295;
				CAA->ChipType = 0x18;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_okim6295(CurChip, ChipClk);
				CAA->StreamUpdate = &okim6295_update;
				okim6295_set_srchg_cb(CurChip, &ChangeChipSampleRate, CAA);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 2;
			}
		}
		if (VGMHead.lngHzK051649)
		{
			//ChipVol = 0xA0;
			ChipCnt = (VGMHead.lngHzK051649 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].K051649;
				CAA->ChipType = 0x19;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_k051649(CurChip, ChipClk);
				CAA->StreamUpdate = &k051649_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzK054539)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzK054539 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].K054539;
				CAA->ChipType = 0x1A;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_k054539(CurChip, ChipClk);
				CAA->StreamUpdate = &k054539_update;
				k054539_init_flags(CurChip, VGMHead.bytK054539Flags);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzHuC6280)
		{
			//ChipVol = 0x100;
			c6280_set_emu_core(ChipOpts[0x00].HuC6280.EmuCore);
			ChipOpts[0x01].HuC6280.EmuCore = ChipOpts[0x00].HuC6280.EmuCore;
			
			ChipCnt = (VGMHead.lngHzHuC6280 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].HuC6280;
				CAA->ChipType = 0x1B;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_c6280(CurChip, ChipClk);
				CAA->StreamUpdate = &c6280_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzC140)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzC140 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].C140;
				CAA->ChipType = 0x1C;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_c140(CurChip, ChipClk, VGMHead.bytC140Type);
				CAA->StreamUpdate = &c140_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzK053260)
		{
			//ChipVol = 0xB3;
			ChipCnt = (VGMHead.lngHzK053260 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].K053260;
				CAA->ChipType = 0x1D;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_k053260(CurChip, ChipClk);
				CAA->StreamUpdate = &k053260_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzPokey)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzPokey & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].Pokey;
				CAA->ChipType = 0x1E;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_pokey(CurChip, ChipClk);
				CAA->StreamUpdate = &pokey_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzQSound)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzQSound & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].QSound;
				CAA->ChipType = 0x1F;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_qsound(CurChip, ChipClk);
				CAA->StreamUpdate = &qsound_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzSCSP)
		{
			scsp_set_options((UINT8)ChipOpts[0x00].SCSP.SpecialFlags);
			ChipOpts[0x01].SCSP.SpecialFlags = ChipOpts[0x00].SCSP.SpecialFlags;
			
			//ChipVol = 0x20;
			ChipCnt = (VGMHead.lngHzSCSP & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].SCSP;
				CAA->ChipType = 0x20;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_scsp(CurChip, ChipClk);
				CAA->StreamUpdate = &SCSP_Update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 8;
			}
		}
		if (VGMHead.lngHzWSwan)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzWSwan & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].WSwan;
				CAA->ChipType = 0x21;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = ws_audio_init(CurChip, ChipClk);
				CAA->StreamUpdate = &ws_audio_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzVSU)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzVSU & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].VSU;
				CAA->ChipType = 0x22;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_vsu(CurChip, ChipClk);
				CAA->StreamUpdate = &vsu_stream_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzSAA1099)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzSAA1099 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].SAA1099;
				CAA->ChipType = 0x23;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_saa1099(CurChip, ChipClk);
				CAA->StreamUpdate = &saa1099_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzES5503)
		{
			//ChipVol = 0x40;
			ChipCnt = (VGMHead.lngHzES5503 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].ES5503;
				CAA->ChipType = 0x24;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_es5503(CurChip, ChipClk, VGMHead.bytES5503Chns);
				CAA->StreamUpdate = &es5503_pcm_update;
				es5503_set_srchg_cb(CurChip, &ChangeChipSampleRate, CAA);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 8;
			}
		}
		if (VGMHead.lngHzES5506)
		{
			//ChipVol = 0x20;
			ChipCnt = (VGMHead.lngHzES5506 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].ES5506;
				CAA->ChipType = 0x25;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_es5506(CurChip, ChipClk, VGMHead.bytES5506Chns);
				CAA->StreamUpdate = &es5506_update;
				es5506_set_srchg_cb(CurChip, &ChangeChipSampleRate, CAA);
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 16;
			}
		}
		if (VGMHead.lngHzX1_010)
		{
			//ChipVol = 0x100;
			ChipCnt = (VGMHead.lngHzX1_010 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].X1_010;
				CAA->ChipType = 0x26;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_x1_010(CurChip, ChipClk);
				CAA->StreamUpdate = &seta_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		if (VGMHead.lngHzC352)
		{
			//ChipVol = 0x40;
			ChipCnt = (VGMHead.lngHzC352 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].C352;
				CAA->ChipType = 0x27;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_c352(CurChip, ChipClk, VGMHead.bytC352ClkDiv * 4);
				CAA->StreamUpdate = &c352_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume * 8;
			}
		}
		if (VGMHead.lngHzGA20)
		{
			//ChipVol = 0x280;
			ChipCnt = (VGMHead.lngHzGA20 & 0x40000000) ? 0x02 : 0x01;
			for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
			{
				CAA = &ChipAudio[CurChip].GA20;
				CAA->ChipType = 0x28;
				
				ChipClk = GetChipClock(&VGMHead, (CurChip << 7) | CAA->ChipType, NULL);
				CAA->SmpRate = device_start_iremga20(CurChip, ChipClk);
				CAA->StreamUpdate = &IremGA20_update;
				
				CAA->Volume = GetChipVolume(&VGMHead, CAA->ChipType, CurChip, ChipCnt);
				AbsVol += CAA->Volume;
			}
		}
		
		// Initialize DAC Control and PCM Bank
		DacCtrlUsed = 0x00;
		//memset(DacCtrlUsg, 0x00, 0x01 * 0xFF);
		for (CurChip = 0x00; CurChip < 0xFF; CurChip ++)
		{
			DacCtrl[CurChip].Enable = false;
		}
		//memset(DacCtrl, 0x00, sizeof(DACCTRL_DATA) * 0xFF);
		
		memset(PCMBank, 0x00, sizeof(VGM_PCM_BANK) * PCM_BANK_COUNT);
		memset(&PCMTbl, 0x00, sizeof(PCMBANK_TBL));
		
		// Reset chips
		Chips_GeneralActions(0x01);
		
		while(AbsVol < 0x200 && AbsVol)
		{
			for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
			{
				CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
				for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
					CAA->Volume *= 2;
				CAA = CA_Paired[CurCSet];
				for (CurChip = 0x00; CurChip < 0x03; CurChip ++, CAA ++)
					CAA->Volume *= 2;
			}
			AbsVol *= 2;
		}
		while(AbsVol > 0x300)
		{
			for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
			{
				CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
				for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
					CAA->Volume /= 2;
				CAA = CA_Paired[CurCSet];
				for (CurChip = 0x00; CurChip < 0x03; CurChip ++, CAA ++)
					CAA->Volume /= 2;
			}
			AbsVol /= 2;
		}
		
		// Initialize Resampler
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
			CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
			for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
				SetupResampler(CAA);
			
			CAA = CA_Paired[CurCSet];
			for (CurChip = 0x00; CurChip < 0x03; CurChip ++, CAA ++)
				SetupResampler(CAA);
		}
		
		GeneralChipLists();
		break;
	case 0x01:	// Reset chips
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
		
		CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
		{
			if (CAA->ChipType == 0xFF)	// chip unused
				continue;
			else if (CAA->ChipType == 0x00 && ! UseFM)
				device_reset_sn764xx(CurCSet);
			else if (CAA->ChipType == 0x01 && ! UseFM)
				device_reset_ym2413(CurCSet);
			else if (CAA->ChipType == 0x02)
				device_reset_ym2612(CurCSet);
			else if (CAA->ChipType == 0x03)
				device_reset_ym2151(CurCSet);
			else if (CAA->ChipType == 0x04)
				device_reset_segapcm(CurCSet);
			else if (CAA->ChipType == 0x05)
				device_reset_rf5c68(CurCSet);
			else if (CAA->ChipType == 0x06)
				device_reset_ym2203(CurCSet);
			else if (CAA->ChipType == 0x07)
				device_reset_ym2608(CurCSet);
			else if (CAA->ChipType == 0x08)
				device_reset_ym2610(CurCSet);
			else if (CAA->ChipType == 0x09)
			{
				if (! UseFM)
				{
					device_reset_ym3812(CurCSet);
				}
				if (FileMode == 0x01)
				{
					chip_reg_write(0x09, CurCSet, 0x00, 0x01, 0x20);	// Enable Waveform Select
					chip_reg_write(0x09, CurCSet, 0x00, 0xBD, 0xC0);	// Disable Rhythm Mode
				}
			}
			else if (CAA->ChipType == 0x0A && ! UseFM)
				device_reset_ym3526(CurCSet);
			else if (CAA->ChipType == 0x0B && ! UseFM)
				device_reset_y8950(CurCSet);
			else if (CAA->ChipType == 0x0C)
			{
				if (! UseFM)
				{
					device_reset_ymf262(CurCSet);
				}
				if (FileMode >= 0x01)
				{
					chip_reg_write(0x0C, CurCSet, 0x01, 0x05, 0x01);	// Enable OPL3-Mode
					chip_reg_write(0x0C, CurCSet, 0x00, 0xBD, 0xC0);	// Disable Rhythm Mode
					chip_reg_write(0x0C, CurCSet, 0x01, 0x04, 0x00);	// Disable 4-Op-Mode
				}
			}
			else if (CAA->ChipType == 0x0D)
				device_reset_ymf278b(CurCSet);
			else if (CAA->ChipType == 0x0E)
				device_reset_ymf271(CurCSet);
			else if (CAA->ChipType == 0x0F)
				device_reset_ymz280b(CurCSet);
			else if (CAA->ChipType == 0x10)
				device_reset_rf5c164(CurCSet);
			else if (CAA->ChipType == 0x11)
				device_reset_pwm(CurCSet);
			else if (CAA->ChipType == 0x12 && ! UseFM)
				device_reset_ayxx(CurCSet);
			else if (CAA->ChipType == 0x13)
				device_reset_gameboy_sound(CurCSet);
			else if (CAA->ChipType == 0x14)
				device_reset_nes(CurCSet);
			else if (CAA->ChipType == 0x15)
				device_reset_multipcm(CurCSet);
			else if (CAA->ChipType == 0x16)
				device_reset_upd7759(CurCSet);
			else if (CAA->ChipType == 0x17)
				device_reset_okim6258(CurCSet);
			else if (CAA->ChipType == 0x18)
				device_reset_okim6295(CurCSet);
			else if (CAA->ChipType == 0x19)
				device_reset_k051649(CurCSet);
			else if (CAA->ChipType == 0x1A)
				device_reset_k054539(CurCSet);
			else if (CAA->ChipType == 0x1B)
				device_reset_c6280(CurCSet);
			else if (CAA->ChipType == 0x1C)
				device_reset_c140(CurCSet);
			else if (CAA->ChipType == 0x1D)
				device_reset_k053260(CurCSet);
			else if (CAA->ChipType == 0x1E)
				device_reset_pokey(CurCSet);
			else if (CAA->ChipType == 0x1F)
				device_reset_qsound(CurCSet);
			else if (CAA->ChipType == 0x20)
				device_reset_scsp(CurCSet);
			else if (CAA->ChipType == 0x21)
				ws_audio_reset(CurCSet);
			else if (CAA->ChipType == 0x22)
				device_reset_vsu(CurCSet);
			else if (CAA->ChipType == 0x23)
				device_reset_saa1099(CurCSet);
			else if (CAA->ChipType == 0x24)
				device_reset_es5503(CurCSet);
			else if (CAA->ChipType == 0x25)
				device_reset_es5506(CurCSet);
			else if (CAA->ChipType == 0x26)
				device_reset_x1_010(CurCSet);
			else if (CAA->ChipType == 0x27)
				device_reset_c352(CurCSet);
			else if (CAA->ChipType == 0x28)
				device_reset_iremga20(CurCSet);
		}	// end for CurChip
		
		}	// end for CurCSet
		
		Chips_GeneralActions(0x10);	// set muting mask
		Chips_GeneralActions(0x20);	// set panning
		
		for (CurChip = 0x00; CurChip < DacCtrlUsed; CurChip ++)
		{
			CurCSet = DacCtrlUsg[CurChip];
			device_reset_daccontrol(CurCSet);
			//DacCtrl[CurCSet].Enable = false;
		}
		//DacCtrlUsed = 0x00;
		//memset(DacCtrlUsg, 0x00, 0x01 * 0xFF);
		
		for (CurChip = 0x00; CurChip < PCM_BANK_COUNT; CurChip ++)
		{
			// reset PCM Bank, but not the data
			// (this way I don't need to decompress the data again when restarting)
			PCMBank[CurChip].DataPos = 0x00000000;
			PCMBank[CurChip].BnkPos = 0x00000000;
		}
		PCMTbl.EntryCount = 0x00;
		break;
	case 0x02:	// Stop chips
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
		
		CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
		{
			if (CAA->ChipType == 0xFF)	// chip unused
				continue;
			else if (CAA->ChipType == 0x00 && ! UseFM)
				device_stop_sn764xx(CurCSet);
			else if (CAA->ChipType == 0x01 && ! UseFM)
				device_stop_ym2413(CurCSet);
			else if (CAA->ChipType == 0x02)
				device_stop_ym2612(CurCSet);
			else if (CAA->ChipType == 0x03)
				device_stop_ym2151(CurCSet);
			else if (CAA->ChipType == 0x04)
				device_stop_segapcm(CurCSet);
			else if (CAA->ChipType == 0x05)
				device_stop_rf5c68(CurCSet);
			else if (CAA->ChipType == 0x06)
				device_stop_ym2203(CurCSet);
			else if (CAA->ChipType == 0x07)
				device_stop_ym2608(CurCSet);
			else if (CAA->ChipType == 0x08)
				device_stop_ym2610(CurCSet);
			else if (CAA->ChipType == 0x09 && ! UseFM)
				device_stop_ym3812(CurCSet);
			else if (CAA->ChipType == 0x0A && ! UseFM)
				device_stop_ym3526(CurCSet);
			else if (CAA->ChipType == 0x0B && ! UseFM)
				device_stop_y8950(CurCSet);
			else if (CAA->ChipType == 0x0C && ! UseFM)
				device_stop_ymf262(CurCSet);
			else if (CAA->ChipType == 0x0D)
				device_stop_ymf278b(CurCSet);
			else if (CAA->ChipType == 0x0E)
				device_stop_ymf271(CurCSet);
			else if (CAA->ChipType == 0x0F)
				device_stop_ymz280b(CurCSet);
			else if (CAA->ChipType == 0x10)
				device_stop_rf5c164(CurCSet);
			else if (CAA->ChipType == 0x11)
				device_stop_pwm(CurCSet);
			else if (CAA->ChipType == 0x12 && ! UseFM)
				device_stop_ayxx(CurCSet);
			else if (CAA->ChipType == 0x13)
				device_stop_gameboy_sound(CurCSet);
			else if (CAA->ChipType == 0x14)
				device_stop_nes(CurCSet);
			else if (CAA->ChipType == 0x15)
				device_stop_multipcm(CurCSet);
			else if (CAA->ChipType == 0x16)
				device_stop_upd7759(CurCSet);
			else if (CAA->ChipType == 0x17)
				device_stop_okim6258(CurCSet);
			else if (CAA->ChipType == 0x18)
				device_stop_okim6295(CurCSet);
			else if (CAA->ChipType == 0x19)
				device_stop_k051649(CurCSet);
			else if (CAA->ChipType == 0x1A)
				device_stop_k054539(CurCSet);
			else if (CAA->ChipType == 0x1B)
				device_stop_c6280(CurCSet);
			else if (CAA->ChipType == 0x1C)
				device_stop_c140(CurCSet);
			else if (CAA->ChipType == 0x1D)
				device_stop_k053260(CurCSet);
			else if (CAA->ChipType == 0x1E)
				device_stop_pokey(CurCSet);
			else if (CAA->ChipType == 0x1F)
				device_stop_qsound(CurCSet);
			else if (CAA->ChipType == 0x20)
				device_stop_scsp(CurCSet);
			else if (CAA->ChipType == 0x21)
				ws_audio_done(CurCSet);
			else if (CAA->ChipType == 0x22)
				device_stop_vsu(CurCSet);
			else if (CAA->ChipType == 0x23)
				device_stop_saa1099(CurCSet);
			else if (CAA->ChipType == 0x24)
				device_stop_es5503(CurCSet);
			else if (CAA->ChipType == 0x25)
				device_stop_es5506(CurCSet);
			else if (CAA->ChipType == 0x26)
				device_stop_x1_010(CurCSet);
			else if (CAA->ChipType == 0x27)
				device_stop_c352(CurCSet);
			else if (CAA->ChipType == 0x28)
				device_stop_iremga20(CurCSet);
			
			CAA->ChipType = 0xFF;	// mark as "unused"
		}	// end for CurChip
		
		}	// end for CurCSet
		
		for (CurChip = 0x00; CurChip < DacCtrlUsed; CurChip ++)
		{
			CurCSet = DacCtrlUsg[CurChip];
			device_stop_daccontrol(CurCSet);
			DacCtrl[CurCSet].Enable = false;
		}
		DacCtrlUsed = 0x00;
		
		for (CurChip = 0x00; CurChip < PCM_BANK_COUNT; CurChip ++)
		{
			free(PCMBank[CurChip].Bank);
			free(PCMBank[CurChip].Data);
		}
		//memset(PCMBank, 0x00, sizeof(VGM_PCM_BANK) * PCM_BANK_COUNT);
		free(PCMTbl.Entries);
		//memset(&PCMTbl, 0x00, sizeof(PCMBANK_TBL));
		break;
	case 0x10:	// Set Muting Mask
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
		
		CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
		{
			if (CAA->ChipType == 0xFF)	// chip unused
				continue;
			else if (CAA->ChipType == 0x00 && ! UseFM)
				sn764xx_set_mute_mask(CurCSet, ChipOpts[CurCSet].SN76496.ChnMute1);
			else if (CAA->ChipType == 0x01 && ! UseFM)
				ym2413_set_mute_mask(CurCSet, ChipOpts[CurCSet].YM2413.ChnMute1);
			else if (CAA->ChipType == 0x02)
				ym2612_set_mute_mask(CurCSet, ChipOpts[CurCSet].YM2612.ChnMute1);
			else if (CAA->ChipType == 0x03)
				ym2151_set_mute_mask(CurCSet, ChipOpts[CurCSet].YM2151.ChnMute1);
			else if (CAA->ChipType == 0x04)
				segapcm_set_mute_mask(CurCSet, ChipOpts[CurCSet].SegaPCM.ChnMute1);
			else if (CAA->ChipType == 0x05)
				rf5c68_set_mute_mask(CurCSet, ChipOpts[CurCSet].RF5C68.ChnMute1);
			else if (CAA->ChipType == 0x06)
				ym2203_set_mute_mask(CurCSet, ChipOpts[CurCSet].YM2203.ChnMute1,
									ChipOpts[CurCSet].YM2203.ChnMute3);
			else if (CAA->ChipType == 0x07)
			{
				MaskVal  = (ChipOpts[CurCSet].YM2608.ChnMute1 & 0x3F) << 0;
				MaskVal |= (ChipOpts[CurCSet].YM2608.ChnMute2 & 0x7F) << 6;
				ym2608_set_mute_mask(CurCSet, MaskVal, ChipOpts[CurCSet].YM2608.ChnMute3);
			}
			else if (CAA->ChipType == 0x08)
			{
				MaskVal  = (ChipOpts[CurCSet].YM2610.ChnMute1 & 0x3F) << 0;
				MaskVal |= (ChipOpts[CurCSet].YM2610.ChnMute2 & 0x7F) << 6;
				ym2610_set_mute_mask(CurCSet, MaskVal, ChipOpts[CurCSet].YM2610.ChnMute3);
			}
			else if (CAA->ChipType == 0x09 && ! UseFM)
				ym3812_set_mute_mask(CurCSet, ChipOpts[CurCSet].YM3812.ChnMute1);
			else if (CAA->ChipType == 0x0A && ! UseFM)
				ym3526_set_mute_mask(CurCSet, ChipOpts[CurCSet].YM3526.ChnMute1);
			else if (CAA->ChipType == 0x0B && ! UseFM)
				y8950_set_mute_mask(CurCSet, ChipOpts[CurCSet].Y8950.ChnMute1);
			else if (CAA->ChipType == 0x0C && ! UseFM)
				ymf262_set_mute_mask(CurCSet, ChipOpts[CurCSet].YMF262.ChnMute1);
			else if (CAA->ChipType == 0x0D)
				ymf278b_set_mute_mask(CurCSet, ChipOpts[CurCSet].YMF278B.ChnMute1,
										ChipOpts[CurCSet].YMF278B.ChnMute2);
			else if (CAA->ChipType == 0x0E)
				ymf271_set_mute_mask(CurCSet, ChipOpts[CurCSet].YMF271.ChnMute1);
			else if (CAA->ChipType == 0x0F)
				ymz280b_set_mute_mask(CurCSet, ChipOpts[CurCSet].YMZ280B.ChnMute1);
			else if (CAA->ChipType == 0x10)
				rf5c164_set_mute_mask(CurCSet, ChipOpts[CurCSet].RF5C164.ChnMute1);
			else if (CAA->ChipType == 0x11)
				;	// PWM - nothing to mute
			else if (CAA->ChipType == 0x12 && ! UseFM)
				ayxx_set_mute_mask(CurCSet, ChipOpts[CurCSet].AY8910.ChnMute1);
			else if (CAA->ChipType == 0x13)
				gameboy_sound_set_mute_mask(CurCSet, ChipOpts[CurCSet].GameBoy.ChnMute1);
			else if (CAA->ChipType == 0x14)
				nes_set_mute_mask(CurCSet, ChipOpts[CurCSet].NES.ChnMute1);
			else if (CAA->ChipType == 0x15)
				multipcm_set_mute_mask(CurCSet, ChipOpts[CurCSet].MultiPCM.ChnMute1);
			else if (CAA->ChipType == 0x16)
				;	// UPD7759 - nothing to mute
			else if (CAA->ChipType == 0x17)
				;	// OKIM6258 - nothing to mute
			else if (CAA->ChipType == 0x18)
				okim6295_set_mute_mask(CurCSet, ChipOpts[CurCSet].OKIM6295.ChnMute1);
			else if (CAA->ChipType == 0x19)
				k051649_set_mute_mask(CurCSet, ChipOpts[CurCSet].K051649.ChnMute1);
			else if (CAA->ChipType == 0x1A)
				k054539_set_mute_mask(CurCSet, ChipOpts[CurCSet].K054539.ChnMute1);
			else if (CAA->ChipType == 0x1B)
				c6280_set_mute_mask(CurCSet, ChipOpts[CurCSet].HuC6280.ChnMute1);
			else if (CAA->ChipType == 0x1C)
				c140_set_mute_mask(CurCSet, ChipOpts[CurCSet].C140.ChnMute1);
			else if (CAA->ChipType == 0x1D)
				k053260_set_mute_mask(CurCSet, ChipOpts[CurCSet].K053260.ChnMute1);
			else if (CAA->ChipType == 0x1E)
				pokey_set_mute_mask(CurCSet, ChipOpts[CurCSet].Pokey.ChnMute1);
			else if (CAA->ChipType == 0x1F)
				qsound_set_mute_mask(CurCSet, ChipOpts[CurCSet].QSound.ChnMute1);
			else if (CAA->ChipType == 0x20)
				scsp_set_mute_mask(CurCSet, ChipOpts[CurCSet].SCSP.ChnMute1);
			else if (CAA->ChipType == 0x21)
				ws_set_mute_mask(CurCSet, ChipOpts[CurCSet].WSwan.ChnMute1);
			else if (CAA->ChipType == 0x22)
				vsu_set_mute_mask(CurCSet, ChipOpts[CurCSet].VSU.ChnMute1);
			else if (CAA->ChipType == 0x23)
				saa1099_set_mute_mask(CurCSet, ChipOpts[CurCSet].SAA1099.ChnMute1);
			else if (CAA->ChipType == 0x24)
				es5503_set_mute_mask(CurCSet, ChipOpts[CurCSet].ES5503.ChnMute1);
			else if (CAA->ChipType == 0x25)
				es5506_set_mute_mask(CurCSet, ChipOpts[CurCSet].ES5506.ChnMute1);
			else if (CAA->ChipType == 0x26)
				x1_010_set_mute_mask(CurCSet, ChipOpts[CurCSet].X1_010.ChnMute1);
			else if (CAA->ChipType == 0x27)
				c352_set_mute_mask(CurCSet, ChipOpts[CurCSet].C352.ChnMute1);
			else if (CAA->ChipType == 0x28)
				iremga20_set_mute_mask(CurCSet, ChipOpts[CurCSet].GA20.ChnMute1);
		}	// end for CurChip
		
		}	// end for CurCSet
		break;
	case 0x20:	// Set Panning
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
		
		CAA = (CAUD_ATTR*)&ChipAudio[CurCSet];
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, CAA ++)
		{
			if (CAA->ChipType == 0xFF)	// chip unused
				continue;
			else if (CAA->ChipType == 0x00 && ! UseFM)
				sn764xx_set_panning(CurCSet, ChipOpts[CurCSet].SN76496.Panning);
			else if (CAA->ChipType == 0x01 && ! UseFM)
				ym2413_set_panning(CurCSet, ChipOpts[CurCSet].YM2413.Panning);
		}	// end for CurChip
		
		}	// end for CurCSet
		break;
	}
	
	return;
}

INLINE INT32 SampleVGM2Pbk_I(INT32 SampleVal)
{
	return (INT32)((INT64)SampleVal * VGMSmplRateMul / VGMSmplRateDiv);
}

INLINE INT32 SamplePbk2VGM_I(INT32 SampleVal)
{
	return (INT32)((INT64)SampleVal * VGMSmplRateDiv / VGMSmplRateMul);
}

INT32 SampleVGM2Playback(INT32 SampleVal)
{
	return (INT32)((INT64)SampleVal * VGMSmplRateMul / VGMSmplRateDiv);
}

INT32 SamplePlayback2VGM(INT32 SampleVal)
{
	return (INT32)((INT64)SampleVal * VGMSmplRateDiv / VGMSmplRateMul);
}

static UINT8 StartThread(void)
{
#ifdef WIN32
	HANDLE PlayThreadHandle;
	DWORD PlayThreadID;
	//char TestStr[0x80];
	
	if (PlayThreadOpen)
		return 0xD0;	// Thread is already active
	
	PauseThread = true;
	ThreadNoWait = false;
	ThreadPauseConfrm = false;
	CloseThread = false;
	ThreadPauseEnable = true;
	
	PlayThreadHandle = CreateThread(NULL, 0x00, &PlayingThread, NULL, 0x00, &PlayThreadID);
	if (PlayThreadHandle == NULL)
		return 0xC8;		// CreateThread failed
	CloseHandle(PlayThreadHandle);
	
	PlayThreadOpen = true;
	//PauseThread = false;	is done after File Init
	
	return 0x00;
#else
	UINT32 RetVal;
	
	PauseThread = true;
	ThreadNoWait = false;
	ThreadPauseConfrm = false;
	CloseThread = false;
	ThreadPauseEnable = true;
	
	RetVal = pthread_create(&hPlayThread, NULL, &PlayingThread, NULL);
	if (RetVal)
		return 0xC8;		// CreateThread failed
	
	PlayThreadOpen = true;
	
	return 0x00;
#endif
}

static UINT8 StopThread(void)
{
#ifdef WIN32
	UINT16 Cnt;
#endif
	
	if (! PlayThreadOpen)
		return 0xD8;	// Thread is not active
	
#ifdef WIN32
	CloseThread = true;
	for (Cnt = 0; Cnt < 100; Cnt ++)
	{
		Sleep(1);
		if (hPlayThread == NULL)
			break;
	}
#else
	CloseThread = true;
	pthread_join(hPlayThread, NULL);
#endif
	PlayThreadOpen = false;
	ThreadPauseEnable = false;
	
	return 0x00;
}

#if defined(WIN32) && defined(MIXER_MUTING)
//static bool GetMixerControl(HMIXEROBJ hmixer, MIXERCONTROL* mxc)
static bool GetMixerControl(void)
{
	// This function attempts to obtain a mixer control. Returns True if successful.
	MIXERLINECONTROLS mxlc;
	MIXERLINE mxl;
	HGLOBAL hmem;
	MMRESULT RetVal;
	
	mxl.cbStruct = sizeof(MIXERLINE);
	mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER;
	// Obtain a line corresponding to the component type
	RetVal = mixerGetLineInfo((HMIXEROBJ)hmixer, &mxl, MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (RetVal != MMSYSERR_NOERROR)
		return false;
	
	mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
	mxlc.dwLineID = mxl.dwLineID;
	mxlc.dwControlID = MIXERCONTROL_CONTROLTYPE_MUTE;
	mxlc.cControls = 1;
	mxlc.cbmxctrl = sizeof(MIXERCONTROL);
	
	// Allocate a buffer for the control
	hmem = GlobalAlloc(0x40, sizeof(MIXERCONTROL));
	mxlc.pamxctrl = (MIXERCONTROL*)GlobalLock(hmem);
	mixctrl.cbStruct = sizeof(MIXERCONTROL);
	
	// Get the control
	RetVal = mixerGetLineControls((HMIXEROBJ)hmixer, &mxlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);
	if (RetVal != MMSYSERR_NOERROR)
	{
		GlobalFree(hmem);
		return false;
	}
	
	// Copy the control into the destination structure
	//memcpy(mixctrl, mxlc.pamxctrl, sizeof(MIXERCONTROL));
	mixctrl = *mxlc.pamxctrl;
	GlobalFree(hmem);
	
	return true;
}
#endif

//static bool SetMuteControl(HMIXEROBJ hmixer, MIXERCONTROL* mxc, bool mute)
static bool SetMuteControl(bool mute)
{
#ifdef MIXER_MUTING

#ifdef WIN32
	MIXERCONTROLDETAILS mxcd;
	MIXERCONTROLDETAILS_UNSIGNED vol;
	HGLOBAL hmem;
	MMRESULT RetVal;
	
	mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
	mxcd.dwControlID = mixctrl.dwControlID;
	mxcd.cChannels = 1;
	mxcd.cMultipleItems = 0;
	mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	
	hmem = GlobalAlloc(0x40, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	mxcd.paDetails = GlobalLock(hmem);
	vol.dwValue = mute;
	
	memcpy(mxcd.paDetails, &vol, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	RetVal = mixerSetControlDetails((HMIXEROBJ)hmixer, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE);
	GlobalFree(hmem);
	
	if (RetVal != MMSYSERR_NOERROR)
		return false;
	
	return true;
#else
	UINT16 mix_vol;
	int RetVal;
	
	ioctl(hmixer, MIXER_READ(SOUND_MIXER_SYNTH), &mix_vol);
	if (mix_vol)
		mixer_vol = mix_vol;
	mix_vol = mute ? 0x0000: mixer_vol;
	
	RetVal = ioctl(hmixer, MIXER_WRITE(SOUND_MIXER_SYNTH), &mix_vol);
	
	return ! RetVal;
#endif

#else	//#indef MIXER_MUTING
	float TempVol;
	
	TempVol = MasterVol;
	if (TempVol > 0.0f)
		VolumeBak = TempVol;
	
	MasterVol = mute ? 0.0f : VolumeBak;
	FinalVol = VolumeLevelM * MasterVol * MasterVol;
	RefreshVolume();
	
	return true;
#endif
}



static void InterpretFile(UINT32 SampleCount)
{
	UINT32 TempLng;
	UINT8 CurChip;
	
	//if (Interpreting && SampleCount == 1)
	//	return;
	while(Interpreting)
		Sleep(1);
	
	if (DacCtrlUsed && SampleCount > 1)	// handle skipping
	{
		for (CurChip = 0x00; CurChip < DacCtrlUsed; CurChip ++)
		{
			daccontrol_update(DacCtrlUsg[CurChip], SampleCount - 1);
		}
	}
	
	Interpreting = true;
	if (! FileMode)
		InterpretVGM(SampleCount);
#ifdef ADDITIONAL_FORMATS
	else
		InterpretOther(SampleCount);
#endif
	
	if (DacCtrlUsed && SampleCount)
	{
		// calling this here makes "Emulating while Paused" nicer
		for (CurChip = 0x00; CurChip < DacCtrlUsed; CurChip ++)
		{
			daccontrol_update(DacCtrlUsg[CurChip], 1);
		}
	}
	
	if (UseFM && FadePlay)
	{
		//TempLng = PlayingTime % (SampleRate / 5);
		//if (! TempLng)
		TempLng = PlayingTime / (SampleRate / 5) -
					(PlayingTime + SampleCount) / (SampleRate / 5);
		if (TempLng)
			RefreshVolume();
	}
	if (AutoStopSkip && SampleCount)
	{
		StopSkipping();
		AutoStopSkip = false;
		ResetPBTimer = true;
	}
	
	if (! PausePlay || ForceVGMExec)
		VGMSmplPlayed += SampleCount;
	PlayingTime += SampleCount;
	
	//if (FadePlay && ! FadeTime)
	//	EndPlay = true;
	
	Interpreting = false;
	
	return;
}

static void AddPCMData(UINT8 Type, UINT32 DataSize, const UINT8* Data)
{
	UINT32 CurBnk;
	VGM_PCM_BANK* TempPCM;
	VGM_PCM_DATA* TempBnk;
	UINT32 BankSize;
	bool RetVal;
	UINT8 BnkType;
	UINT8 CurDAC;
	
	BnkType = Type & 0x3F;
	if (BnkType >= PCM_BANK_COUNT || VGMCurLoop)
		return;
	
	if (Type == 0x7F)
	{
		ReadPCMTable(DataSize, Data);
		return;
	}
	
	TempPCM = &PCMBank[BnkType];
	TempPCM->BnkPos ++;
	if (TempPCM->BnkPos <= TempPCM->BankCount)
		return;	// Speed hack for restarting playback (skip already loaded blocks)
	CurBnk = TempPCM->BankCount;
	TempPCM->BankCount ++;
	if (Last95Max != 0xFFFF)
		Last95Max = TempPCM->BankCount;
	TempPCM->Bank = (VGM_PCM_DATA*)realloc(TempPCM->Bank,
											sizeof(VGM_PCM_DATA) * TempPCM->BankCount);
	
	if (! (Type & 0x40))
		BankSize = DataSize;
	else
		BankSize = ReadLE32(&Data[0x01]);
	TempPCM->Data = realloc(TempPCM->Data, TempPCM->DataSize + BankSize);
	TempBnk = &TempPCM->Bank[CurBnk];
	TempBnk->DataStart = TempPCM->DataSize;
	if (! (Type & 0x40))
	{
		TempBnk->DataSize = DataSize;
		TempBnk->Data = TempPCM->Data + TempBnk->DataStart;
		memcpy(TempBnk->Data, Data, DataSize);
	}
	else
	{
		TempBnk->Data = TempPCM->Data + TempBnk->DataStart;
		RetVal = DecompressDataBlk(TempBnk, DataSize, Data);
		if (! RetVal)
		{
			TempBnk->Data = NULL;
			TempBnk->DataSize = 0x00;
			//return;
			goto RefreshDACStrm;	// sorry for the goto, but I don't want to copy-paste the code
		}
	}
	if (BankSize != TempBnk->DataSize)
		printf("Error reading Data Block! Data Size conflict!\n");
	TempPCM->DataSize += BankSize;
	
	// realloc may've moved the Bank block, so refresh all DAC Streams
RefreshDACStrm:
	for (CurDAC = 0x00; CurDAC < DacCtrlUsed; CurDAC ++)
	{
		if (DacCtrl[DacCtrlUsg[CurDAC]].Bank == BnkType)
			daccontrol_refresh_data(DacCtrlUsg[CurDAC], TempPCM->Data, TempPCM->DataSize);
	}
	
	return;
}

/*INLINE FUINT16 ReadBits(UINT8* Data, UINT32* Pos, FUINT8* BitPos, FUINT8 BitsToRead)
{
	FUINT8 BitReadVal;
	UINT32 InPos;
	FUINT8 InVal;
	FUINT8 BitMask;
	FUINT8 InShift;
	FUINT8 OutBit;
	FUINT16 RetVal;
	
	InPos = *Pos;
	InShift = *BitPos;
	OutBit = 0x00;
	RetVal = 0x0000;
	while(BitsToRead)
	{
		BitReadVal = (BitsToRead >= 8) ? 8 : BitsToRead;
		BitsToRead -= BitReadVal;
		BitMask = (1 << BitReadVal) - 1;
		
		InShift += BitReadVal;
		InVal = (Data[InPos] << InShift >> 8) & BitMask;
		if (InShift >= 8)
		{
			InShift -= 8;
			InPos ++;
			if (InShift)
				InVal |= (Data[InPos] << InShift >> 8) & BitMask;
		}
		
		RetVal |= InVal << OutBit;
		OutBit += BitReadVal;
	}
	
	*Pos = InPos;
	*BitPos = InShift;
	return RetVal;
}

static void DecompressDataBlk(VGM_PCM_DATA* Bank, UINT32 DataSize, const UINT8* Data)
{
	UINT8 ComprType;
	UINT8 BitDec;
	FUINT8 BitCmp;
	UINT8 CmpSubType;
	UINT16 AddVal;
	UINT32 InPos;
	UINT32 OutPos;
	FUINT16 InVal;
	FUINT16 OutVal;
	FUINT8 ValSize;
	FUINT8 InShift;
	FUINT8 OutShift;
	UINT8* Ent1B;
	UINT16* Ent2B;
	//UINT32 Time;
	
	//Time = GetTickCount();
	ComprType = Data[0x00];
	Bank->DataSize = ReadLE32(&Data[0x01]);
	BitDec = Data[0x05];
	BitCmp = Data[0x06];
	CmpSubType = Data[0x07];
	AddVal = ReadLE16(&Data[0x08]);
	
	switch(ComprType)
	{
	case 0x00:	// n-Bit compression
		if (CmpSubType == 0x02)
		{
			Ent1B = (UINT8*)PCMTbl.Entries;
			Ent2B = (UINT16*)PCMTbl.Entries;
			if (! PCMTbl.EntryCount)
			{
				printf("Error loading table-compressed data block! No table loaded!\n");
				return;
			}
			else if (BitDec != PCMTbl.BitDec || BitCmp != PCMTbl.BitCmp)
			{
				printf("Warning! Data block and loaded value table incompatible!\n");
				return;
			}
		}
		
		ValSize = (BitDec + 7) / 8;
		InPos = 0x0A;
		InShift = 0;
		OutShift = BitDec - BitCmp;
		
		for (OutPos = 0x00; OutPos < Bank->DataSize; OutPos += ValSize)
		{
			if (InPos >= DataSize)
				break;
			InVal = ReadBits(Data, &InPos, &InShift, BitCmp);
			switch(CmpSubType)
			{
			case 0x00:	// Copy
				OutVal = InVal + AddVal;
				break;
			case 0x01:	// Shift Left
				OutVal = (InVal << OutShift) + AddVal;
				break;
			case 0x02:	// Table
				switch(ValSize)
				{
				case 0x01:
					OutVal = Ent1B[InVal];
					break;
				case 0x02:
					OutVal = Ent2B[InVal];
					break;
				}
				break;
			}
			memcpy(&Bank->Data[OutPos], &OutVal, ValSize);
		}
		break;
	}
	
	//Time = GetTickCount() - Time;
	//printf("Decompression Time: %lu\n", Time);
	
	return;
}*/

static bool DecompressDataBlk(VGM_PCM_DATA* Bank, UINT32 DataSize, const UINT8* Data)
{
	UINT8 ComprType;
	UINT8 BitDec;
	FUINT8 BitCmp;
	UINT8 CmpSubType;
	UINT16 AddVal;
	const UINT8* InPos;
	const UINT8* InDataEnd;
	UINT8* OutPos;
	const UINT8* OutDataEnd;
	FUINT16 InVal;
	FUINT16 OutVal;
	FUINT8 ValSize;
	FUINT8 InShift;
	FUINT8 OutShift;
	UINT8* Ent1B;
	UINT16* Ent2B;
#if defined(_DEBUG) && defined(WIN32)
	UINT32 Time;
#endif
	
	// ReadBits Variables
	FUINT8 BitsToRead;
	FUINT8 BitReadVal;
	FUINT8 InValB;
	FUINT8 BitMask;
	FUINT8 OutBit;
	
	// Variables for DPCM
	UINT16 OutMask;
	
#if defined(_DEBUG) && defined(WIN32)
	Time = GetTickCount();
#endif
	ComprType = Data[0x00];
	Bank->DataSize = ReadLE32(&Data[0x01]);
	
	switch(ComprType)
	{
	case 0x00:	// n-Bit compression
		BitDec = Data[0x05];
		BitCmp = Data[0x06];
		CmpSubType = Data[0x07];
		AddVal = ReadLE16(&Data[0x08]);
		
		if (CmpSubType == 0x02)
		{
			Ent1B = (UINT8*)PCMTbl.Entries;	// Big Endian note: Those are stored in LE and converted when reading.
			Ent2B = (UINT16*)PCMTbl.Entries;
			if (! PCMTbl.EntryCount)
			{
				Bank->DataSize = 0x00;
				printf("Error loading table-compressed data block! No table loaded!\n");
				return false;
			}
			else if (BitDec != PCMTbl.BitDec || BitCmp != PCMTbl.BitCmp)
			{
				Bank->DataSize = 0x00;
				printf("Warning! Data block and loaded value table incompatible!\n");
				return false;
			}
		}
		
		ValSize = (BitDec + 7) / 8;
		InPos = Data + 0x0A;
		InDataEnd = Data + DataSize;
		InShift = 0;
		OutShift = BitDec - BitCmp;
		OutDataEnd = Bank->Data + Bank->DataSize;
		
		for (OutPos = Bank->Data; OutPos < OutDataEnd && InPos < InDataEnd; OutPos += ValSize)
		{
			//InVal = ReadBits(Data, InPos, &InShift, BitCmp);
			// inlined - is 30% faster
			OutBit = 0x00;
			InVal = 0x0000;
			BitsToRead = BitCmp;
			while(BitsToRead)
			{
				BitReadVal = (BitsToRead >= 8) ? 8 : BitsToRead;
				BitsToRead -= BitReadVal;
				BitMask = (1 << BitReadVal) - 1;
				
				InShift += BitReadVal;
				InValB = (*InPos << InShift >> 8) & BitMask;
				if (InShift >= 8)
				{
					InShift -= 8;
					InPos ++;
					if (InShift)
						InValB |= (*InPos << InShift >> 8) & BitMask;
				}
				
				InVal |= InValB << OutBit;
				OutBit += BitReadVal;
			}
			
			switch(CmpSubType)
			{
			case 0x00:	// Copy
				OutVal = InVal + AddVal;
				break;
			case 0x01:	// Shift Left
				OutVal = (InVal << OutShift) + AddVal;
				break;
			case 0x02:	// Table
				switch(ValSize)
				{
				case 0x01:
					OutVal = Ent1B[InVal];
					break;
				case 0x02:
#ifndef VGM_BIG_ENDIAN
					OutVal = Ent2B[InVal];
#else
					OutVal = ReadLE16((UINT8*)&Ent2B[InVal]);
#endif
					break;
				}
				break;
			}
			
#ifndef VGM_BIG_ENDIAN
			//memcpy(OutPos, &OutVal, ValSize);
			if (ValSize == 0x01)
				*((UINT8*)OutPos) = (UINT8)OutVal;
			else //if (ValSize == 0x02)
				*((UINT16*)OutPos) = (UINT16)OutVal;
#else
			if (ValSize == 0x01)
			{
				*OutPos = (UINT8)OutVal;
			}
			else //if (ValSize == 0x02)
			{
				OutPos[0x00] = (UINT8)((OutVal & 0x00FF) >> 0);
				OutPos[0x01] = (UINT8)((OutVal & 0xFF00) >> 8);
			}
#endif
		}
		break;
	case 0x01:	// Delta-PCM
		BitDec = Data[0x05];
		BitCmp = Data[0x06];
		OutVal = ReadLE16(&Data[0x08]);
		
		Ent1B = (UINT8*)PCMTbl.Entries;
		Ent2B = (UINT16*)PCMTbl.Entries;
		if (! PCMTbl.EntryCount)
		{
			Bank->DataSize = 0x00;
			printf("Error loading table-compressed data block! No table loaded!\n");
			return false;
		}
		else if (BitDec != PCMTbl.BitDec || BitCmp != PCMTbl.BitCmp)
		{
			Bank->DataSize = 0x00;
			printf("Warning! Data block and loaded value table incompatible!\n");
			return false;
		}
		
		ValSize = (BitDec + 7) / 8;
		OutMask = (1 << BitDec) - 1;
		InPos = Data + 0x0A;
		InDataEnd = Data + DataSize;
		InShift = 0;
		OutShift = BitDec - BitCmp;
		OutDataEnd = Bank->Data + Bank->DataSize;
		AddVal = 0x0000;
		
		for (OutPos = Bank->Data; OutPos < OutDataEnd && InPos < InDataEnd; OutPos += ValSize)
		{
			//InVal = ReadBits(Data, InPos, &InShift, BitCmp);
			// inlined - is 30% faster
			OutBit = 0x00;
			InVal = 0x0000;
			BitsToRead = BitCmp;
			while(BitsToRead)
			{
				BitReadVal = (BitsToRead >= 8) ? 8 : BitsToRead;
				BitsToRead -= BitReadVal;
				BitMask = (1 << BitReadVal) - 1;
				
				InShift += BitReadVal;
				InValB = (*InPos << InShift >> 8) & BitMask;
				if (InShift >= 8)
				{
					InShift -= 8;
					InPos ++;
					if (InShift)
						InValB |= (*InPos << InShift >> 8) & BitMask;
				}
				
				InVal |= InValB << OutBit;
				OutBit += BitReadVal;
			}
			
			switch(ValSize)
			{
			case 0x01:
				AddVal = Ent1B[InVal];
				OutVal += AddVal;
				OutVal &= OutMask;
				*((UINT8*)OutPos) = (UINT8)OutVal;
				break;
			case 0x02:
#ifndef VGM_BIG_ENDIAN
				AddVal = Ent2B[InVal];
#else
				AddVal = ReadLE16((UINT8*)&Ent2B[InVal]);
#endif
				OutVal += AddVal;
				OutVal &= OutMask;
#ifndef VGM_BIG_ENDIAN
				*((UINT16*)OutPos) = (UINT16)OutVal;
#else
				OutPos[0x00] = (UINT8)((OutVal & 0x00FF) >> 0);
				OutPos[0x01] = (UINT8)((OutVal & 0xFF00) >> 8);
#endif
				break;
			}
		}
		break;
	default:
		printf("Error: Unknown data block compression!\n");
		return false;
	}
	
#if defined(_DEBUG) && defined(WIN32)
	Time = GetTickCount() - Time;
	printf("Decompression Time: %lu\n", Time);
#endif
	
	return true;
}

static UINT8 GetDACFromPCMBank(void)
{
	// for YM2612 DAC data only
	/*VGM_PCM_BANK* TempPCM;
	UINT32 CurBnk;*/
	UINT32 DataPos;
	
	/*TempPCM = &PCMBank[0x00];
	DataPos = TempPCM->DataPos;
	for (CurBnk = 0x00; CurBnk < TempPCM->BankCount; CurBnk ++)
	{
		if (DataPos < TempPCM->Bank[CurBnk].DataSize)
		{
			if (TempPCM->DataPos < TempPCM->DataSize)
				TempPCM->DataPos ++;
			return TempPCM->Bank[CurBnk].Data[DataPos];
		}
		DataPos -= TempPCM->Bank[CurBnk].DataSize;
	}
	return 0x80;*/
	
	DataPos = PCMBank[0x00].DataPos;
	if (DataPos >= PCMBank[0x00].DataSize)
		return 0x80;
	
	PCMBank[0x00].DataPos ++;
	return PCMBank[0x00].Data[DataPos];
}

static UINT8* GetPointerFromPCMBank(UINT8 Type, UINT32 DataPos)
{
	if (Type >= PCM_BANK_COUNT)
		return NULL;
	
	if (DataPos >= PCMBank[Type].DataSize)
		return NULL;
	
	return &PCMBank[Type].Data[DataPos];
}

static void ReadPCMTable(UINT32 DataSize, const UINT8* Data)
{
	UINT8 ValSize;
	UINT32 TblSize;
	
	PCMTbl.ComprType = Data[0x00];
	PCMTbl.CmpSubType = Data[0x01];
	PCMTbl.BitDec = Data[0x02];
	PCMTbl.BitCmp = Data[0x03];
	PCMTbl.EntryCount = ReadLE16(&Data[0x04]);
	
	ValSize = (PCMTbl.BitDec + 7) / 8;
	TblSize = PCMTbl.EntryCount * ValSize;
	
	PCMTbl.Entries = realloc(PCMTbl.Entries, TblSize);
	memcpy(PCMTbl.Entries, &Data[0x06], TblSize);
	
	if (DataSize < 0x06 + TblSize)
		printf("Warning! Bad PCM Table Length!\n");
	
	return;
}

#define CHIP_CHECK(name)	(ChipAudio[CurChip].name.ChipType != 0xFF)
static void InterpretVGM(UINT32 SampleCount)
{
	INT32 SmplPlayed;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	VGM_PCM_BANK* TempPCM;
	VGM_PCM_DATA* TempBnk;
	UINT32 ROMSize;
	UINT32 DataStart;
	UINT32 DataLen;
	const UINT8* ROMData;
	UINT8 CurChip;
	const UINT8* VGMPnt;
	
	if (VGMEnd)
		return;
	if (PausePlay && ! ForceVGMExec)
		return;
	
	SmplPlayed = SamplePbk2VGM_I(VGMSmplPlayed + SampleCount);
	while(VGMSmplPos <= SmplPlayed)
	{
		Command = VGMData[VGMPos + 0x00];
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				VGMSmplPos += (Command & 0x0F) + 0x01;
				break;
			case 0x80:
				TempByt = GetDACFromPCMBank();
				if (VGMHead.lngHzYM2612)
				{
					chip_reg_write(0x02, 0x00, 0x00, 0x2A, TempByt);
				}
				VGMSmplPos += (Command & 0x0F);
				break;
			}
			VGMPos += 0x01;
		}
		else
		{
			VGMPnt = &VGMData[VGMPos];
			
			// Cheat Mode (to use 2 instances of 1 chip)
			CurChip = 0x00;
			switch(Command)
			{
			case 0x30:
				if (VGMHead.lngHzPSG & 0x40000000)
				{
					Command += 0x20;
					CurChip = 0x01;
				}
				break;
			case 0x3F:
				if (VGMHead.lngHzPSG & 0x40000000)
				{
					Command += 0x10;
					CurChip = 0x01;
				}
				break;
			case 0xA1:
				if (VGMHead.lngHzYM2413 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA2:
			case 0xA3:
				if (VGMHead.lngHzYM2612 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA4:
				if (VGMHead.lngHzYM2151 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA5:
				if (VGMHead.lngHzYM2203 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA6:
			case 0xA7:
				if (VGMHead.lngHzYM2608 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA8:
			case 0xA9:
				if (VGMHead.lngHzYM2610 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAA:
				if (VGMHead.lngHzYM3812 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAB:
				if (VGMHead.lngHzYM3526 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAC:
				if (VGMHead.lngHzY8950 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAE:
			case 0xAF:
				if (VGMHead.lngHzYMF262 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAD:
				if (VGMHead.lngHzYMZ280B & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			}
			
			switch(Command)
			{
			case 0x66:	// End Of File
				if (VGMHead.lngLoopOffset)
				{
					VGMPos = VGMHead.lngLoopOffset;
					VGMSmplPos -= VGMHead.lngLoopSamples;
					VGMSmplPlayed -= SampleVGM2Pbk_I(VGMHead.lngLoopSamples);
					SmplPlayed = SamplePbk2VGM_I(VGMSmplPlayed + SampleCount);
					VGMCurLoop ++;
					
					if (VGMMaxLoopM && VGMCurLoop >= VGMMaxLoopM)
					{
#ifndef CONSOLE_MODE
						if (! FadePlay)
						{
							FadeStart = SampleVGM2Pbk_I(VGMHead.lngTotalSamples +
															(VGMCurLoop - 1) * VGMHead.lngLoopSamples);
						}
#endif
						FadePlay = true;
					}
					if (FadePlay && ! FadeTime)
						VGMEnd = true;
				}
				else
				{
					if (VGMHead.lngTotalSamples != (UINT32)VGMSmplPos)
					{
#ifdef CONSOLE_MODE
						printf("Warning! Header Samples: %u\t Counted Samples: %u\n",
								VGMHead.lngTotalSamples, VGMSmplPos);
						ErrorHappened = true;
#endif
						VGMHead.lngTotalSamples = VGMSmplPos;
					}
					VGMEnd = true;
					break;
				}
				break;
			case 0x62:	// 1/60s delay
				VGMSmplPos += 735;
				VGMPos += 0x01;
				break;
			case 0x63:	// 1/50s delay
				VGMSmplPos += 882;
				VGMPos += 0x01;
				break;
			case 0x61:	// xx Sample Delay
				TempSht = ReadLE16(&VGMPnt[0x01]);
				VGMSmplPos += TempSht;
				VGMPos += 0x03;
				break;
			case 0x50:	// SN76496 write
				if (CHIP_CHECK(SN76496))
				{
					chip_reg_write(0x00, CurChip, 0x00, 0x00, VGMPnt[0x01]);
				}
				VGMPos += 0x02;
				break;
			case 0x51:	// YM2413 write
				if (CHIP_CHECK(YM2413))
				{
					chip_reg_write(0x01, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				if (CHIP_CHECK(YM2612))
				{
					chip_reg_write(0x02, CurChip, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMPnt[0x02];
				TempLng = ReadLE32(&VGMPnt[0x03]);
				if (TempLng & 0x80000000)
				{
					TempLng &= 0x7FFFFFFF;
					CurChip = 0x01;
				}
				
				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					AddPCMData(TempByt, TempLng, &VGMPnt[0x07]);
					/*switch(TempByt)
					{
					case 0x00:	// YM2612 PCM Database
						break;
					case 0x01:	// RF5C68 PCM Database
						break;
					case 0x02:	// RF5C164 PCM Database
						break;
					}*/
					break;
				case 0x80:	// ROM/RAM Dump
					if (VGMCurLoop)
						break;
					
					ROMSize = ReadLE32(&VGMPnt[0x07]);
					DataStart = ReadLE32(&VGMPnt[0x0B]);
					DataLen = TempLng - 0x08;
					ROMData = &VGMPnt[0x0F];
					switch(TempByt)
					{
					case 0x80:	// SegaPCM ROM
						if (! CHIP_CHECK(SegaPCM))
							break;
						sega_pcm_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x81:	// YM2608 DELTA-T ROM Image
						if (! CHIP_CHECK(YM2608))
							break;
						ym2608_write_data_pcmrom(CurChip, 0x02, ROMSize, DataStart, DataLen,
												ROMData);
						break;
					case 0x82:	// YM2610 ADPCM ROM Image
					case 0x83:	// YM2610 DELTA-T ROM Image
						if (! CHIP_CHECK(YM2610))
							break;
						TempByt = 0x01 + (TempByt - 0x82);
						ym2610_write_data_pcmrom(CurChip, TempByt, ROMSize, DataStart, DataLen,
												ROMData);
						break;
					case 0x84:	// YMF278B ROM Image
						if (! CHIP_CHECK(YMF278B))
							break;
						ymf278b_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x85:	// YMF271 ROM Image
						if (! CHIP_CHECK(YMF271))
							break;
						ymf271_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x86:	// YMZ280B ROM Image
						if (! CHIP_CHECK(YMZ280B))
							break;
						ymz280b_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x87:	// YMF278B RAM Image
						if (! CHIP_CHECK(YMF278B))
							break;
						//ymf278b_write_ram(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x88:	// Y8950 DELTA-T ROM Image
						if (! CHIP_CHECK(Y8950) || PlayingMode == 0x01)
							break;
						y8950_write_data_pcmrom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x89:	// MultiPCM ROM Image
						if (! CHIP_CHECK(MultiPCM))
							break;
						multipcm_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x8A:	// UPD7759 ROM Image
						if (! CHIP_CHECK(UPD7759))
							break;
						upd7759_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x8B:	// OKIM6295 ROM Image
						if (! CHIP_CHECK(OKIM6295))
							break;
						okim6295_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x8C:	// K054539 ROM Image
						if (! CHIP_CHECK(K054539))
							break;
						k054539_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x8D:	// C140 ROM Image
						if (! CHIP_CHECK(C140))
							break;
						c140_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x8E:	// K053260 ROM Image
						if (! CHIP_CHECK(K053260))
							break;
						k053260_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x8F:	// QSound ROM Image
						if (! CHIP_CHECK(QSound))
							break;
						qsound_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x90:	// ES5506 ROM Image
						if (! CHIP_CHECK(ES5506))
							break;
						es5506_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x91:	// X1-010 ROM Image
						if (! CHIP_CHECK(X1_010))
							break;
						x1_010_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x92:	// C352 ROM Image
						if (! CHIP_CHECK(C352))
							break;
						c352_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
					case 0x93:	// GA20 ROM Image
						if (! CHIP_CHECK(GA20))
							break;
						iremga20_write_rom(CurChip, ROMSize, DataStart, DataLen, ROMData);
						break;
				//	case 0x8C:	// OKIM6376 ROM Image
				//		if (! CHIP_CHECK(OKIM6376))
				//			break;
				//		break;
					}
					break;
				case 0xC0:	// RAM Write
					if (! (TempByt & 0x20))
					{
						DataStart = ReadLE16(&VGMPnt[0x07]);
						DataLen = TempLng - 0x02;
						ROMData = &VGMPnt[0x09];
					}
					else
					{
						DataStart = ReadLE32(&VGMPnt[0x07]);
						DataLen = TempLng - 0x04;
						ROMData = &VGMPnt[0x0B];
					}
					switch(TempByt)
					{
					case 0xC0:	// RF5C68 RAM Database
						if (! CHIP_CHECK(RF5C68))
							break;
						rf5c68_write_ram(CurChip, DataStart, DataLen, ROMData);
						break;
					case 0xC1:	// RF5C164 RAM Database
						if (! CHIP_CHECK(RF5C164))
							break;
						rf5c164_write_ram(CurChip, DataStart, DataLen, ROMData);
						break;
					case 0xC2:	// NES APU RAM
						if (! CHIP_CHECK(NES))
							break;
						nes_write_ram(CurChip, DataStart, DataLen, ROMData);
						break;
					case 0xE0:	// SCSP RAM
						if (! CHIP_CHECK(SCSP))
							break;
						scsp_write_ram(CurChip, DataStart, DataLen, ROMData);
						break;
					case 0xE1:	// ES5503 RAM
						if (! CHIP_CHECK(ES5503))
							break;
						es5503_write_ram(CurChip, DataStart, DataLen, ROMData);
						break;
					}
					break;
				}
				VGMPos += 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				PCMBank[0x00].DataPos = ReadLE32(&VGMPnt[0x01]);
				VGMPos += 0x05;
				break;
			case 0x4F:	// GG Stereo
				if (CHIP_CHECK(SN76496))
				{
					chip_reg_write(0x00, CurChip, 0x01, 0x00, VGMPnt[0x01]);
				}
				VGMPos += 0x02;
				break;
			case 0x54:	// YM2151 write
				if (CHIP_CHECK(YM2151))
				{
					chip_reg_write(0x03, CurChip, 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				//VGMSmplPos += 80;
				VGMPos += 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				TempSht = ReadLE16(&VGMPnt[0x01]);
				CurChip = (TempSht & 0x8000) >> 15;
				if (CHIP_CHECK(SegaPCM))
				{
					sega_pcm_w(CurChip, TempSht & 0x7FFF, VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				if (CHIP_CHECK(RF5C68))
				{
					chip_reg_write(0x05, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xC1:	// RF5C164 memory write
				if (CHIP_CHECK(RF5C68))
				{
					TempSht = ReadLE16(&VGMPnt[0x01]);
					rf5c68_mem_w(CurChip, TempSht, VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0x55:	// YM2203
				if (CHIP_CHECK(YM2203))
				{
					chip_reg_write(0x06, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				if (CHIP_CHECK(YM2608))
				{
					chip_reg_write(0x07, CurChip, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				if (CHIP_CHECK(YM2610))
				{
					chip_reg_write(0x08, CurChip, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x5A:	// YM3812 write
				if (CHIP_CHECK(YM3812))
				{
					chip_reg_write(0x09, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x5B:	// YM3526 write
				if (CHIP_CHECK(YM3526))
				{
					chip_reg_write(0x0A, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x5C:	// Y8950 write
				if (CHIP_CHECK(Y8950))
				{
					chip_reg_write(0x0B, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				if (CHIP_CHECK(YMF262))
				{
					chip_reg_write(0x0C, CurChip, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x5D:	// YMZ280B write
				if (CHIP_CHECK(YMZ280B))
				{
					chip_reg_write(0x0F, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xD0:	// YMF278B write
				if (CHIP_CHECK(YMF278B))
				{
					CurChip = (VGMPnt[0x01] & 0x80) >> 7;
					chip_reg_write(0x0D, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xD1:	// YMF271 write
				if (CHIP_CHECK(YMF271))
				{
					CurChip = (VGMPnt[0x01] & 0x80) >> 7;
					chip_reg_write(0x0E, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xB1:	// RF5C164 register write
				if (CHIP_CHECK(RF5C164))
				{
					chip_reg_write(0x10, CurChip, 0x00, VGMPnt[0x01], VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xC2:	// RF5C164 memory write
				if (CHIP_CHECK(RF5C164))
				{
					TempSht = ReadLE16(&VGMPnt[0x01]);
					rf5c164_mem_w(CurChip, TempSht, VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xB2:	// PWM channel write
				if (CHIP_CHECK(PWM))
				{
					chip_reg_write(0x11, CurChip, (VGMPnt[0x01] & 0xF0) >> 4,
									VGMPnt[0x01] & 0x0F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x68:	// PCM RAM write
				CurChip = (VGMPnt[0x02] & 0x80) >> 7;
				TempByt =  VGMPnt[0x02] & 0x7F;
				
				DataStart = ReadLE24(&VGMPnt[0x03]);
				TempLng = ReadLE24(&VGMPnt[0x06]);
				DataLen = ReadLE24(&VGMPnt[0x09]);
				if (! DataLen)
					DataLen += 0x01000000;
				ROMData = GetPointerFromPCMBank(TempByt, DataStart);
				if (ROMData == NULL)
				{
					VGMPos += 0x0C;
					break;
				}
				
				switch(TempByt)
				{
				case 0x01:
					if (! CHIP_CHECK(RF5C68))
						break;
					rf5c68_write_ram(CurChip, TempLng, DataLen, ROMData);
					break;
				case 0x02:
					if (! CHIP_CHECK(RF5C164))
						break;
					rf5c164_write_ram(CurChip, TempLng, DataLen, ROMData);
					break;
				case 0x06:
					if (! CHIP_CHECK(SCSP))
						break;
					scsp_write_ram(CurChip, TempLng, DataLen, ROMData);
					break;
				case 0x07:
					if (! CHIP_CHECK(NES))
						break;
					Last95Drum = DataStart / DataLen - 1;
					Last95Max = PCMBank[TempByt].DataSize / DataLen;
					nes_write_ram(CurChip, TempLng, DataLen, ROMData);
					break;
				}
				VGMPos += 0x0C;
				break;
			case 0xA0:	// AY8910 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(AY8910))
				{
					chip_reg_write(0x12, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xB3:	// GameBoy DMG write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(GameBoy))
				{
					chip_reg_write(0x13, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xB4:	// NES APU write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(NES))
				{
					chip_reg_write(0x14, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xB5:	// MultiPCM write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(MultiPCM))
				{
					chip_reg_write(0x15, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xC3:	// MultiPCM memory write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(MultiPCM))
				{
					TempSht = ReadLE16(&VGMPnt[0x02]);
					multipcm_bank_write(CurChip, VGMPnt[0x01] & 0x7F, TempSht);
				}
				VGMPos += 0x04;
				break;
			case 0xB6:	// UPD7759 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(UPD7759))
				{
					chip_reg_write(0x16, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xB7:	// OKIM6258 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(OKIM6258))
				{
					chip_reg_write(0x17, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xB8:	// OKIM6295 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(OKIM6295))
				{
					chip_reg_write(0x18, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xD2:	// SCC1 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(K051649))
				{
					chip_reg_write(0x19, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xD3:	// K054539 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(K054539))
				{
					chip_reg_write(0x1A, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xB9:	// HuC6280 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(HuC6280))
				{
					chip_reg_write(0x1B, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xD4:	// C140 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(C140))
				{
					chip_reg_write(0x1C, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xBA:	// K053260 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(K053260))
				{
					chip_reg_write(0x1D, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xBB:	// Pokey write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(Pokey))
				{
					chip_reg_write(0x1E, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xC4:	// QSound write
				if (CHIP_CHECK(QSound))
				{
					chip_reg_write(0x1F, CurChip, VGMPnt[0x01], VGMPnt[0x02], VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xC5:	// YMF292/SCSP write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(SCSP))
				{
					chip_reg_write(0x20, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xBC:	// WonderSwan write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(WSwan))
				{
					chip_reg_write(0x21, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xC6:	// WonderSwan memory write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(WSwan))
				{
					TempSht = ReadBE16(&VGMPnt[0x01]) & 0x7FFF;
					ws_write_ram(CurChip, TempSht, VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xC7:	// VSU write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(VSU))
				{
					chip_reg_write(0x22, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xBD:	// SAA1099 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(SAA1099))
				{
					chip_reg_write(0x23, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xD5:	// ES5503 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(ES5503))
				{
					chip_reg_write(0x24, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xBE:	// ES5506 write (8-bit data)
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(ES5506))
				{
					chip_reg_write(0x25, CurChip, VGMPnt[0x01] & 0x7F, 0x00, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0xD6:	// ES5506 write (16-bit data)
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(ES5506))
				{
					chip_reg_write(0x25, CurChip, 0x80 | (VGMPnt[0x01] & 0x7F),
									VGMPnt[0x02], VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
			case 0xC8:	// X1-010 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(X1_010))
				{
					chip_reg_write(0x26, CurChip, VGMPnt[0x01] & 0x7F, VGMPnt[0x02],
									VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
#if 0	// for ctr's WIP rips
			case 0xC9:	// C352 write
				CurChip = 0x00;
				if (CHIP_CHECK(C352))
				{
					if (VGMPnt[0x01] == 0x03 && VGMPnt[0x02] == 0xFF && VGMPnt[0x03] == 0xFF)
						c352_w(CurChip, 0x202, 0x0020);
					else
						chip_reg_write(0x27, CurChip, VGMPnt[0x01], VGMPnt[0x02],
										VGMPnt[0x03]);
				}
				VGMPos += 0x04;
				break;
#endif
			case 0xE1:	// C352 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(C352))
				{
					TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
					c352_w(CurChip, TempSht, (VGMPnt[0x03] << 8) | VGMPnt[0x04]);
				}
				VGMPos += 0x05;
				break;
			case 0xBF:	// GA20 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				if (CHIP_CHECK(GA20))
				{
					chip_reg_write(0x28, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				VGMPos += 0x03;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				CurChip = VGMPnt[0x01];
				if (CurChip == 0xFF)
				{
					VGMPos += 0x05;
					break;
				}
				if (! DacCtrl[CurChip].Enable)
				{
					device_start_daccontrol(CurChip);
					device_reset_daccontrol(CurChip);
					DacCtrl[CurChip].Enable = true;
					DacCtrlUsg[DacCtrlUsed] = CurChip;
					DacCtrlUsed ++;
				}
				TempByt = VGMPnt[0x02];	// Chip Type
				TempSht = ReadBE16(&VGMPnt[0x03]);
				daccontrol_setup_chip(CurChip, TempByt & 0x7F, (TempByt & 0x80) >> 7, TempSht);
				VGMPos += 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				CurChip = VGMPnt[0x01];
				if (CurChip == 0xFF || ! DacCtrl[CurChip].Enable)
				{
					VGMPos += 0x05;
					break;
				}
				DacCtrl[CurChip].Bank = VGMPnt[0x02];
				if (DacCtrl[CurChip].Bank >= PCM_BANK_COUNT)
					DacCtrl[CurChip].Bank = 0x00;
				
				TempPCM = &PCMBank[DacCtrl[CurChip].Bank];
				Last95Max = TempPCM->BankCount;
				daccontrol_set_data(CurChip, TempPCM->Data, TempPCM->DataSize,
									VGMPnt[0x03], VGMPnt[0x04]);
				VGMPos += 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				CurChip = VGMPnt[0x01];
				if (CurChip == 0xFF || ! DacCtrl[CurChip].Enable)
				{
					VGMPos += 0x06;
					break;
				}
				TempLng = ReadLE32(&VGMPnt[0x02]);
				Last95Freq = TempLng;
				daccontrol_set_frequency(CurChip, TempLng);
				VGMPos += 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				CurChip = VGMPnt[0x01];
				if (CurChip == 0xFF || ! DacCtrl[CurChip].Enable ||
					! PCMBank[DacCtrl[CurChip].Bank].BankCount)
				{
					VGMPos += 0x0B;
					break;
				}
				DataStart = ReadLE32(&VGMPnt[0x02]);
				Last95Drum = 0xFFFF;
				TempByt = VGMPnt[0x06];
				DataLen = ReadLE32(&VGMPnt[0x07]);
				daccontrol_start(CurChip, DataStart, TempByt, DataLen);
				VGMPos += 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				CurChip = VGMPnt[0x01];
				if (! DacCtrl[CurChip].Enable)
				{
					VGMPos += 0x02;
					break;
				}
				Last95Drum = 0xFFFF;
				if (CurChip < 0xFF)
				{
					daccontrol_stop(CurChip);
				}
				else
				{
					for (CurChip = 0x00; CurChip < 0xFF; CurChip ++)
						daccontrol_stop(CurChip);
				}
				VGMPos += 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				CurChip = VGMPnt[0x01];
				if (CurChip == 0xFF || ! DacCtrl[CurChip].Enable ||
					! PCMBank[DacCtrl[CurChip].Bank].BankCount)
				{
					VGMPos += 0x05;
					break;
				}
				TempPCM = &PCMBank[DacCtrl[CurChip].Bank];
				TempSht = ReadLE16(&VGMPnt[0x02]);
				Last95Drum = TempSht;
				Last95Max = TempPCM->BankCount;
				if (TempSht >= TempPCM->BankCount)
					TempSht = 0x00;
				TempBnk = &TempPCM->Bank[TempSht];
				
				TempByt = DCTRL_LMODE_BYTES |
							(VGMPnt[0x04] & 0x10) |			// Reverse Mode
							((VGMPnt[0x04] & 0x01) << 7);	// Looping
				daccontrol_start(CurChip, TempBnk->DataStart, TempByt, TempBnk->DataSize);
				VGMPos += 0x05;
				break;
			default:
#ifdef CONSOLE_MODE
				if (! CmdList[Command])
				{
					printf("Unknown command: %02hX\n", Command);
					CmdList[Command] = true;
				}
#endif
				
				switch(Command & 0xF0)
				{
				case 0x00:
				case 0x10:
				case 0x20:
					VGMPos += 0x01;
					break;
				case 0x30:
					VGMPos += 0x02;
					break;
				case 0x40:
				case 0x50:
				case 0xA0:
				case 0xB0:
					VGMPos += 0x03;
					break;
				case 0xC0:
				case 0xD0:
					VGMPos += 0x04;
					break;
				case 0xE0:
				case 0xF0:
					VGMPos += 0x05;
					break;
				default:
					VGMEnd = true;
					EndPlay = true;
					break;
				}
				break;
			}
		}
		
		if (VGMPos >= VGMHead.lngEOFOffset)
			VGMEnd = true;
		
		if (VGMEnd)
			break;
	}
	
	return;
}


static void GeneralChipLists(void)
{
	// Generate Chip List for playback loop
	UINT16 CurBufIdx;
	CA_LIST* CLstOld;
	CA_LIST* CLst;
	CA_LIST* CurLst;
	UINT8 CurChip;
	UINT8 CurCSet;
	CAUD_ATTR* CAA;
	
	ChipListAll = NULL;
	ChipListPause = NULL;
	//ChipListOpt = NULL;
	
	// generate list of all chips that are used in the current VGM
	CurBufIdx = 0x00;
	CLstOld = NULL;
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
	{
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
			CAA = (CAUD_ATTR*)&ChipAudio[CurCSet] + CurChip;
			if (CAA->ChipType != 0xFF)
			{
				CLst = &ChipListBuffer[CurBufIdx];
				CurBufIdx ++;
				if (CLstOld == NULL)
					ChipListAll = CLst;
				else
					CLstOld->next = CLst;
				
				CLst->CAud = CAA;
				CLst->COpts = (CHIP_OPTS*)&ChipOpts[CurCSet] + CurChip;
				CLstOld = CLst;
			}
		}
	}
	if (CLstOld != NULL)
		CLstOld->next = NULL;
	
	// Go through the chip list and copy all chips to a new list, except for a few
	// selected ones.
	CLstOld = NULL;
	CurLst = ChipListAll;
	while(CurLst != NULL)
	{
		// don't emulate the RF5Cxx chips when paused+emulated
		if (CurLst->CAud->ChipType != 0x05 && CurLst->CAud->ChipType != 0x10)
		{
			CLst = &ChipListBuffer[CurBufIdx];
			CurBufIdx ++;
			if (CLstOld == NULL)
				ChipListPause = CLst;
			else
				CLstOld->next = CLst;
			
			*CLst = *CurLst;
			CLstOld = CLst;
		}
		CurLst = CurLst->next;
	}
	if (CLstOld != NULL)
		CLstOld->next = NULL;
	
	return;
}

static void SetupResampler(CAUD_ATTR* CAA)
{
	if (! CAA->SmpRate)
	{
		CAA->Resampler = 0xFF;
		return;
	}
	
	if (CAA->SmpRate < SampleRate)
		CAA->Resampler = 0x01;
	else if (CAA->SmpRate == SampleRate)
		CAA->Resampler = 0x02;
	else if (CAA->SmpRate > SampleRate)
		CAA->Resampler = 0x03;
	if (CAA->Resampler == 0x01 || CAA->Resampler == 0x03)
	{
		if (ResampleMode == 0x02 || (ResampleMode == 0x01 && CAA->Resampler == 0x03))
			CAA->Resampler = 0x00;
	}
	
	CAA->SmpP = 0x00;
	CAA->SmpLast = 0x00;
	CAA->SmpNext = 0x00;
	CAA->LSmpl.Left = 0x00;
	CAA->LSmpl.Right = 0x00;
	if (CAA->Resampler == 0x01)
	{
		// Pregenerate first Sample (the upsampler is always one too late)
		CAA->StreamUpdate(CAA->ChipID, StreamBufs, 1);
		CAA->NSmpl.Left = StreamBufs[0x00][0x00];
		CAA->NSmpl.Right = StreamBufs[0x01][0x00];
	}
	else
	{
		CAA->NSmpl.Left = 0x00;
		CAA->NSmpl.Right = 0x00;
	}
	
	return;
}

static void ChangeChipSampleRate(void* DataPtr, UINT32 NewSmplRate)
{
	CAUD_ATTR* CAA = (CAUD_ATTR*)DataPtr;
	
	if (CAA->SmpRate == NewSmplRate)
		return;
	
	// quick and dirty hack to make sample rate changes work
	CAA->SmpRate = NewSmplRate;
	if (CAA->SmpRate < SampleRate)
		CAA->Resampler = 0x01;
	else if (CAA->SmpRate == SampleRate)
		CAA->Resampler = 0x02;
	else if (CAA->SmpRate > SampleRate)
		CAA->Resampler = 0x03;
	CAA->SmpP = 1;
	CAA->SmpNext -= CAA->SmpLast;
	CAA->SmpLast = 0x00;
	
	return;
}


INLINE INT16 Limit2Short(INT32 Value)
{
	INT32 NewValue;
	
	NewValue = Value;
	if (NewValue < -0x8000)
		NewValue = -0x8000;
	if (NewValue > 0x7FFF)
		NewValue = 0x7FFF;
	
	return (INT16)NewValue;
}

static void null_update(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	memset(outputs[0x00], 0x00, sizeof(stream_sample_t) * samples);
	memset(outputs[0x01], 0x00, sizeof(stream_sample_t) * samples);
	
	return;
}

static void dual_opl2_stereo(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	ym3812_stream_update(ChipID, outputs, samples);
	
	// Dual-OPL with Stereo
	if (ChipID & 0x01)
		memset(outputs[0x00], 0x00, sizeof(stream_sample_t) * samples);	// Mute Left Chanel
	else
		memset(outputs[0x01], 0x00, sizeof(stream_sample_t) * samples);	// Mute Right Chanel
	
	return;
}

// I recommend 11 bits as it's fast and accurate
#define FIXPNT_BITS		11
#define FIXPNT_FACT		(1 << FIXPNT_BITS)
#if (FIXPNT_BITS <= 11)
	typedef UINT32	SLINT;	// 32-bit is a lot faster
#else
	typedef UINT64	SLINT;
#endif
#define FIXPNT_MASK		(FIXPNT_FACT - 1)

#define getfriction(x)	((x) & FIXPNT_MASK)
#define getnfriction(x)	((FIXPNT_FACT - (x)) & FIXPNT_MASK)
#define fpi_floor(x)	((x) & ~FIXPNT_MASK)
#define fpi_ceil(x)		((x + FIXPNT_MASK) & ~FIXPNT_MASK)
#define fp2i_floor(x)	((x) / FIXPNT_FACT)
#define fp2i_ceil(x)	((x + FIXPNT_MASK) / FIXPNT_FACT)

static void ResampleChipStream(CA_LIST* CLst, WAVE_32BS* RetSample, UINT32 Length)
{
	CAUD_ATTR* CAA;
	INT32* CurBufL;
	INT32* CurBufR;
	INT32* StreamPnt[0x02];
	UINT32 InBase;
	UINT32 InPos;
	UINT32 InPosNext;
	UINT32 OutPos;
	UINT32 SmpFrc;	// Sample Friction
	UINT32 InPre;
	UINT32 InNow;
	SLINT InPosL;
	INT64 TempSmpL;
	INT64 TempSmpR;
	INT32 TempS32L;
	INT32 TempS32R;
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	INT32 CurSmpl;
	UINT64 ChipSmpRate;
	
	CAA = CLst->CAud;
	CurBufL = StreamBufs[0x00];
	CurBufR = StreamBufs[0x01];
	
	// This Do-While-Loop gets and resamples the chip output of one or more chips.
	// It's a loop to support the AY8910 paired with the YM2203/YM2608/YM2610.
	do
	{
		switch(CAA->Resampler)
		{
		case 0x00:	// old, but very fast resampler
			CAA->SmpLast = CAA->SmpNext;
			CAA->SmpP += Length;
			CAA->SmpNext = (UINT32)((UINT64)CAA->SmpP * CAA->SmpRate / SampleRate);
			if (CAA->SmpLast >= CAA->SmpNext)
			{
				RetSample->Left += CAA->LSmpl.Left * CAA->Volume;
				RetSample->Right += CAA->LSmpl.Right * CAA->Volume;
			}
			else
			{
				SmpCnt = CAA->SmpNext - CAA->SmpLast;
				
				CAA->StreamUpdate(CAA->ChipID, StreamBufs, SmpCnt);
				
				if (SmpCnt == 1)
				{
					RetSample->Left += CurBufL[0x00] * CAA->Volume;
					RetSample->Right += CurBufR[0x00] * CAA->Volume;
					CAA->LSmpl.Left = CurBufL[0x00];
					CAA->LSmpl.Right = CurBufR[0x00];
				}
				else if (SmpCnt == 2)
				{
					RetSample->Left += (CurBufL[0x00] + CurBufL[0x01]) * CAA->Volume >> 1;
					RetSample->Right += (CurBufR[0x00] + CurBufR[0x01]) * CAA->Volume >> 1;
					CAA->LSmpl.Left = CurBufL[0x01];
					CAA->LSmpl.Right = CurBufR[0x01];
				}
				else
				{
					TempS32L = CurBufL[0x00];
					TempS32R = CurBufR[0x00];
					for (CurSmpl = 0x01; CurSmpl < SmpCnt; CurSmpl ++)
					{
						TempS32L += CurBufL[CurSmpl];
						TempS32R += CurBufR[CurSmpl];
					}
					RetSample->Left += TempS32L * CAA->Volume / SmpCnt;
					RetSample->Right += TempS32R * CAA->Volume / SmpCnt;
					CAA->LSmpl.Left = CurBufL[SmpCnt - 1];
					CAA->LSmpl.Right = CurBufR[SmpCnt - 1];
				}
			}
			break;
		case 0x01:	// Upsampling
			ChipSmpRate = CAA->SmpRate;
			InPosL = (SLINT)(FIXPNT_FACT * CAA->SmpP * ChipSmpRate / SampleRate);
			InPre = (UINT32)fp2i_floor(InPosL);
			InNow = (UINT32)fp2i_ceil(InPosL);
			/*if (InNow - CAA->SmpNext >= SMPL_BUFSIZE)
			{
				printf("Sample Buffer Overflow!\n");
#ifdef _DEBUG
				*(char*)NULL = 0;
#endif
				CAA->SmpLast = 0;
				CAA->SmpNext = 0;
				CAA->SmpP = 0;
				break;
			}*/
			
			CurBufL[0x00] = CAA->LSmpl.Left;
			CurBufR[0x00] = CAA->LSmpl.Right;
			CurBufL[0x01] = CAA->NSmpl.Left;
			CurBufR[0x01] = CAA->NSmpl.Right;
			StreamPnt[0x00] = &CurBufL[0x02];
			StreamPnt[0x01] = &CurBufR[0x02];
			CAA->StreamUpdate(CAA->ChipID, StreamPnt, InNow - CAA->SmpNext);
			
			InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->SmpNext * FIXPNT_FACT);
			/*if (fp2i_floor(InBase) >= SMPL_BUFSIZE)
			{
				printf("Sample Buffer Overflow!\n");
#ifdef _DEBUG
				*(char*)NULL = 0;
#endif
				CAA->SmpLast = 0;
				CAA->SmpP = 0;
				break;
			}*/
			SmpCnt = FIXPNT_FACT;
			CAA->SmpLast = InPre;
			CAA->SmpNext = InNow;
			for (OutPos = 0x00; OutPos < Length; OutPos ++)
			{
				InPos = InBase + (UINT32)(FIXPNT_FACT * OutPos * ChipSmpRate / SampleRate);
				
				InPre = fp2i_floor(InPos);
				InNow = fp2i_ceil(InPos);
				SmpFrc = getfriction(InPos);
				
				// Linear interpolation
				TempSmpL = ((INT64)CurBufL[InPre] * (FIXPNT_FACT - SmpFrc)) +
							((INT64)CurBufL[InNow] * SmpFrc);
				TempSmpR = ((INT64)CurBufR[InPre] * (FIXPNT_FACT - SmpFrc)) +
							((INT64)CurBufR[InNow] * SmpFrc);
				RetSample[OutPos].Left += (INT32)(TempSmpL * CAA->Volume / SmpCnt);
				RetSample[OutPos].Right += (INT32)(TempSmpR * CAA->Volume / SmpCnt);
			}
			CAA->LSmpl.Left = CurBufL[InPre];
			CAA->LSmpl.Right = CurBufR[InPre];
			CAA->NSmpl.Left = CurBufL[InNow];
			CAA->NSmpl.Right = CurBufR[InNow];
			CAA->SmpP += Length;
			break;
		case 0x02:	// Copying
			CAA->SmpNext = CAA->SmpP * CAA->SmpRate / SampleRate;
			CAA->StreamUpdate(CAA->ChipID, StreamBufs, Length);
			
			for (OutPos = 0x00; OutPos < Length; OutPos ++)
			{
				RetSample[OutPos].Left += CurBufL[OutPos] * CAA->Volume;
				RetSample[OutPos].Right += CurBufR[OutPos] * CAA->Volume;
			}
			CAA->SmpP += Length;
			CAA->SmpLast = CAA->SmpNext;
			break;
		case 0x03:	// Downsampling
			ChipSmpRate = CAA->SmpRate;
			InPosL = (SLINT)(FIXPNT_FACT * (CAA->SmpP + Length) * ChipSmpRate / SampleRate);
			CAA->SmpNext = (UINT32)fp2i_ceil(InPosL);
			
			CurBufL[0x00] = CAA->LSmpl.Left;
			CurBufR[0x00] = CAA->LSmpl.Right;
			StreamPnt[0x00] = &CurBufL[0x01];
			StreamPnt[0x01] = &CurBufR[0x01];
			CAA->StreamUpdate(CAA->ChipID, StreamPnt, CAA->SmpNext - CAA->SmpLast);
			
			InPosL = (SLINT)(FIXPNT_FACT * CAA->SmpP * ChipSmpRate / SampleRate);
			// I'm adding 1.0 to avoid negative indexes
			InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->SmpLast * FIXPNT_FACT);
			InPosNext = InBase;
			for (OutPos = 0x00; OutPos < Length; OutPos ++)
			{
				//InPos = InBase + (UINT32)(FIXPNT_FACT * OutPos * ChipSmpRate / SampleRate);
				InPos = InPosNext;
				InPosNext = InBase + (UINT32)(FIXPNT_FACT * (OutPos+1) * ChipSmpRate / SampleRate);
				
				// first frictional Sample
				SmpFrc = getnfriction(InPos);
				if (SmpFrc)
				{
					InPre = fp2i_floor(InPos);
					TempSmpL = (INT64)CurBufL[InPre] * SmpFrc;
					TempSmpR = (INT64)CurBufR[InPre] * SmpFrc;
				}
				else
				{
					TempSmpL = TempSmpR = 0x00;
				}
				SmpCnt = SmpFrc;
				
				// last frictional Sample
				SmpFrc = getfriction(InPosNext);
				InPre = fp2i_floor(InPosNext);
				if (SmpFrc)
				{
					TempSmpL += (INT64)CurBufL[InPre] * SmpFrc;
					TempSmpR += (INT64)CurBufR[InPre] * SmpFrc;
					SmpCnt += SmpFrc;
				}
				
				// whole Samples in between
				//InPre = fp2i_floor(InPosNext);
				InNow = fp2i_ceil(InPos);
				SmpCnt += (InPre - InNow) * FIXPNT_FACT;	// this is faster
				while(InNow < InPre)
				{
					TempSmpL += (INT64)CurBufL[InNow] * FIXPNT_FACT;
					TempSmpR += (INT64)CurBufR[InNow] * FIXPNT_FACT;
					//SmpCnt ++;
					InNow ++;
				}
				
				RetSample[OutPos].Left += (INT32)(TempSmpL * CAA->Volume / SmpCnt);
				RetSample[OutPos].Right += (INT32)(TempSmpR * CAA->Volume / SmpCnt);
			}
			
			CAA->LSmpl.Left = CurBufL[InPre];
			CAA->LSmpl.Right = CurBufR[InPre];
			CAA->SmpP += Length;
			CAA->SmpLast = CAA->SmpNext;
			break;
		default:
			CAA->SmpP += SampleRate;
			break;	// do absolutely nothing
		}
		
		if (CAA->SmpLast >= CAA->SmpRate)
		{
			CAA->SmpLast -= CAA->SmpRate;
			CAA->SmpNext -= CAA->SmpRate;
			CAA->SmpP -= SampleRate;
		}
		
		CAA = CAA->Paired;
	} while(CAA != NULL);
	
	return;
}

static INT32 RecalcFadeVolume(void)
{
	float TempSng;
	
	if (FadePlay)
	{
		if (! FadeStart)
			FadeStart = PlayingTime;
		
		TempSng = (PlayingTime - FadeStart) / (float)SampleRate;
		MasterVol = 1.0f - TempSng / (FadeTime * 0.001f);
		if (MasterVol < 0.0f)
		{
			MasterVol = 0.0f;
			//EndPlay = true;
			VGMEnd = true;
		}
		FinalVol = VolumeLevelM * MasterVol * MasterVol;
	}
	
	return (INT32)(0x100 * FinalVol + 0.5f);
}

UINT32 FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize)
{
	UINT32 CurSmpl;
	WAVE_32BS TempBuf;
	INT32 CurMstVol;
	UINT32 RecalcStep;
	CA_LIST* CurCLst;
	
	//memset(Buffer, 0x00, sizeof(WAVE_16BS) * BufferSize);
	
	RecalcStep = FadePlay ? SampleRate / 44100 : 0;
	CurMstVol = RecalcFadeVolume();
	
	if (Buffer == NULL)
	{
		//for (CurSmpl = 0x00; CurSmpl < BufferSize; CurSmpl ++)
		//	InterpretFile(1);
		InterpretFile(BufferSize);
		
		if (FadePlay && ! FadeStart)
		{
			FadeStart = PlayingTime;
			RecalcStep = FadePlay ? SampleRate / 100 : 0;
		}
		//if (RecalcStep && ! (CurSmpl % RecalcStep))
		if (RecalcStep)
			CurMstVol = RecalcFadeVolume();
		
		if (VGMEnd)
		{
			if (PauseSmpls <= BufferSize)
			{
				PauseSmpls = 0;
				EndPlay = true;
			}
			else
			{
				PauseSmpls -= BufferSize;
			}
		}
		
		return BufferSize;
	}
	
	CurChipList = (VGMEnd || PausePlay) ? ChipListPause : ChipListAll;
	
	for (CurSmpl = 0x00; CurSmpl < BufferSize; CurSmpl ++)
	{
		InterpretFile(1);
		
		// Sample Structures
		//	00 - SN76496
		//	01 - YM2413
		//	02 - YM2612
		//	03 - YM2151
		//	04 - SegaPCM
		//	05 - RF5C68
		//	06 - YM2203
		//	07 - YM2608
		//	08 - YM2610/YM2610B
		//	09 - YM3812
		//	0A - YM3526
		//	0B - Y8950
		//	0C - YMF262
		//	0D - YMF278B
		//	0E - YMF271
		//	0F - YMZ280B
		//	10 - RF5C164
		//	11 - PWM
		//	12 - AY8910
		//	13 - GameBoy
		//	14 - NES APU
		//	15 - MultiPCM
		//	16 - UPD7759
		//	17 - OKIM6258
		//	18 - OKIM6295
		//	19 - K051649
		//	1A - K054539
		//	1B - HuC6280
		//	1C - C140
		//	1D - K053260
		//	1E - Pokey
		//	1F - QSound
		//	20 - YMF292/SCSP
		//	21 - WonderSwan
		//	22 - VSU
		//	23 - SAA1099
		//	24 - ES5503
		//	25 - ES5506
		//	26 - X1-010
		//	27 - C352
		//	28 - GA20
		TempBuf.Left = 0x00;
		TempBuf.Right = 0x00;
		CurCLst = CurChipList;
		while(CurCLst != NULL)
		{
			if (! CurCLst->COpts->Disabled)
			{
				ResampleChipStream(CurCLst, &TempBuf, 1);
			}
			CurCLst = CurCLst->next;
		}
		
		// ChipData << 9 [ChipVol] >> 5 << 8 [MstVol] >> 11  ->  9-5+8-11 = <<1
		TempBuf.Left = ((TempBuf.Left >> 5) * CurMstVol) >> 11;
		TempBuf.Right = ((TempBuf.Right >> 5) * CurMstVol) >> 11;
		if (SurroundSound)
			TempBuf.Right *= -1;
		Buffer[CurSmpl].Left = Limit2Short(TempBuf.Left);
		Buffer[CurSmpl].Right = Limit2Short(TempBuf.Right);
		
		if (FadePlay && ! FadeStart)
		{
			FadeStart = PlayingTime;
			RecalcStep = FadePlay ? SampleRate / 100 : 0;
		}
		if (RecalcStep && ! (CurSmpl % RecalcStep))
			CurMstVol = RecalcFadeVolume();
		
		if (VGMEnd)
		{
			if (! PauseSmpls)
			{
				//if (! FullBufFill)
				if (! EndPlay)
				{
					EndPlay = true;
					break;
				}
			}
			else //if (PauseSmpls)
			{
				PauseSmpls --;
			}
		}
	}
	
	return CurSmpl;
}

#ifdef WIN32
DWORD WINAPI PlayingThread(void* Arg)
{
	LARGE_INTEGER CPUFreq;
	LARGE_INTEGER TimeNow;
	LARGE_INTEGER TimeLast;
	UINT64 TimeDiff;
	UINT64 SampleTick;
	UINT64 Ticks;
	BOOL RetVal;
	
	hPlayThread = GetCurrentThread();
#ifndef _DEBUG
	RetVal = SetThreadPriority(hPlayThread, THREAD_PRIORITY_TIME_CRITICAL);
#else
	RetVal = SetThreadPriority(hPlayThread, THREAD_PRIORITY_ABOVE_NORMAL);
#endif
	if (! RetVal)
	{
		// Error by setting priority
	}
	
	QueryPerformanceFrequency(&CPUFreq);
	QueryPerformanceCounter(&TimeNow);
	TimeLast = TimeNow;
	SampleTick = CPUFreq.QuadPart / SampleRate;
	
	while (! CloseThread)
	{
		while(PlayingMode != 0x01 && ! CloseThread)
			Sleep(1);
		
		if (! PauseThread)
		{
			TimeDiff = TimeNow.QuadPart - TimeLast.QuadPart;
			if (TimeDiff >= SampleTick)
			{
				Ticks = TimeDiff * SampleRate / CPUFreq.QuadPart;
				if (Ticks > SampleRate / 2)
					Ticks = SampleRate / 50;
				FillBuffer(NULL, (UINT32)Ticks);
				if (! ResetPBTimer)
				{
					TimeLast = TimeNow;
				}
				else
				{
					QueryPerformanceCounter(&TimeLast);
					TimeLast.QuadPart -= TimeDiff;
					ResetPBTimer = false;
				}
			}
			
			// I tried to make sample-accurate replaying through Hardware FM
			// to make PSG-PCM clear, but it didn't work
			//if (! FMAccurate)
			Sleep(1);
			//else
			//	Sleep(0);
		}
		else
		{
			ThreadPauseConfrm = true;
			if (! ThreadNoWait)
				TimeLast = TimeNow;
			Sleep(1);
		}
		QueryPerformanceCounter(&TimeNow);
	}
	
	hPlayThread = NULL;
	return 0x00000000;
}
#else
UINT64 TimeSpec2Int64(const struct timespec* ts)
{
	return (UINT64)ts->tv_sec * 1000000000 + ts->tv_nsec;
}

void* PlayingThread(void* Arg)
{
	UINT64 CPUFreq;
	UINT64 TimeNow;
	UINT64 TimeLast;
	UINT64 TimeDiff;
	UINT64 SampleTick;
	UINT64 Ticks;
	struct timespec TempTS;
	int RetVal;
	
	//RetVal = clock_getres(CLOCK_MONOTONIC, &TempTS);
	//CPUFreq = TimeSpec2Int64(&TempTS);
	CPUFreq = 1000000000;
	RetVal = clock_gettime(CLOCK_MONOTONIC, &TempTS);
	TimeNow = TimeSpec2Int64(&TempTS);
	TimeLast = TimeNow;
	SampleTick = CPUFreq / SampleRate;
	
	while (! CloseThread)
	{
		while(PlayingMode != 0x01 && ! CloseThread)
			Sleep(1);
		
		if (! PauseThread)
		{
			TimeDiff = TimeNow - TimeLast;
			if (TimeDiff >= SampleTick)
			{
				Ticks = TimeDiff * SampleRate / CPUFreq;
				if (Ticks > SampleRate / 2)
					Ticks = SampleRate / 50;
				FillBuffer(NULL, (UINT32)Ticks);
				if (! ResetPBTimer)
				{
					TimeLast = TimeNow;
				}
				else
				{
					RetVal = clock_gettime(CLOCK_MONOTONIC, &TempTS);
					TimeLast = TimeSpec2Int64(&TempTS) - TimeDiff;
					ResetPBTimer = false;
				}
			}
			//if (! FMAccurate)
			Sleep(1);
			//else
			//	Sleep(0);
		}
		else
		{
			ThreadPauseConfrm = true;
			if (ThreadNoWait)
				TimeLast = TimeNow;
			Sleep(1);
		}
		RetVal = clock_gettime(CLOCK_MONOTONIC, &TempTS);
		TimeNow = TimeSpec2Int64(&TempTS);
	}
	
	return NULL;
}
#endif
