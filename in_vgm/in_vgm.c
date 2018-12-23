// Winamp VGM input plug-in
// ------------------------
// Programmed by Valley Bell, 2011-2017
//
// Made using the Example .RAW plug-in by Justin Frankel and
// small parts (mainly dialogues) of the old in_vgm 0.35 by Maxim.

/*3456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
0000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999*/

// #defines used by the Visual C++ project file:
//	ENABLE_ALL_CORES
//	DISABLE_HW_SUPPORT
//	UNICODE_INPUT_PLUGIN (Unicode build only)

#include <windows.h>
#include <stdio.h>
#include <locale.h>	// for setlocale
#include "Winamp/in2.h"
#include "Winamp/wa_ipc.h"

#include <zlib.h>	// for info in About message

#include "stdbool.h"
#include "chips/mamedef.h"	// for (U)INTxx types
#include "VGMPlay.h"
#include "VGMPlay_Intf.h"

#include "in_vgm.h"
#include "ini_func.h"


#ifndef WM_WA_MPEG_EOF
// post this to the main window at end of file (after playback as stopped)
#define WM_WA_MPEG_EOF	WM_USER+2
#endif


// configuration.
#define NUM_CHN		2
#define BIT_PER_SEC	16
#define SMPL_BYTES	(NUM_CHN * (BIT_PER_SEC / 8))


// Function Prototypes from dlg_cfg.c
BOOL CALLBACK ConfigDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
void Dialogue_TrackChange(void);

// Function Protorypes from dlg_fileinfo.c
void SetInfoDlgFile(const char* FileName);
void SetInfoDlgFileW(const wchar_t* FileName);
UINT32 GetVGZFileSize(const char* FileName);
UINT32 GetVGZFileSizeW(const wchar_t* FileName);
wchar_t* GetTagStringEngJap(wchar_t* TextEng, wchar_t* TextJap, bool LangMode);
UINT32 FormatVGMTag(const UINT32 BufLen, in_char* Buffer, GD3_TAG* FileTag, VGM_HEADER* FileHead);
bool LoadPlayingVGMInfo(const char* FileName);
bool LoadPlayingVGMInfoW(const wchar_t* FileName);
BOOL CALLBACK FileInfoDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);


// Function Prototypes
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved);
void Config(HWND hWndParent);
void About(HWND hWndParent);
void Init(void);
void FindIniFile(void);
void LoadConfigurationFile(void);
static void ReadIntoBitfield2(const char* Section, const char* Key, UINT16* Value,
							  UINT8 BitStart, UINT8 BitCount);
void SaveConfigurationFile(void);
static void WriteFromBitfield(const char* Section, const char* Key, UINT32 Value,
							  UINT8 BitStart, UINT8 BitCount);
void Deinit(void);
int IsOurFile(const in_char* fn);

INLINE UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator);
int Play(const in_char* FileName);
void Pause(void);
void Unpause(void);
int IsPaused(void);
void Stop(void);

int GetFileLength(VGM_HEADER* FileHead);
int GetLength(void);

int GetOutputTime(void);
void SetOutputTime(int time_in_ms);
void SetVolume(int volume);
void SetPan(int pan);
void UpdatePlayback(void);

int InfoDialog(const in_char* FileName, HWND hWnd);
const in_char* GetFileNameTitle(const in_char* FileName);
void GetFileInfo(const in_char* filename, in_char* title, int* length_in_ms);
void EQ_Set(int on, char data[10], int preamp);

DWORD WINAPI DecodeThread(LPVOID b);


#define PATH_SIZE	(MAX_PATH * 2)

// the output module
static In_Module WmpMod;

// currently playing file (used for getting info on the current file)
static in_char CurFileName[PATH_SIZE];

static int decode_pos;				// current decoding position (depends on SampleRate)
static int decode_pos_ms;			// Used for correcting DSP plug-in pitch changes
static volatile int seek_needed;	// if != -1, it is the point that the decode 
									// thread should seek to, in ms.

static volatile bool killDecodeThread = 0;			// the kill switch for the decode thread
static HANDLE thread_handle = INVALID_HANDLE_VALUE;	// the handle to the decode thread
static volatile bool InStopFunc;

char IniFilePath[PATH_SIZE + 0x10];	// 16 extra characters for "in_vgm.ini"
PLGIN_OPTS Options;



extern UINT32 SampleRate;
extern UINT32 VGMMaxLoop;
extern UINT32 VGMPbRate;

extern UINT32 FadeTime;
extern UINT32 PauseTime;

extern float VolumeLevel;
extern bool SurroundSound;
extern UINT8 HardStopOldVGMs;
//extern bool FadeRAWLog;

extern bool DoubleSSGVol;

extern UINT8 ResampleMode;	// 00 - HQ both, 01 - LQ downsampling, 02 - LQ both
extern UINT8 CHIP_SAMPLING_MODE;
extern INT32 CHIP_SAMPLE_RATE;

extern CHIPS_OPTION ChipOpts[0x02];

extern char* AppPaths[8];
static char AppPathBuffer[PATH_SIZE];

extern VGM_HEADER VGMHead;
extern GD3_TAG VGMTag;

extern UINT32 VGMMaxLoopM;
extern bool VGMEnd;
extern bool PausePlay;
extern bool EndPlay;



HANDLE hPluginInst;


// avoid CRT. Evil. Big. Bloated. Only uncomment this code if you are using 
// 'ignore default libraries' in VC++. Keeps DLL size way down.
// /*
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	hPluginInst = hInst;	// save for InitConfigDialog
	
	return TRUE;
}
// */

void Config(HWND hWndParent)
{
	// if we had a configuration box we'd want to write it here (using DialogBox, etc)
	DialogBox(WmpMod.hDllInstance, (LPTSTR)DlgConfigMain, hWndParent, &ConfigDialogProc);
	
	return;
}

void About(HWND hWndParent)
{
	const char* MsgStr =
		INVGM_TITLE
#ifdef UNICODE_INPUT_PLUGIN
		" (Unicode build)"
#endif
		"\n"
		"by Valley Bell 2011-2016\n"
		"\n"
		"Build date: " __DATE__ " (" INVGM_VERSION ")\n"
		"\n"
		"http://vgmrips.net/\n"
		"\n"
		"Current status:\n"
		"VGM file support up to version " VGM_VER_STR "\n"
		"Currently %u chips are emulated:\n"	// CHIP_COUNT
		"%s\n"	// ChipList
		"\n"
		"Don\'t be put off by the pre-1.0 version numbers.\n"
		"This is a non-commercial project and as such it is permanently in beta.\n"
		"\n"
		"Using:\n"
		"ZLib " ZLIB_VERSION " (http://www.zlib.org)\n"
		//"LZMA SDK 4.40 (http://www.7-zip.org)\n"		// currently unused
		"\n"
		"Thanks also go to:\n"
		"Maxim for the first version of in_vgm, one SN76489 and\n"
		"YM2413 core and some dialogues\n"
		"MAME team for most sound cores\n"
		"openMSX team for the YMF278B core\n"
		"GerbilSoft for the RF5C164 and PWM cores from Gens/GS\n"
		"DOSBox Team for AdLibEmu (OPL2/3 sound core)\n"
		"The author of Ootake for the HuC6280 core\n"
		"rainwarrior for NSFPlay and the EMU2149 and NES cores\n"
		"superctr for the new C352 core";
	char* ChipList;
	const char* ChipStr;
	char* FinalMsg;
	char* TempPnt;
	UINT8 CurChip;
	
	// generate Chip list
	ChipList = (char*)malloc(CHIP_COUNT * 12);
	TempPnt = ChipList;
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
	{
		if (CurChip && ! (CurChip % 6))
		{
			// replace space with new-line every 6 chips
			*(TempPnt - 1) = '\n';
		}
		
		ChipStr = GetAccurateChipName(CurChip, 0xFF);
		strcpy(TempPnt, ChipStr);
		TempPnt += strlen(TempPnt);
		strcpy(TempPnt, ", ");
		TempPnt += 2;
	}
	TempPnt -= 2;
	*TempPnt = '\0';
	
	FinalMsg = (char*)malloc(strlen(MsgStr) + 0x10 + strlen(ChipList));
	sprintf(FinalMsg, MsgStr, CHIP_COUNT, ChipList);
	free(ChipList);
	
	MessageBox(hWndParent, FinalMsg, WmpMod.description, MB_ICONINFORMATION | MB_OK);
	
	free(FinalMsg);
	
	return;
}

void Init(void)
{
	char* FileTitle;
	
	setlocale(LC_CTYPE, "");
	
	// any one-time initialization goes here (configuration reading, etc)
	VGMPlay_Init();	// General Init
	
	GetModuleFileName(hPluginInst, AppPathBuffer, PATH_SIZE);	// get path of in_vgm.dll
	GetFullPathName(AppPathBuffer, PATH_SIZE, AppPathBuffer, &FileTitle);  // find file title
	*FileTitle = '\0';
	
	// Path 1: dll's directory
	AppPaths[0x00] = AppPathBuffer;
	// Path 2: working directory ("\0")
	AppPaths[0x01] = AppPathBuffer + strlen(AppPathBuffer);
	
	LoadConfigurationFile();
	
	VGMPlay_Init2();	// Post-Config-Load Init
	
	return;
}

void FindIniFile(void)
{
	const char* WAIniDir;
	
	// get directory for Winamp INI files
	WAIniDir = (char *)SendMessage(WmpMod.hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
	if (WAIniDir == NULL)
	{
		// old Winamp
		strcpy(IniFilePath, AppPathBuffer);
	}
	else
	{
		// Winamp 5.11+ (with user profiles)
		strcpy(IniFilePath, WAIniDir);
		strcat(IniFilePath,"\\Plugins");
		
		// make sure folder exists
		CreateDirectory(IniFilePath, NULL);
		strcat(IniFilePath, "\\");
	}
	strcat(IniFilePath, "in_vgm.ini");
	
	return;
}

void LoadConfigurationFile(void)
{
	UINT8 CurCSet;
	UINT8 CurChip;
	UINT8 CurChn;
	CHIP_OPTS* TempCOpt;
	char TempStr[0x20];
	const char* ChipName;
	
	// -- set default values --
	// VGMPlay_Init() already sets most default values
	Options.ImmediateUpdate = false;
	Options.NoInfoCache = false;
	
	Options.SampleRate = 44100;
	Options.PauseNL = PauseTime;
	Options.PauseLp = PauseTime;
	
	strcpy(Options.TitleFormat, "%t (%g) - %a");
	Options.JapTags = false;
	Options.AppendFM2413 = false;
	Options.TrimWhitespc = true;
	Options.StdSeparators = true;
	Options.TagFallback = false;
	Options.MLFileType = 0;
	
	Options.Enable7z = false;
	
	Options.ResetMuting = true;
	DoubleSSGVol = true;
	
	FindIniFile();
	
	// Read actual options
	ReadIni_Boolean ("General",		"ImmdtUpdate",	&Options.ImmediateUpdate);
	ReadIni_Boolean ("General",		"NoInfoCache",	&Options.NoInfoCache);
	
	ReadIni_Integer	("Playback",	"SampleRate",	&Options.SampleRate);
	ReadIni_Integer	("Playback",	"FadeTime",		&FadeTime);
	ReadIni_Integer	("Playback",	"PauseNoLoop",	&Options.PauseNL);
	ReadIni_Integer	("Playback",	"PauseLoop",	&Options.PauseLp);
	ReadIni_IntByte	("Playback",	"HardStopOld",	&HardStopOldVGMs);
	ReadIni_Float	("Playback",	"Volume",		&VolumeLevel);
	ReadIni_Integer	("Playback",	"MaxLoops",		&VGMMaxLoop);
	ReadIni_Integer	("Playback",	"PlaybackRate",	&VGMPbRate);
	ReadIni_Boolean	("Playback",	"DoubleSSGVol",	&DoubleSSGVol);
	ReadIni_IntByte	("Playback",	"ResamplMode",	&ResampleMode);
	ReadIni_Integer	("Playback",	"ChipSmplRate",	&Options.ChipRate);
	ReadIni_IntByte	("Playback",	"ChipSmplMode",	&CHIP_SAMPLING_MODE);
	ReadIni_Boolean	("Playback",	"SurroundSnd",	&SurroundSound);
	
	ReadIni_String	("Tags",		"TitleFormat",	 Options.TitleFormat, 0x80);
	ReadIni_Boolean	("Tags",		"UseJapTags",	&Options.JapTags);
	ReadIni_Boolean	("Tags",		"AppendFM2413",	&Options.AppendFM2413);
	ReadIni_Boolean	("Tags",		"TrimWhitespc",	&Options.TrimWhitespc);
	ReadIni_Boolean	("Tags",		"SeparatorStd",	&Options.StdSeparators);
	ReadIni_Boolean	("Tags",		"TagFallback",	&Options.TagFallback);
	ReadIni_Integer	("Tags",		"MLFileType",	&Options.MLFileType);
	
	//ReadIni_Boolean	("Vgm7z",		"Enable",		&Options.Enable7z);
	
	ReadIni_Boolean ("Muting",		"Reset",		&Options.ResetMuting);
	
	TempCOpt = (CHIP_OPTS*)&ChipOpts[0x00];
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCOpt ++)
	{
		ReadIni_IntByte("EmuCore",	GetChipName(CurChip),	&TempCOpt->EmuCore);
	}
	
	for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
	{
		TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet];
		
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCOpt ++)
		{
			ChipName = GetChipName(CurChip);
			
			sprintf(TempStr, "%s #%u All", ChipName, CurCSet);
			ReadIni_Boolean("Muting",	TempStr,	&TempCOpt->Disabled);
			
			sprintf(TempStr, "%s #%u", ChipName, CurCSet);
			ReadIni_Integer("Muting",	TempStr,	&TempCOpt->ChnMute1);
			sprintf(TempStr, "%s #%u_%u", ChipName, CurCSet, 2);
			ReadIni_Integer("Muting",	TempStr,	&TempCOpt->ChnMute2);
			sprintf(TempStr, "%s #%u_%u", ChipName, CurCSet, 3);
			ReadIni_Integer("Muting",	TempStr,	&TempCOpt->ChnMute3);
			
			for (CurChn = 0x00; CurChn < TempCOpt->ChnCnt; CurChn ++)
			{
				if (TempCOpt->ChnCnt > 10)
					sprintf(TempStr, "%s #%u %02u", ChipName, CurCSet, CurChn);
				else
					sprintf(TempStr, "%s #%u %u", ChipName, CurCSet, CurChn);
				ReadIni_SIntSht("Panning",	TempStr,	&TempCOpt->Panning[CurChn]);
			}
			
		}
	}
	
	// Additional options
	// YM2612
	ChipName = GetChipName(0x02);	TempCOpt = &ChipOpts[0x00].YM2612;
	sprintf(TempStr, "%s Gens DACHighpass", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	sprintf(TempStr, "%s Gens SSG-EG", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 1, 1);
	
	sprintf(TempStr, "%s PseudoStereo", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 2, 1);
	
	sprintf(TempStr, "%s NukedType", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 3, 2);
	
	// YM2203
	ChipName = GetChipName(0x06);	TempCOpt = &ChipOpts[0x00].YM2203;
	sprintf(TempStr, "%s Disable AY", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	// YM2608
	ChipName = GetChipName(0x07);	TempCOpt = &ChipOpts[0x00].YM2608;
	sprintf(TempStr, "%s Disable AY", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	// YM2610
	ChipName = GetChipName(0x08);	TempCOpt = &ChipOpts[0x00].YM2610;
	sprintf(TempStr, "%s Disable AY", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	// GameBoy
	ChipName = GetChipName(0x13);	TempCOpt = &ChipOpts[0x00].GameBoy;
	sprintf(TempStr, "%s Boost WaveCh", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	// NES
	ChipName = GetChipName(0x14);	TempCOpt = &ChipOpts[0x00].NES;
	TempCOpt->SpecialFlags &= 0x7FFF;
	sprintf(TempStr, "%s Shared Opts", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 2);
	
	sprintf(TempStr, "%s APU Opts", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 2, 2);
	
	sprintf(TempStr, "%s DMC Opts", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 4, 8);
	
	sprintf(TempStr, "%s FDS Opts", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 12, 1);
	
	// OKIM6258
	ChipName = GetChipName(0x17);	TempCOpt = &ChipOpts[0x00].OKIM6258;
	sprintf(TempStr, "%s Internal 10bit", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	// SCSP
	ChipName = GetChipName(0x20);	TempCOpt = &ChipOpts[0x00].SCSP;
	sprintf(TempStr, "%s Bypass DSP", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	// C352
	ChipName = GetChipName(0x27);	TempCOpt = &ChipOpts[0x00].C352;
	sprintf(TempStr, "%s Disable Rear", ChipName);
	ReadIntoBitfield2("ChipOpts", TempStr, &TempCOpt->SpecialFlags, 0, 1);
	
	return;
}

static void ReadIntoBitfield2(const char* Section, const char* Key, UINT16* Value,
							  UINT8 BitStart, UINT8 BitCount)
{
	// Bitfield Read routine (2-byte/16-bit data)
	UINT16 BitMask;
	UINT16 NewBits;
	
	if (! BitCount)
		return;
	
	BitMask = (1 << BitCount) - 1;
	
	NewBits = (*Value >> BitStart) & BitMask;	// read old bits, making them the default data
	ReadIni_SIntSht(Section, Key, &NewBits);	// read .ini
	
	*Value &= ~(BitMask << BitStart);			// clear bit range
	*Value |= (NewBits & BitMask) << BitStart;	// add new bits in
	
	return;
}

void SaveConfigurationFile(void)
{
	UINT8 CurCSet;
	UINT8 CurChip;
	UINT8 CurChn;
	CHIP_OPTS* TempCOpt;
	char TempStr[0x20];
	const char* ChipName;
	
	WriteIni_Boolean("General",		"ImmdtUpdate",	Options.ImmediateUpdate);
	WriteIni_Boolean("General",		"NoInfoCache",	Options.NoInfoCache);
	
	WriteIni_Integer("Playback",	"SampleRate",	Options.SampleRate);
	WriteIni_Integer("Playback",	"FadeTime",		FadeTime);
	WriteIni_Integer("Playback",	"PauseNoLoop",	Options.PauseNL);
	WriteIni_Integer("Playback",	"PauseLoop",	Options.PauseLp);
	WriteIni_Integer("Playback",	"HardStopOld",	HardStopOldVGMs);
	WriteIni_Float	("Playback",	"Volume",		VolumeLevel);
	WriteIni_Integer("Playback",	"MaxLoops",		VGMMaxLoop);
	WriteIni_Integer("Playback",	"PlaybackRate",	VGMPbRate);
	WriteIni_Boolean("Playback",	"DoubleSSGVol",	DoubleSSGVol);
	WriteIni_Integer("Playback",	"ResamplMode",	ResampleMode);
	WriteIni_Integer("Playback",	"ChipSmplRate",	Options.ChipRate);
	WriteIni_Integer("Playback",	"ChipSmplMode",	CHIP_SAMPLING_MODE);
	WriteIni_Boolean("Playback",	"SurroundSnd",	SurroundSound);
	
	WriteIni_String	("Tags",		"TitleFormat",	Options.TitleFormat);
	WriteIni_Boolean("Tags",		"UseJapTags",	Options.JapTags);
	WriteIni_Boolean("Tags",		"AppendFM2413",	Options.AppendFM2413);
	WriteIni_Boolean("Tags",		"TrimWhitespc",	Options.TrimWhitespc);
	WriteIni_Boolean("Tags",		"SeparatorStd",	Options.StdSeparators);
	WriteIni_Boolean("Tags",		"TagFallback",	Options.TagFallback);
	WriteIni_XInteger("Tags",		"MLFileType",	Options.MLFileType);
	
	//WriteIni_Boolean("Vgm7z",		"Enable",		Options.Enable7z);
	
	TempCOpt = (CHIP_OPTS*)&ChipOpts[0x00];
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCOpt ++)
	{
		ChipName = GetChipName(CurChip);
		WriteIni_Integer("EmuCore",	ChipName,		TempCOpt->EmuCore);
	}
			
	WriteIni_Boolean("Muting",		"Reset",		Options.ResetMuting);
	
	for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
	{
		TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet];
		
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCOpt ++)
		{
			ChipName = GetChipName(CurChip);
			sprintf(TempStr, "%s #%u All", ChipName, CurCSet, 1);
			WriteIni_Boolean("Muting",	TempStr,	TempCOpt->Disabled);
			
			sprintf(TempStr, "%s #%u", ChipName, CurCSet, 1);
			WriteIni_XInteger("Muting",	TempStr,	TempCOpt->ChnMute1);
			if (CurChip == 0x07 || CurChip == 0x08 || CurChip == 0x0D)
			{
				// YM2608 (ADPCM), YM2610 (ADPCM), YMF278B (WaveTable)
				sprintf(TempStr, "%s #%u_%u", ChipName, CurCSet, 2);
				WriteIni_XInteger("Muting",	TempStr,	TempCOpt->ChnMute2);
			}
			if (CurChip == 0x06 || CurChip == 0x07 || CurChip == 0x08)
			{
				// YM2203, YM2608, YM2610 (all AY8910)
				sprintf(TempStr, "%s #%u_%u", ChipName, CurCSet, 3);
				WriteIni_XInteger("Muting",	TempStr,	TempCOpt->ChnMute3);
			}
			
			for (CurChn = 0x00; CurChn < TempCOpt->ChnCnt; CurChn ++)
			{
				if (TempCOpt->ChnCnt > 10)
					sprintf(TempStr, "%s #%u %02u", ChipName, CurCSet, CurChn);
				else
					sprintf(TempStr, "%s #%u %u", ChipName, CurCSet, CurChn);
				WriteIni_SInteger("Panning",	TempStr,	TempCOpt->Panning[CurChn]);
			}
		}
	}
	
	// Additional options
	// YM2612
	ChipName = GetChipName(0x02);	TempCOpt = &ChipOpts[0x00].YM2612;
	sprintf(TempStr, "%s Gens DACHighpass", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	sprintf(TempStr, "%s Gens SSG-EG", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 1, 1);
	
	sprintf(TempStr, "%s PseudoStereo", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 2, 1);
	
	sprintf(TempStr, "%s NukedType", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 3, 2);
	
	// YM2203
	ChipName = GetChipName(0x06);	TempCOpt = &ChipOpts[0x00].YM2203;
	sprintf(TempStr, "%s Disable AY", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	// YM2608
	ChipName = GetChipName(0x07);	TempCOpt = &ChipOpts[0x00].YM2608;
	sprintf(TempStr, "%s Disable AY", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	// YM2610
	ChipName = GetChipName(0x08);	TempCOpt = &ChipOpts[0x00].YM2610;
	sprintf(TempStr, "%s Disable AY", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	// GameBoy
	ChipName = GetChipName(0x13);	TempCOpt = &ChipOpts[0x00].GameBoy;
	sprintf(TempStr, "%s Boost WaveCh", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	// NES
	ChipName = GetChipName(0x14);	TempCOpt = &ChipOpts[0x00].NES;
	sprintf(TempStr, "%s Shared Opts", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 2);
	
	sprintf(TempStr, "%s APU Opts", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 2, 2);
	
	sprintf(TempStr, "%s DMC Opts", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 4, 8);
	
	sprintf(TempStr, "%s FDS Opts", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 12, 1);
	
	// OKIM6258
	ChipName = GetChipName(0x17);	TempCOpt = &ChipOpts[0x00].OKIM6258;
	sprintf(TempStr, "%s Internal 10bit", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	// SCSP
	ChipName = GetChipName(0x20);	TempCOpt = &ChipOpts[0x00].SCSP;
	sprintf(TempStr, "%s Bypass DSP", ChipName);
	//TempCOpt->SpecialFlags |= 0x01;	// force SCSP DSP bypass upon next loading
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	// C352
	ChipName = GetChipName(0x27);	TempCOpt = &ChipOpts[0x00].C352;
	sprintf(TempStr, "%s Disable Rear", ChipName);
	WriteFromBitfield("ChipOpts", TempStr, TempCOpt->SpecialFlags, 0, 1);
	
	return;
}

static void WriteFromBitfield(const char* Section, const char* Key, UINT32 Value,
							  UINT8 BitStart, UINT8 BitCount)
{
	UINT32 BitMask;
	UINT32 WrtBits;
	
	if (! BitCount)
		return;
	
	BitMask = (1 << BitCount) - 1;
	
	WrtBits = Value >> BitStart;
	WrtBits &= BitMask;
	WriteIni_XInteger(Section, Key, WrtBits);
	
	return;
}

void Deinit(void)
{
	// one-time deinit, such as memory freeing
	SaveConfigurationFile();
	
	VGMPlay_Deinit();
	
	return;
}

int IsOurFile(const in_char* fn)
{
	// used for detecting URL streams - currently unused.
	// return ! strncmp(fn,"http://",7); to detect HTTP streams, etc
	return 0;
} 


INLINE UINT32 MulDivRound(UINT64 Number, UINT64 Numerator, UINT64 Denominator)
{
	return (UINT32)((Number * Numerator + Denominator / 2) / Denominator);
}

// called when winamp wants to play a file
int Play(const in_char* FileName)
{
	UINT32 TempLng;
	int maxlatency;
	int thread_id;
	
	PausePlay = false;
	decode_pos = 0;
	decode_pos_ms = 0;
	seek_needed = -1;
	SampleRate = Options.SampleRate;
	CHIP_SAMPLE_RATE = Options.ChipRate ? Options.ChipRate : Options.SampleRate;
	
	// return -1 - jump to next file in playlist
	// return +1 - stop the playlist
#ifndef UNICODE_INPUT_PLUGIN
	if (! OpenVGMFile(FileName))
	{
		if (GetGZFileLength(FileName) == 0xFFFFFFFF)
			return -1;	// file not found
		else
			return +1;	// file invalid
	}
	
	LoadPlayingVGMInfo(FileName);
	strcpy(CurFileName, FileName);
#else
	if (! OpenVGMFileW(FileName))
	{
		if (GetGZFileLengthW(FileName) == 0xFFFFFFFF)
			return -1;	// file not found
		else
			return +1;	// file invalid
	}
	
	LoadPlayingVGMInfoW(FileName);
	wcscpy(CurFileName, FileName);
#endif
	
	// -1 and -1 are to specify buffer and prebuffer lengths.
	// -1 means to use the default, which all input plug-ins should really do.
	maxlatency = WmpMod.outMod->Open(SampleRate, NUM_CHN, BIT_PER_SEC, -1, -1);
	if (maxlatency < 0) // error opening device
		return +1;
	
	if (! VGMHead.lngLoopOffset)
		PauseTime = Options.PauseNL;
	else
		PauseTime = Options.PauseLp;
	
	PlayVGM();
	
#ifndef UNICODE_INPUT_PLUGIN
	TempLng = GetVGZFileSize(FileName);
#else
	TempLng = GetVGZFileSizeW(FileName);
#endif
	if (! TempLng)
	{
		if (VGMHead.lngGD3Offset)
			TempLng = VGMHead.lngGD3Offset - VGMHead.lngDataOffset;
		else
			TempLng = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
	}
	// Bit/Sec = (TrackBytes * 8) / TrackTime
	// kbps = Bytes * (8 * SampleRate) / (Samples * 1000)
	if (VGMHead.lngTotalSamples)
		TempLng = MulDivRound(TempLng, CalcSampleMSec(8000, 0x03),
								(UINT64)VGMHead.lngTotalSamples * 1000);
	else
		TempLng = 0;
	WmpMod.SetInfo(TempLng, (SampleRate + 500) / 1000, NUM_CHN, 1);
	
	// initialize visualization stuff
	WmpMod.SAVSAInit(maxlatency, SampleRate);
	WmpMod.VSASetInfo(SampleRate, NUM_CHN);
	
	// set the output plug-ins default volume.
	// volume is 0-255, -666 is a token for current volume.
	WmpMod.outMod->SetVolume(-666); 
	
	// launch decode thread
	killDecodeThread = 0;
	thread_handle = CreateThread(NULL, 0, &DecodeThread, NULL, 0, &thread_id);
	
	Dialogue_TrackChange();
	
	return 0; 
}

// standard pause implementation
void Pause(void)
{
	PauseVGM(true);
	WmpMod.outMod->Pause(PausePlay);
	
	return;
}

void Unpause(void)
{
	PauseVGM(false);
	WmpMod.outMod->Pause(PausePlay);
	
	return;
}

int IsPaused(void)
{
	return PausePlay;
}

// stop playing.
void Stop(void)
{
	if (InStopFunc)
		return;
	
	InStopFunc = true;
	// TODO: add Mutex for this block.
	//	Stupid XMPlay seems to call Stop() twice in 2 seperate threads.
	if (thread_handle != INVALID_HANDLE_VALUE)
	{
		killDecodeThread = 1;
		if (WaitForSingleObject(thread_handle, 10000) == WAIT_TIMEOUT)
		{
			MessageBox(WmpMod.hMainWindow, "Error asking thread to die!\n",
						"Error killing decode thread", 0);
			TerminateThread(thread_handle,0);
		}
		CloseHandle(thread_handle);
		thread_handle = INVALID_HANDLE_VALUE;
	}
	
	// close output system
	WmpMod.outMod->Close();
	
	// deinitialize visualization
	WmpMod.SAVSADeInit();
	
	StopVGM();
	
	if (Options.ResetMuting)
	{
		UINT8 CurChip;
		UINT8 CurCSet;
		CHIP_OPTS* TempCOpt;
		
		for (CurCSet = 0x00; CurCSet < 0x02; CurCSet ++)
		{
			TempCOpt = (CHIP_OPTS*)&ChipOpts[CurCSet];
			for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCOpt ++)
			{
				TempCOpt->Disabled = false;
				TempCOpt->ChnMute1 = 0x00;
				TempCOpt->ChnMute2 = 0x00;
				TempCOpt->ChnMute3 = 0x00;
			}
		}
		
		// that would just make the check boxes flash during track change
		// and Winamp's main window is locked when the configuration is open
		// so I do it only when muting is reset
		Dialogue_TrackChange();
	}
	
	CloseVGMFile();
	LoadPlayingVGMInfo(NULL);
	
	InStopFunc = false;
	return;
}


int GetFileLength(VGM_HEADER* FileHead)
{
	UINT32 SmplCnt;
	UINT32 MSecCnt;
	INT32 LoopCnt;
	
	if (FileHead == &VGMHead)
	{
		LoopCnt = VGMMaxLoopM;
	}
	else
	{
		LoopCnt = (VGMMaxLoop * FileHead->bytLoopModifier + 0x08) / 0x10 - FileHead->bytLoopBase;
		if (LoopCnt < 0x01)
			LoopCnt = 0x01;
	}
	
	if (! LoopCnt && FileHead->lngLoopSamples)
		return -1000;
	
	// Note: SmplCnt is ALWAYS 44.1 KHz, VGM's native sample rate
	SmplCnt = FileHead->lngTotalSamples + FileHead->lngLoopSamples * (LoopCnt - 0x01);
	if (FileHead == &VGMHead)
		MSecCnt = CalcSampleMSec(SmplCnt, 0x02);
	else
		MSecCnt = CalcSampleMSecExt(SmplCnt, 0x02, FileHead);
	
	if (FileHead->lngLoopSamples)
		MSecCnt += FadeTime + Options.PauseLp;
	else
		MSecCnt += Options.PauseNL;
	
	return MSecCnt;
}

int GetLength(void)	// return length of playing track
{
	return GetFileLength(&VGMHead);
}


// returns current output position, in ms.
// you could just use return mod.outMod->GetOutputTime(),
// but the dsp plug-ins that do tempo changing tend to make that wrong.
int GetOutputTime(void)
{
	if (seek_needed == -1) // seek is needed.
		return decode_pos_ms + (WmpMod.outMod->GetOutputTime() -
								WmpMod.outMod->GetWrittenTime());
	else
		return seek_needed;
}

void SetOutputTime(int time_in_ms)	// for seeking
{
	// Winamp seems send negative seeking values,
	// when GetLength returns -1000.
	if (time_in_ms >= 0)
		seek_needed = time_in_ms;
	
	return;
}

void SetVolume(int volume)
{
	WmpMod.outMod->SetVolume(volume);
	
	return;
}

void SetPan(int pan)
{
	WmpMod.outMod->SetPan(pan);
	
	return;
}

void UpdatePlayback(void)
{
	if (WmpMod.outMod == NULL || ! WmpMod.outMod->IsPlaying())
		return;
	
	if (Options.ImmediateUpdate)
	{
		// add 30 ms - else it sounds like you seek back a little
		SetOutputTime(GetOutputTime() + 30);
	}
	
	return;
}


int InfoDialog(const in_char* FileName, HWND hWnd)
{
	//MessageBox(hWnd, "No File Info. Yet.", "File Info", MB_OK);
#ifndef UNICODE_INPUT_PLUGIN
	SetInfoDlgFile(FileName);
#else
	SetInfoDlgFileW(FileName);
#endif
	
	return DialogBox(WmpMod.hDllInstance, (LPTSTR)DlgFileInfo, hWnd, &FileInfoDialogProc);
}

const in_char* GetFileNameTitle(const in_char* FileName)
{
	const in_char* TempPnt;
	
#ifndef UNICODE_INPUT_PLUGIN
	TempPnt = FileName + strlen(FileName) - 0x01;
	while(TempPnt >= FileName && *TempPnt != '\\')
		TempPnt --;
#else
	TempPnt = FileName + wcslen(FileName) - 0x01;
	while(TempPnt >= FileName && *TempPnt != L'\\')
		TempPnt --;
#endif
	
	return TempPnt + 0x01;
}

// This is an odd function. It is used to get the title and/or length of a track.
// If filename is either NULL or of length 0, it means you should
// return the info of LastFileName. Otherwise, return the information
// for the file in filename.
void GetFileInfo(const in_char* filename, in_char* title, int* length_in_ms)
{
	UINT32 FileSize;
	VGM_HEADER FileHead;
	GD3_TAG FileTag;
	const wchar_t* Tag_TrackName;
	
#if 0
	{
		char MsgStr[MAX_PATH * 2];
		sprintf(MsgStr, "Calling GetFileInfo() with file:\n%ls", filename);
		MessageBoxA(WmpMod.hMainWindow, MsgStr, WmpMod.description, MB_ICONINFORMATION | MB_OK);
	}
#endif
	
	// Note: If filename is be null OR of length zero, return info about the current file.
	if (filename == NULL || filename[0x00] == '\0')
	{
		// called when the Play-Button is pressed
		if (length_in_ms != NULL)
			*length_in_ms = GetLength();
		if (title != NULL) // get non-path portion.of filename
		{
			if (! VGMTag.lngVersion)
				Tag_TrackName = NULL;
			else
				Tag_TrackName = GetTagStringEngJap(VGMTag.strTrackNameE, VGMTag.strTrackNameJ, false);
			if (Tag_TrackName != NULL)
			{
				//_snprintf(title, GETFILEINFO_TITLE_LENGTH, "%ls (%ls)",
				//			VGMTag.strTrackNameE, VGMTag.strGameNameE);
				FormatVGMTag(GETFILEINFO_TITLE_LENGTH, title, &VGMTag, &VGMHead);
			}
			else
			{
#ifndef UNICODE_INPUT_PLUGIN
				strncpy(title, GetFileNameTitle(CurFileName), GETFILEINFO_TITLE_LENGTH);
#else
				wcsncpy(title, GetFileNameTitle(CurFileName), GETFILEINFO_TITLE_LENGTH);
#endif
			}
		}
	}
	else // some other file
	{
#ifndef UNICODE_INPUT_PLUGIN
		//FileSize = GetVGMFileInfo(filename, (length_in_ms != NULL) ? &FileHead : NULL,
		//							(title != NULL) ? &FileTag : NULL);
		FileSize = GetVGMFileInfo(filename, &FileHead, (title != NULL) ? &FileTag : NULL);
#else
		FileSize = GetVGMFileInfoW(filename, &FileHead, (title != NULL) ? &FileTag : NULL);
#endif
		if (length_in_ms != NULL)
		{
			if (FileSize)
				*length_in_ms = GetFileLength(&FileHead);
			else
				*length_in_ms = -1000;	// File not found
		}
		if (title != NULL)
		{
			if (! FileSize || ! FileTag.lngVersion)
				Tag_TrackName = NULL;
			else
				Tag_TrackName = GetTagStringEngJap(FileTag.strTrackNameE, FileTag.strTrackNameJ, false);
			if (Tag_TrackName != NULL)
			{
				//_snprintf(title, GETFILEINFO_TITLE_LENGTH, "%ls (%ls)",
				//			FileTag.strTrackNameE, FileTag.strGameNameE);
				FormatVGMTag(GETFILEINFO_TITLE_LENGTH, title, &FileTag, &FileHead);
				FreeGD3Tag(&FileTag);
			}
			else
			{
#ifndef UNICODE_INPUT_PLUGIN
				strncpy(title, GetFileNameTitle(filename), GETFILEINFO_TITLE_LENGTH);
#else
				wcsncpy(title, GetFileNameTitle(filename), GETFILEINFO_TITLE_LENGTH);
#endif
			}
		}
	}
	
	return;
}

void EQ_Set(int on, char data[10], int preamp)
{
	// unsupported
	
	// Format: each data byte is 0-63 (+20db <-> -20db) and preamp is the same. 
	return;
}


#define RENDER_SAMPLES	576	// visualizations look best with 576 samples
#define BLOCK_SIZE		(RENDER_SAMPLES * SMPL_BYTES)

DWORD WINAPI DecodeThread(LPVOID b)
{
	char sample_buffer[BLOCK_SIZE * 2];	// has to be twice as big as the blocksize
	UINT32 RetSamples;
	
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	
	while (! killDecodeThread) 
	{ 
		if (seek_needed != -1) // seek is needed.
		{
			decode_pos = MulDivRound(seek_needed, SampleRate, 1000);
			//decode_pos_ms = MulDivRound(decode_pos, 1000, SampleRate);
			decode_pos_ms = seek_needed;
			
			SeekVGM(false, decode_pos);
			
			// flush output device and set output to seek position
			WmpMod.outMod->Flush(decode_pos_ms);
			seek_needed = -1;
		}
		
		if (EndPlay /*|| VGMEnd*/)
		{
			// Playback finished
			WmpMod.outMod->CanWrite();		// needed for some output drivers
			
			if (! WmpMod.outMod->IsPlaying())
			{
				// we're done playing, so tell Winamp and quit the thread.
				PostMessage(WmpMod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
				break;
			}
			Sleep(10);		// give a little CPU time back to the system.
		}
		else if (WmpMod.outMod->CanWrite() >= (BLOCK_SIZE * (WmpMod.dsp_isactive() ? 2 : 1)))
			// CanWrite() returns the number of bytes you can write, so we check that
			// to the block size. the reason we multiply the block size by two if 
			// mod.dsp_isactive() is that DSP plug-ins can change it by up to a 
			// factor of two (for tempo adjustment).
		{	
			RetSamples = FillBuffer((WAVE_16BS*)sample_buffer, RENDER_SAMPLES);
			if (RetSamples)
			{
				// send data to the visualization and dsp systems
				WmpMod.SAAddPCMData(sample_buffer, NUM_CHN, BIT_PER_SEC, decode_pos_ms);
				WmpMod.VSAAddPCMData(sample_buffer, NUM_CHN, BIT_PER_SEC, decode_pos_ms);
				if (WmpMod.dsp_isactive())
					RetSamples = WmpMod.dsp_dosamples((short*)sample_buffer, RetSamples,
														BIT_PER_SEC, NUM_CHN, SampleRate);
				
				WmpMod.outMod->Write(sample_buffer, RetSamples * SMPL_BYTES);
				
				decode_pos += RetSamples;
				decode_pos_ms = MulDivRound(decode_pos, 1000, SampleRate);
			}
			else
			{
				//done = true;
			}
		}
		else
		{
			Sleep(20);	// Wait a little, until we can write again.
		}
	}
	
	return 0;
}


// module definition.
In_Module WmpMod =
{
	IN_VER,	// defined in IN2.H
	INVGM_TITLE_FULL
	// winamp runs on both alpha systems and x86 ones. :)
#ifdef __alpha
	" (AXP)"
//#else
//	" (x86)"
#endif
	,
	0,	// hMainWindow (filled in by winamp)
	0,	// hDllInstance (filled in by winamp)
	// Format List: "EXT\0Description\0EXT\0Description\0 ..."
	"vgm;vgz\0VGM Audio Files (*.vgm; *.vgz)\0",
	//"vgm;vgz\0VGM Audio Files (*.vgm; *.vgz)\0vgm7z\0VGM7Z Archive (*.vgm7z)\0",
	1,	// is_seekable
	IN_MODULE_FLAG_USES_OUTPUT_PLUGIN,
	&Config,
	&About,
	&Init,
	&Deinit,
	&GetFileInfo,
	&InfoDialog,
	&IsOurFile,
	&Play,
	&Pause,
	&Unpause,
	&IsPaused,
	&Stop,
	
	&GetLength,
	&GetOutputTime,
	&SetOutputTime,
	
	&SetVolume,
	&SetPan,
	
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,	// visualization calls filled in by winamp
	
	NULL, NULL,	// dsp calls filled in by winamp
	
	&EQ_Set,
	
	NULL,	// setinfo call filled in by winamp
	
	NULL	// out_mod filled in by winamp
};

// exported symbol. Returns output module.
__declspec(dllexport) In_Module* winampGetInModule2(void)
{
	return &WmpMod;
}
