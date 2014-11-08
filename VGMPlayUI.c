// VGMPlayUI.c: C Source File for the Console User Interface

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "stdbool.h"
#include <math.h>

#ifdef WIN32
#include <conio.h>
#include <windows.h>
#else
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <limits.h>
#include <termios.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#endif

#include "chips/mamedef.h"

#include "Stream.h"
#include "VGMPlay.h"
#include "VGMPlay_Intf.h"

#ifdef XMAS_EXTRA
#include "XMasBonus.h"
#endif

#ifndef WIN32
void WaveOutLinuxCallBack(void);
#endif

#ifdef WIN32
#define DIR_CHR		'\\'
#define DIR_STR		"\\"
#define QMARK_CHR	'\"'
#else
#define DIR_CHR		'/'
#define DIR_STR		"/"
#define QMARK_CHR	'\''
#endif


int main(int argc, char* argv[]);
static void RemoveQuotationMarks(char* String);
#ifdef WIN32
static void FixFilenameCodepage(char* FileName);
static void WinNT_Check(void);
#endif
static char* GetAppFileName(void);
void cls(void);
#ifndef WIN32
void changemode(bool);
int _kbhit(void);
int _getch(void);
static void Sleep(UINT32 msec);
#endif
static INT8 stricmp_u(const char *string1, const char *string2);
static INT8 strnicmp_u(const char *string1, const char *string2, size_t count);
static void ReadOptions(const char* AppName);
static bool GetBoolFromStr(const char* TextStr);
#ifdef XMAS_EXTRA
static bool XMas_Extra(char* FileName, bool Mode);
#endif
static bool OpenPlayListFile(const char* FileName);
static bool OpenMusicFile(const char* FileName);
extern bool OpenVGMFile(const char* FileName);
extern bool OpenOtherFile(const char* FileName);

static char CharConversion(char Char, UINT8 Mode);
static void printc(const char* format, ...);
static void PrintChipStr(UINT8 ChipID, UINT8 SubType, UINT32 Clock);
static void ShowVGMTag(void);

static void PlayVGM_UI(void);
static INT8 sign(double Value);
static long int Round(double Value);
static double RoundSpecial(double Value, double RoundTo);
static void PrintMinSec(UINT32 SamplePos, UINT32 SmplRate);

INLINE INT32 SampleVGM2Playback(INT32 SampleVal);
INLINE INT32 SamplePlayback2VGM(INT32 SampleVal);


// Options Variables
extern UINT32 SampleRate;	// Note: also used by some sound cores to determinate the chip sample rate

extern UINT32 VGMMaxLoop;
extern UINT32 CMFMaxLoop;
UINT32 FadeTimeN;	// normal fade time
UINT32 FadeTimePL;	// in-playlist fade time
extern UINT32 FadeTime;
UINT32 PauseTimeJ;	// Pause Time for Jingles
UINT32 PauseTimeL;	// Pause Time for Looping Songs
extern UINT32 PauseTime;

extern float VolumeLevel;
extern bool SurroundSound;
extern bool FadeRAWLog;
UINT8 LogToWave;
extern bool FullBufFill;
bool PauseEmulate;
UINT16 ForceAudioBuf;

extern UINT8 ResampleMode;	// 00 - HQ both, 01 - LQ downsampling, 02 - LQ both
extern UINT8 CHIP_SAMPLING_MODE;
extern INT32 CHIP_SAMPLE_RATE;

extern UINT16 FMPort;
extern bool UseFM;
extern bool FMForce;
//extern bool FMAccurate;
extern bool FMBreakFade;

extern CHIPS_OPTION ChipOpts[0x02];


extern bool ThreadPauseEnable;
extern bool ThreadPauseConfrm;
extern bool ThreadNoWait;	// don't reset the timer
extern UINT16 AUDIOBUFFERU;
extern UINT32 SMPL_P_BUFFER;
extern char SoundLogFile[MAX_PATH];

extern UINT8 OPL_MODE;
extern UINT8 OPL_CHIPS;
//extern bool WINNT_MODE;
UINT8 NEED_LARGE_AUDIOBUFS;

extern char* AppPathExt;

char PLFileBase[MAX_PATH];
char PLFileName[MAX_PATH];
UINT32 PLFileCount;
char** PlayListFile;
UINT32 CurPLFile;
UINT8 NextPLCmd;
bool FirstInit;
extern bool AutoStopSkip;

char VgmFileName[MAX_PATH];
UINT8 FileMode;
extern VGM_HEADER VGMHead;
extern UINT32 VGMDataLen;
extern UINT8* VGMData;
extern GD3_TAG VGMTag;

extern bool PauseThread;
static bool StreamStarted;

extern float MasterVol;

extern UINT32 VGMPos;
extern INT32 VGMSmplPos;
extern INT32 VGMSmplPlayed;
extern INT32 VGMSampleRate;
extern UINT32 BlocksSent;
extern UINT32 BlocksPlayed;
bool IsRAWLog;
extern bool EndPlay;
extern bool PausePlay;
extern bool FadePlay;
extern bool ForceVGMExec;
extern UINT8 PlayingMode;

extern UINT32 PlayingTime;

extern UINT32 FadeStart;
extern UINT32 VGMMaxLoopM;
extern UINT32 VGMCurLoop;
extern float VolumeLevelM;
bool ErrorHappened;
extern float FinalVol;
extern bool ResetPBTimer;

#ifndef WIN32
static struct termios oldterm;
static bool termmode;
#endif

UINT8 CmdList[0x100];

//extern UINT8 DISABLE_YMZ_FIX;
extern UINT8 IsVGMInit;

static bool PrintMSHours;

int main(int argc, char* argv[])
{
	const char* AppName;
#ifdef XMAS_EXTRA
	bool XMasEnable;
#endif
	const char* FileExt;
	
#ifndef WIN32
	tcgetattr(STDIN_FILENO, &oldterm);
	termmode = false;
#endif
	
	printf("VGM Player");
#ifdef XMAS_EXTRA
	printf("- XMas Release");
#endif
	printf("\n----------\n");
	
	//if (argv[0x00] == NULL)
	//	printf("Argument \"Application-Name\" is NULL!\n");
	
	// Warning! It's dangerous to use Argument 0!
	// AppName may be "vgmplay" instead of "vgmplay.exe"
	
	VGMPlay_Init();
	
	//AppName = argv[0x00];
	AppName = GetAppFileName();
	FileExt = strrchr(AppName, DIR_CHR) + 0x01;
	strncpy(AppPathExt, AppName, (int)(FileExt - AppName));
	AppPathExt[FileExt - AppName] = 0x00;
#ifdef _DEBUG
	ReadOptions(FileExt);
	AppPathExt[FileExt - AppName - 0x01] = 0x00;
	FileExt = strrchr(AppPathExt, DIR_CHR) + 0x01;
	AppPathExt[FileExt - AppPathExt] = 0x00;
#else
	ReadOptions(AppName);
#endif
	VGMPlay_Init2();
	
	printf("\nFile Name:\t");
	if (argc <= 0x01)
	{
		gets(VgmFileName);
		RemoveQuotationMarks(VgmFileName);
	}
	else
	{
		strcpy(VgmFileName, argv[0x01]);
		printf("%s\n", VgmFileName);
	}
	if (! strlen(VgmFileName))
		return 0;
	
#ifdef WIN32
	FixFilenameCodepage(VgmFileName);	// fix stupid DOS<->Windows Charset conflict
#endif
	
#ifdef XMAS_EXTRA
	XMasEnable = XMas_Extra(VgmFileName, 0x00);
#endif
	FileExt = strrchr(VgmFileName, '.');
	FirstInit = true;
	StreamStarted = false;
	if (FileExt == NULL || stricmp_u(FileExt + 1, "m3u"))
	{
		PLFileCount = 0x00;
		CurPLFile = 0x00;
		// no Play List File
		if (! OpenMusicFile(VgmFileName))
		{
			printf("Error opening the file!\n");
			_getch();
			return 0;
		}
		printf("\n");
		
		ErrorHappened = false;
		FadeTime = FadeTimeN;
		PauseTime = PauseTimeL;
		PrintMSHours = (VGMHead.lngTotalSamples >= 158760000);	// 44100 smpl * 60 sec * 60 min
		ShowVGMTag();
		NextPLCmd = 0x80;
		PlayVGM_UI();
		
		CloseVGMFile();
	}
	else
	{
		strcpy(PLFileName, VgmFileName);
		if (! OpenPlayListFile(PLFileName))
		{
			printf("Error opening the file!\n");
			_getch();
			return 0;
		}
		
		for (CurPLFile = 0x00; CurPLFile < PLFileCount; CurPLFile ++)
		{
			cls();
			printf("VGM Player");
			printf("\n----------\n");
			printf("\nPlaylist File:\t%s\n", PLFileName);
			printf("Playlist Entry:\t%u / %u\t", CurPLFile + 1, PLFileCount);
			printc("\nFile Name:\t%s\n", PlayListFile[CurPLFile]);
			
			strcpy(VgmFileName, PLFileBase);
			strcat(VgmFileName, PlayListFile[CurPLFile]);
			
			if (! OpenMusicFile(VgmFileName))
			{
				printf("Error opening the file!\n");
				_getch();
				continue;
			}
			printf("\n");
			
			ErrorHappened = false;
			if (CurPLFile < PLFileCount - 1)
				FadeTime = FadeTimePL;
			else
				FadeTime = FadeTimeN;
			PauseTime = VGMHead.lngLoopOffset ? PauseTimeL : PauseTimeJ;
			PrintMSHours = (VGMHead.lngTotalSamples >= 158760000);
			ShowVGMTag();
			NextPLCmd = 0x00;
			PlayVGM_UI();
			
			CloseVGMFile();
			
			if (ErrorHappened)
			{
				if (_kbhit())
					_getch();
				_getch();
				ErrorHappened = false;
			}
			if (NextPLCmd == 0xFF)
				break;
			else if (NextPLCmd == 0x01)
				CurPLFile -= 0x02;	// Jump to last File (-2 + 1 = -1)
		}
	}
	
	if (ErrorHappened)
	{
		if (_kbhit())
			_getch();
		_getch();
	}
	
#ifdef _DEBUG
	printf("Press any key ...");
	_getch();
#endif
	
#ifdef XMAS_EXTRA
	if (XMasEnable)
		XMas_Extra(VgmFileName, 0x01);
#endif
#ifndef WIN32
	changemode(false);
#endif
	
	return 0;
}

static void RemoveQuotationMarks(char* String)
{
	UINT32 StrLen;
	char* EndQMark;
	
	if (String[0x00] != QMARK_CHR)
		return;
	
	StrLen = strlen(String);
	memmove(String, String + 0x01, StrLen);	// Remove first char
	EndQMark = strrchr(String, QMARK_CHR);
	if (EndQMark != NULL)
		*EndQMark = 0x00;	// Remove last Quot.-Mark
	
	return;
}

#ifdef WIN32
static void FixFilenameCodepage(char* FileName)
{
	FILE* hFile;
	char* NewName;
	char* CurChr;
	char* NewChr;
	
	CurChr = FileName;
	while(! (*CurChr & 0x80))
	{
		if (! *CurChr)
			return;	// only ASCII-Chars used
		CurChr ++;
	}
	// Char 0x80-0xFF found
	
	hFile = fopen(FileName, "rb");
	if (hFile != NULL)
	{
		fclose(hFile);
		// File found - File Name OK
		return;
	}
	
	NewName = (char*)malloc(strlen(FileName) + 0x01);
	
	CurChr = FileName;
	NewChr = NewName;
	do
	{
		*NewChr = CharConversion(*CurChr, 0x01);
		CurChr ++;	NewChr ++;
	} while(*CurChr);
	*NewChr = 0x00;
	
	hFile = fopen(NewName, "rb");
	if (hFile != NULL)
	{
		fclose(hFile);
		// File found - use converted File Name
		strcpy(FileName, NewName);
	}
	free(NewName);
	
	return;
}

static void WinNT_Check(void)
{
	OSVERSIONINFO VerInf;
	
	VerInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&VerInf);
	//WINNT_MODE = (VerInf.dwPlatformId == VER_PLATFORM_WIN32_NT);
	
	/* Following Systems need larger Audio Buffers:
		- Windows 95 (500+ ms)
		- Windows Vista (200+ ms)
	Tested Systems:
		- Windows 95B
		- Windows 98 SE
		- Windows 2000
		- Windows XP (32-bit)
		- Windows Vista (32-bit)
		- Windows 7 (64-bit)
	*/
	
	NEED_LARGE_AUDIOBUFS = 0;
	if (VerInf.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
	{
		if (VerInf.dwMajorVersion == 4 && VerInf.dwMinorVersion == 0)
			NEED_LARGE_AUDIOBUFS = 50;	// Windows 95
	}
	else if (VerInf.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		if (VerInf.dwMajorVersion == 6 && VerInf.dwMinorVersion == 0)
			NEED_LARGE_AUDIOBUFS = 20;	// Windows Vista
	}
	
	return;
}
#endif

static char* GetAppFileName(void)
{
	char* AppPath;
	
	AppPath = (char*)malloc(MAX_PATH * sizeof(char));
#ifdef WIN32
	GetModuleFileName(NULL, AppPath, MAX_PATH);
#else
	readlink("/proc/self/exe", AppPath, PATH_MAX);
#endif
	
	return AppPath;
}

void cls(void)
{
#ifdef WIN32
	// CLS-Function from the MSDN Help
	HANDLE hConsole;
	COORD coordScreen = {0, 0};
	BOOL bSuccess;
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD dwConSize;
	
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	
	// get the number of character cells in the current buffer
	bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
	
	// fill the entire screen with blanks
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
	bSuccess = FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen,
											&cCharsWritten);
	
	// get the current text attribute
	//bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
	
	// now set the buffer's attributes accordingly
	//bSuccess = FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen,
	//										&cCharsWritten);
	
	// put the cursor at (0, 0)
	bSuccess = SetConsoleCursorPosition(hConsole, coordScreen);
#else
	system("clear");
#endif
	
	return;
}

#ifndef WIN32

void changemode(bool dir)
{
	static struct termios newterm;
	
	if (termmode == dir)
		return;
	
	if (dir)
	{
		newterm = oldterm;
		newterm.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newterm);
	}
	else
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
	}
	termmode = dir;
	
	return;
}

int _kbhit(void)
{
	struct timeval tv;
	fd_set rdfs;
	int kbret;
	bool needchg;
	
	needchg = (! termmode);
	if (needchg)
		changemode(true);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);
	
	select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
	kbret = FD_ISSET(STDIN_FILENO, &rdfs);
	if (needchg)
		changemode(false);
	
	return kbret;
}

int _getch(void)
{
	int ch;
	bool needchg;
	
	needchg = (! termmode);
	if (needchg)
		changemode(true);
	ch = getchar();
	if (needchg)
		changemode(false);
	
	return ch;
}

static void Sleep(UINT32 msec)
{
	usleep(msec * 1000);
	
	return;
}
#endif

static INT8 stricmp_u(const char *string1, const char *string2)
{
	// my own stricmp, because VC++6 doesn't find _stricmp when compiling without
	// standard libraries
	const char* StrPnt1;
	const char* StrPnt2;
	char StrChr1;
	char StrChr2;
	
	StrPnt1 = string1;
	StrPnt2 = string2;
	while(true)
	{
		StrChr1 = toupper(*StrPnt1);
		StrChr2 = toupper(*StrPnt2);
		
		if (StrChr1 < StrChr2)
			return -1;
		else if (StrChr1 > StrChr2)
			return +1;
		if (StrChr1 == 0x00)
			return 0;
		
		StrPnt1 ++;
		StrPnt2 ++;
	}
	
	return 0;
}

static INT8 strnicmp_u(const char *string1, const char *string2, size_t count)
{
	// my own strnicmp, because GCC doesn't seem to have _strnicmp
	const char* StrPnt1;
	const char* StrPnt2;
	char StrChr1;
	char StrChr2;
	size_t CurChr;
	
	StrPnt1 = string1;
	StrPnt2 = string2;
	CurChr = 0x00;
	while(CurChr < count)
	{
		StrChr1 = toupper(*StrPnt1);
		StrChr2 = toupper(*StrPnt2);
		
		if (StrChr1 < StrChr2)
			return -1;
		else if (StrChr1 > StrChr2)
			return +1;
		if (StrChr1 == 0x00)
			return 0;
		
		StrPnt1 ++;
		StrPnt2 ++;
		CurChr ++;
	}
	
	return 0;
}

static void ReadOptions(const char* AppName)
{
	const UINT8 CHN_COUNT[CHIP_COUNT] =
	{	0x04, 0x09, 0x06, 0x08, 0x10, 0x08, 0x03, 0x00,
		0x00, 0x09, 0x09, 0x09, 0x12, 0x00, 0x0C, 0x08,
		0x08, 0x00, 0x03, 0x04, 0x05, 0x1C, 0x00, 0x00,
		0x04, 0x05, 0x08, 0x08, 0x18, 0x04, 0x04
	};
	const UINT8 CHN_MASK_CNT[CHIP_COUNT] =
	{	0x04, 0x0E, 0x06, 0x08, 0x10, 0x08, 0x03, 0x06,
		0x06, 0x0E, 0x0E, 0x0E, 0x17, 0x18, 0x0C, 0x08,
		0x08, 0x00, 0x03, 0x04, 0x05, 0x1C, 0x00, 0x00,
		0x04, 0x05, 0x08, 0x08, 0x18, 0x04, 0x04
	};
	char* FileName;
	FILE* hFile;
	char TempStr[0x40];
	UINT32 StrLen;
	UINT32 TempLng;
	char* LStr;
	char* RStr;
	UINT8 IniSection;
	UINT8 CurChip;
	CHIP_OPTS* TempCOpt;
	UINT8 CurChn;
	char* TempPnt;
	bool TempFlag;
	
	// most defaults are set by VGMPlay_Init()
	FadeTimeN = FadeTime;
	PauseTimeJ = PauseTime;
	PauseTimeL = 0;
	LogToWave = 0x00;
	ForceAudioBuf = 0x00;
	
	if (AppName == NULL)
	{
		printf("Argument \"Application-Path\" is NULL!\nSkip loading INI.\n");
		return;
	}
	FileName = (char*)malloc(strlen(AppName) + 0x05);	// ".ini" + 00
	strcpy(FileName, AppName);
	LStr = strrchr(FileName, '.');
	RStr = strrchr(FileName, DIR_CHR);
	if (LStr <= RStr)
	{
		LStr = FileName + strlen(FileName);
		*LStr = '.';
	}
	strcpy(LStr + 0x01, "ini");
	
	hFile = fopen(FileName, "rt");
	if (hFile == NULL)
	{
		if (RStr == NULL)
			LStr = FileName;
		else
			LStr = RStr + 0x01;
		hFile = fopen(LStr, "rt");
		if (hFile == NULL)
		{
			printf("Failed to load INI.\n");
			return;
		}
	}
	
	IniSection = 0x00;
	while(! feof(hFile))
	{
		LStr = fgets(TempStr, 0x40, hFile);
		if (LStr == NULL)
			break;
		if (TempStr[0x00] == ';')	// Comment line
			continue;
		
		StrLen = strlen(TempStr) - 0x01;
		//if (TempStr[StrLen] == '\n')
		//	TempStr[StrLen] = 0x00;
		while(TempStr[StrLen] < 0x20)
		{
			TempStr[StrLen] = 0x00;
			if (! StrLen)
				break;
			StrLen --;
		}
		if (! StrLen)
			continue;
		StrLen ++;
		
		LStr = &TempStr[0x00];
		while(*LStr == ' ')
			LStr ++;
		if (LStr[0x00] == ';')	// Comment line
			continue;
		
		if (LStr[0x00] == '[')
			RStr = strchr(TempStr, ']');
		else
			RStr = strchr(TempStr, '=');
		if (RStr == NULL)
			continue;
		
		if (LStr[0x00] == '[')
		{
			// Line pattern: [Group]
			LStr ++;
			RStr = strchr(TempStr, ']');
			if (RStr != NULL)
				RStr[0x00] = 0x00;
			
			if (! stricmp_u(LStr, "General"))
			{
				IniSection = 0x00;
			}
			else
			{
				IniSection = 0xFF;
				for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
				{
					if (! stricmp_u(LStr, GetChipName(CurChip)))
					{
						IniSection = 0x80 | CurChip;
						break;
					}
				}
				if (IniSection == 0xFF)
					continue;
			}
		}
		else
		{
			// Line pattern: Option = Value
			TempLng = RStr - TempStr;
			TempStr[TempLng] = 0x00;
			
			// Prepare Strings (trim the spaces)
			RStr = &TempStr[TempLng - 0x01];
			while(*RStr == ' ')
				*(RStr --) = 0x00;
			
			RStr = &TempStr[StrLen - 0x01];
			while(*RStr == ' ')
				*(RStr --) = 0x00;
			RStr = &TempStr[TempLng + 0x01];
			while(*RStr == ' ')
				RStr ++;
			
			switch(IniSection)
			{
			case 0x00:	// General Sction
				if (! stricmp_u(LStr, "SampleRate"))
				{
					SampleRate = strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "FadeTime"))
				{
					FadeTimeN = strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "FadeTimePL"))
				{
					FadeTimePL = strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "JinglePause"))
				{
					PauseTimeJ = strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "FadeRAWLogs"))
				{
					FadeRAWLog = GetBoolFromStr(RStr);
				}
				else if (! stricmp_u(LStr, "Volume"))
				{
					VolumeLevel = (float)strtod(RStr, NULL);
				}
				else if (! stricmp_u(LStr, "LogSound"))
				{
					//LogToWave = GetBoolFromStr(RStr);
					LogToWave = (UINT8)strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "MaxLoops"))
				{
					VGMMaxLoop = strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "MaxLoopsCMF"))
				{
					CMFMaxLoop = strtoul(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "ResamplingMode"))
				{
					ResampleMode = (UINT8)strtol(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "ChipSmplMode"))
				{
					CHIP_SAMPLING_MODE = (UINT8)strtol(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "ChipSmplRate"))
				{
					CHIP_SAMPLE_RATE = strtol(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "AudioBuffers"))
				{
					ForceAudioBuf = (UINT16)strtol(RStr, NULL, 0);
					if (ForceAudioBuf < 0x04)
						ForceAudioBuf = 0x00;
				}
				else if (! stricmp_u(LStr, "SurroundSound"))
				{
					SurroundSound = GetBoolFromStr(RStr);
				}
				else if (! stricmp_u(LStr, "EmulatePause"))
				{
					PauseEmulate = GetBoolFromStr(RStr);
				}
				else if (! stricmp_u(LStr, "FMPort"))
				{
					FMPort = (UINT16)strtoul(RStr, NULL, 16);
				}
				else if (! stricmp_u(LStr, "FMForce"))
				{
					FMForce = GetBoolFromStr(RStr);
				}
				/*else if (! stricmp_u(LStr, "AccurateFM"))
				{
					FMAccurate = GetBoolFromStr(RStr);
				}*/
				else if (! stricmp_u(LStr, "FMSoftStop"))
				{
					FMBreakFade = GetBoolFromStr(RStr);
				}
				break;
			case 0x80:	// SN76496
			case 0x81:	// YM2413
			case 0x82:	// YM2612
			case 0x83:	// YM2151
			case 0x84:	// SegaPCM
			case 0x85:	// RF5C68
			case 0x86:	// YM2203
			case 0x87:	// YM2608
			case 0x88:	// YM2610
			case 0x89:	// YM3812
			case 0x8A:	// YM3526
			case 0x8B:	// Y8950
			case 0x8C:	// YMF262
			case 0x8D:	// YMF278B
			case 0x8E:	// YMF271
			case 0x8F:	// YMZ280B
			case 0x90:	// RF5C164
			case 0x91:	// PWM
			case 0x92:	// AY8910
			case 0x93:	// GameBoy
			case 0x94:	// NES
			case 0x95:	// MultiPCM
			case 0x96:	// UPD7759
			case 0x97:	// OKIM6258
			case 0x98:	// OKIM6295
			case 0x99:	// K051649
			case 0x9A:	// K054539
			case 0x9B:	// HuC6280
			case 0x9C:	// C140
			case 0x9D:	// K053260
			case 0x9E:	// Pokey
			case 0x9F:	// QSound
				CurChip = IniSection & 0x1F;
				TempCOpt = (CHIP_OPTS*)&ChipOpts[0x00] + CurChip;
				
				if (! stricmp_u(LStr, "Disabled"))
				{
					TempCOpt->Disabled = GetBoolFromStr(RStr);
				}
				else if (! stricmp_u(LStr, "EmulatorType"))
				{
					TempCOpt->EmuCore = (UINT8)strtol(RStr, NULL, 0);
				}
				else if (! stricmp_u(LStr, "MuteMask"))
				{
					if (! CHN_COUNT[CurChip])
						break;	// must use MuteMaskFM and MuteMask???
					TempCOpt->ChnMute1 = strtoul(RStr, NULL, 0);
					TempCOpt->ChnMute1 &= (1 << CHN_MASK_CNT[CurChip]) - 1;
				}
				else if (! strnicmp_u(LStr, "MuteCh", 0x06))
				{
					if (! CHN_COUNT[CurChip])
						break;	// must use MuteFM and Mute???
					CurChn = (UINT8)strtol(LStr + 0x06, &TempPnt, 0);
					if (TempPnt == NULL || *TempPnt)
						break;
					if (CurChn >= CHN_COUNT[CurChip])
						break;
					TempFlag = GetBoolFromStr(RStr);
					TempCOpt->ChnMute1 &= ~(0x01 << CurChn);
					TempCOpt->ChnMute1 |= TempFlag << CurChn;
				}
				else
				{
					switch(CurChip)
					{
					//case 0x00:	// SN76496
					case 0x02:	// YM2612
						if (! stricmp_u(LStr, "MuteDAC"))
						{
							CurChn = 0x06;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute1 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute1 |= TempFlag << CurChn;
						}
						else if (! stricmp_u(LStr, "DACHighpass"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 0);
							TempCOpt->SpecialFlags |= TempFlag << 0;
						}
						else if (! stricmp_u(LStr, "SSG-EG"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 1);
							TempCOpt->SpecialFlags |= TempFlag << 1;
						}
						break;
					//case 0x03:	// YM2151
					//case 0x04:	// SegaPCM
					//case 0x05:	// RF5C68
					case 0x06:	// YM2203
						if (! stricmp_u(LStr, "DisableAY"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 0);
							TempCOpt->SpecialFlags |= TempFlag << 0;
						}
						break;
					case 0x07:	// YM2608
					case 0x08:	// YM2610
						if (! stricmp_u(LStr, "DisableAY"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 0);
							TempCOpt->SpecialFlags |= TempFlag << 0;
						}
						else if (! stricmp_u(LStr, "MuteMask_FM"))
						{
							TempCOpt->ChnMute1 = strtoul(RStr, NULL, 0);
							TempCOpt->ChnMute1 &= (1 << CHN_MASK_CNT[CurChip]) - 1;
						}
						else if (! stricmp_u(LStr, "MuteMask_PCM"))
						{
							TempCOpt->ChnMute2 = strtoul(RStr, NULL, 0);
							TempCOpt->ChnMute2 &= (1 << CHN_MASK_CNT[CurChip]) - 1;
						}
						else if (! strnicmp_u(LStr, "MuteFMCh", 0x08))
						{
							CurChn = (UINT8)strtol(LStr + 0x08, &TempPnt, 0);
							if (TempPnt == NULL || *TempPnt)
								break;
							if (CurChn >= CHN_COUNT[CurChip])
								break;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute1 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute1 |= TempFlag << CurChn;
						}
						else if (! strnicmp_u(LStr, "MutePCMCh", 0x08))
						{
							CurChn = (UINT8)strtol(LStr + 0x08, &TempPnt, 0);
							if (TempPnt == NULL || *TempPnt)
								break;
							if (CurChn >= CHN_COUNT[CurChip])
								break;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute2 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute2 |= TempFlag << CurChn;
						}
						else if (! stricmp_u(LStr, "MuteDT"))
						{
							CurChn = 0x06;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute2 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute2 |= TempFlag << CurChn;
						}
						break;
					case 0x01:	// YM2413
					case 0x09:	// YM3812
					case 0x0A:	// YM3526
					case 0x0B:	// Y8950
					case 0x0C:	// YMF262
						CurChn = 0xFF;
						if (! stricmp_u(LStr, "MuteBD"))
							CurChn = 0x00;
						else if (! stricmp_u(LStr, "MuteSD"))
							CurChn = 0x01;
						else if (! stricmp_u(LStr, "MuteTOM"))
							CurChn = 0x02;
						else if (! stricmp_u(LStr, "MuteTC"))
							CurChn = 0x03;
						else if (! stricmp_u(LStr, "MuteHH"))
							CurChn = 0x04;
						else if (CurChip == 0x0B && ! stricmp_u(LStr, "MuteDT"))
							CurChn = 0x05;
						if (CurChn != 0xFF)
						{
							if (CurChip < 0x0C)
								CurChn += 9;
							else
								CurChn += 18;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute1 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute1 |= TempFlag << CurChn;
						}
						break;
					case 0x0D:	// YMF278B
						if (! stricmp_u(LStr, "MuteMask_FM"))
						{
							TempCOpt->ChnMute1 = strtoul(RStr, NULL, 0);
							TempCOpt->ChnMute1 &= (1 << CHN_MASK_CNT[CurChip - 0x01]) - 1;
						}
						else if (! stricmp_u(LStr, "MuteMask_WT"))
						{
							TempCOpt->ChnMute2 = strtoul(RStr, NULL, 0);
							TempCOpt->ChnMute2 &= (1 << CHN_MASK_CNT[CurChip]) - 1;
						}
						else if (! strnicmp_u(LStr, "MuteFMCh", 0x08))
						{
							CurChn = (UINT8)strtol(LStr + 0x08, &TempPnt, 0);
							if (TempPnt == NULL || *TempPnt)
								break;
							if (CurChn >= CHN_COUNT[CurChip - 0x01])
								break;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute1 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute1 |= TempFlag << CurChn;
						}
						else if (! strnicmp_u(LStr, "MuteFM", 0x06))
						{
							if (! stricmp_u(LStr + 4, "BD"))
								CurChn = 0x00;
							else if (! stricmp_u(LStr + 4, "SD"))
								CurChn = 0x01;
							else if (! stricmp_u(LStr + 4, "TOM"))
								CurChn = 0x02;
							else if (! stricmp_u(LStr + 4, "TC"))
								CurChn = 0x03;
							else if (! stricmp_u(LStr + 4, "HH"))
								CurChn = 0x04;
							if (CurChn != 0xFF)
							{
								CurChn += 18;
								TempFlag = GetBoolFromStr(RStr);
								TempCOpt->ChnMute1 &= ~(0x01 << CurChn);
								TempCOpt->ChnMute1 |= TempFlag << CurChn;
							}
						}
						else if (! strnicmp_u(LStr, "MuteWTCh", 0x08))
						{
							CurChn = (UINT8)strtol(LStr + 0x08, &TempPnt, 0);
							if (TempPnt == NULL || *TempPnt)
								break;
							if (CurChn >= CHN_COUNT[CurChip])
								break;
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->ChnMute2 &= ~(0x01 << CurChn);
							TempCOpt->ChnMute2 |= TempFlag << CurChn;
						}
						break;
					//case 0x0E:	// YMF271
					//case 0x0F:	// YMZ280B
						/*if (! stricmp_u(LStr, "DisableFix"))
						{
							DISABLE_YMZ_FIX = GetBoolFromStr(RStr);
						}
						break;*/
					//case 0x10:	// RF5C164
					//case 0x11:	// PWM
					//case 0x12:	// AY8910
					case 0x13:	// GameBoy
						if (! stricmp_u(LStr, "BoostWaveChn"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 0);
							TempCOpt->SpecialFlags |= TempFlag << 0;
						}
						else if (! stricmp_u(LStr, "LowerNoiseChn"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 1);
							TempCOpt->SpecialFlags |= TempFlag << 1;
						}
						else if (! stricmp_u(LStr, "Inaccurate"))
						{
							TempFlag = GetBoolFromStr(RStr);
							TempCOpt->SpecialFlags &= ~(0x01 << 2);
							TempCOpt->SpecialFlags |= TempFlag << 2;
						}
						break;
					//case 0x14:	// NES
					}
				}
				break;
			case 0xFF:	// Dummy Section
				break;
			}
		}
	}
	ChipOpts[0x01] = ChipOpts[0x00];
	
	fclose(hFile);
	
#ifdef WIN32
	WinNT_Check();
#endif
	if (CHIP_SAMPLE_RATE <= 0)
		CHIP_SAMPLE_RATE = SampleRate;
	
	return;
}

static bool GetBoolFromStr(const char* TextStr)
{
	if (! stricmp_u(TextStr, "True"))
		return true;
	else if (! stricmp_u(TextStr, "False"))
		return false;
	else
		return strtol(TextStr, NULL, 0) ? true : false;
}

#ifdef XMAS_EXTRA
static bool XMas_Extra(char* FileName, bool Mode)
{
	char* FileTitle;
	const UINT8* XMasData;
	UINT32 XMasSize;
	FILE* hFile;
	
	if (! Mode)
	{	// Prepare Mode
		FileTitle = NULL;
		XMasData = NULL;
		if (! strnicmp_u(FileName, "WEWISH", 0x10))
		{
			FileTitle = "WEWISH.CMF";
			XMasSize = sizeof(WEWISH_CMF);
			XMasData = WEWISH_CMF;
		}
		else if (! strnicmp_u(FileName, "tim7", 0x10))
		{
			FileTitle = "lem_tim7.vgz";
			XMasSize = sizeof(TIM7_VGZ);
			XMasData = TIM7_VGZ;
		}
		else if (! strnicmp_u(FileName, "jingleb", 0x10))
		{
			FileTitle = "lxmas_jb.dro";
			XMasSize = sizeof(JB_DRO);
			XMasData = JB_DRO;
		}
		else if (! strnicmp_u(FileName, "rudolph", 0x10))
		{
			FileTitle = "rudolph.dro";
			XMasSize = sizeof(RODOLPH_DRO);
			XMasData = RODOLPH_DRO;
		}
		else if (! strnicmp_u(FileName, "clyde", 0x10))
		{
			FileTitle = "clyde1_1.dro";
			XMasSize = sizeof(clyde1_1_dro);
			XMasData = clyde1_1_dro;
		}
		
		if (XMasData)
		{
#ifdef WIN32
			GetEnvironmentVariable("Temp", FileName, MAX_PATH);
#else
			FileName = "/tmp/"
#endif
			strcat(FileName, DIR_STR);
			if (FileTitle == NULL)
				FileTitle = "XMas.dat";
			strcat(FileName, FileTitle);
			
			hFile = fopen(FileName, "wb");
			if (hFile == NULL)
			{
				FileName[0x00] = 0x00;
				printf("Critical XMas-Error!\n");
				return false;
			}
			fwrite(XMasData, 0x01, XMasSize, hFile);
			fclose(hFile);
		}
		else
		{
			FileName = NULL;
			return false;
		}
	}
	else
	{	// Unprepare Mode
		if (! remove(FileName))
			return false;
		// btw: it's intentional that the user can grab the file from the temp-folder
	}
	
	return true;
}
#endif

static bool OpenPlayListFile(const char* FileName)
{
	const char M3UV2_HEAD[] = "#EXTM3U";
	const char M3UV2_META[] = "#EXTINF:";
	UINT32 METASTR_LEN;
	
	FILE* hFile;
	UINT32 LineNo;
	bool IsV2Fmt;
	UINT32 PLAlloc;
	char TempStr[0x1000];	// 4096 chars should be enough
	char* RetStr;
	
	hFile = fopen(FileName, "rt");
	if (hFile == NULL)
		return false;
	
	PLAlloc = 0x0100;
	PLFileCount = 0x00;
	LineNo = 0x00;
	IsV2Fmt = false;
	METASTR_LEN = strlen(M3UV2_META);
	PlayListFile = (char**)malloc(PLAlloc * sizeof(char*));
	while(! feof(hFile))
	{
		RetStr = fgets(TempStr, 0x1000, hFile);
		if (RetStr == NULL)
			break;
		//RetStr = strchr(TempStr, 0x0D);
		//if (RetStr)
		//	*RetStr = 0x00;	// remove NewLine-Character
		RetStr = TempStr + strlen(TempStr) - 0x01;
		while(RetStr >= TempStr && *RetStr < 0x20)
		{
			*RetStr = 0x00;	// remove NewLine-Characters
			RetStr --;
		}
		if (! strlen(TempStr))
			continue;
		
		if (! LineNo)
		{
			if (! strcmp(TempStr, M3UV2_HEAD))
			{
				IsV2Fmt = true;
				LineNo ++;
				continue;
			}
		}
		if (IsV2Fmt)
		{
			if (! strncmp(TempStr, M3UV2_META, METASTR_LEN))
			{
				// Ignore Metadata of m3u Version 2
				LineNo ++;
				continue;
			}
		}
		
		if (PLFileCount >= PLAlloc)
		{
			PLAlloc += 0x0100;
			PlayListFile = (char**)realloc(PlayListFile, PLAlloc * sizeof(char*));
		}
		
		PlayListFile[PLFileCount] = (char*)malloc((strlen(TempStr) + 0x01) * sizeof(char));
		strcpy(PlayListFile[PLFileCount], TempStr);
		PLFileCount ++;
		LineNo ++;
	}
	
	fclose(hFile);
	
	RetStr = strrchr(FileName, DIR_CHR);
	if (RetStr != NULL)
	{
		RetStr ++;
		strncpy(TempStr, FileName, RetStr - FileName);
		TempStr[RetStr - FileName] = 0x00;
/*#ifdef WIN32
		SetCurrentDirectory(TempStr);
#else
		LineNo = chdir(TempStr);
printf("Dir Change to: %s, Result %u\n", TempStr, LineNo);
_getch();
#endif*/
		strcpy(PLFileBase, TempStr);
	}
	else
	{
		strcpy(PLFileBase, "");
	}
	
	return true;
}

static bool OpenMusicFile(const char* FileName)
{
	if (OpenVGMFile(FileName))
		return true;
	else if (OpenOtherFile(FileName))
		return true;
	
	return false;
}

static char CharConversion(char Char, UINT8 Mode)
{
	// Mode	00	Windows -> DOS
	//		01	DOS -> Windows
	
	// actually I'd like to use "char", not "UINT8", but that generates 128 warnings
	UINT8 ConvTable[0x80] = {
		0x00, 0x00, 0x27, 0x9F, 0x22, 0x2E, 0xC5, 0xCE,		// 80 - 87
		0x5E, 0x25, 0x53, 0x3C, 0x4F, 0x00, 0x5A, 0x00,		// 88 - 8F
		0x00, 0x27, 0x27, 0x22, 0x22, 0x07, 0x2D, 0x2D,		// 90 - 97
		0x7E, 0x54, 0x73, 0x3E, 0x6F, 0x00, 0x7A, 0x59,		// 98 - 9F
		0xFF, 0xAD, 0xBD, 0x9C, 0xCF, 0xBE, 0xDD, 0xF5,		// A0 - A7
		0xF9, 0xB8, 0xA6, 0xAE, 0xAA, 0xF0, 0xA9, 0xEE,		// A8 - AF
		0xF8, 0xF1, 0xFD, 0xFC, 0xEF, 0xE6, 0xF4, 0xFA,		// B0 - B7
		0xF7, 0xFB, 0xA7, 0xAF, 0xAC, 0xAB, 0xF3, 0xA8,		// B8 - BF
		0xB7, 0xB5, 0xB6, 0xC7, 0x8E, 0x8F, 0x92, 0x80,		// C0 - C7
		0xD4, 0x90, 0xD2, 0xD3, 0xDE, 0xD6, 0xD7, 0xD8,		// C8 - CF
		0xD1, 0xA5, 0xE3, 0xE0, 0xE2, 0xE5, 0x99, 0x9E,		// D0 - D7
		0x9D, 0xEB, 0xE9, 0xEA, 0x9A, 0xED, 0xE8, 0xE1,		// D8 - DF
		0x85, 0xA0, 0x83, 0xC6, 0x84, 0x86, 0x91, 0x87,		// E0 - E7
		0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,		// E8 - EF
		0xD0, 0xA4, 0x95, 0xA2, 0x93, 0xE4, 0x94, 0xF6,		// F0 - F7
		0x9B, 0x97, 0xA3, 0x96, 0x81, 0xEC, 0xE7, 0x98};	// F8 - FF
	UINT8 CurChr;
	
	if (! (Char & 0x80))
		return Char;
	
	switch(Mode)
	{
	case 0x00:
		CurChr = Char & 0x7F;
		if (ConvTable[CurChr])
			return (char)ConvTable[CurChr];
	case 0x01:
		for (CurChr = 0x00; CurChr < 0x80; CurChr ++)
		{
			if (ConvTable[CurChr] == (UINT8)Char)
				return (char)(0x80 | CurChr);
		}
		break;
	}
	
	return 0x20;
}

static void printc(const char* format, ...)
{
#ifdef WIN32
	int RetVal;
	UINT32 BufSize;
	char* printbuf;
	va_list arg_list;
	UINT32 CurPos;
	
	va_start(arg_list, format);
	
	BufSize = 0x00;
	printbuf = NULL;
	do
	{
		BufSize += 0x100;
		printbuf = (char*)realloc(printbuf, BufSize);
		RetVal = _vsnprintf(printbuf, BufSize - 0x01, format, arg_list);
	} while(RetVal == -1);
	CurPos = 0x00;
	while(printbuf[CurPos])
	{
		printbuf[CurPos] = CharConversion(printbuf[CurPos], 0x00);
		CurPos ++;
	}
	printbuf[CurPos] = 0x00;
	
	printf("%s", printbuf);
	free(printbuf);
	va_end(arg_list);
#else
	va_list arg_list;
	
	va_start(arg_list, format);
	vprintf(format, arg_list);
	va_end(arg_list);
#endif
	
	return;
}

static void PrintChipStr(UINT8 ChipID, UINT8 SubType, UINT32 Clock)
{
	if (! Clock)
		return;
	
	if (ChipID == 0x00 && (Clock & 0x80000000))
		Clock &= ~0x40000000;
	if (Clock & 0x80000000)
	{
		Clock &= ~0x80000000;
		ChipID |= 0x80;
	}
	
	if (Clock & 0x40000000)
		printf("2x");
	printf("%s, ", GetAccurateChipName(ChipID, SubType));
	
	return;
}

static void ShowVGMTag(void)
{
	UINT8 CurChip;
	UINT32 ChpClk;
	UINT8 ChpType;
	
	printc("Track Title:\t%ls\n", VGMTag.strTrackNameE);
	printc("Game Name:\t%ls\n", VGMTag.strGameNameE);
	printc("System:\t\t%ls\n", VGMTag.strSystemNameE);
	printc("Composer:\t%ls\n", VGMTag.strAuthorNameE);
	printf("Release:\t%ls\n", VGMTag.strReleaseDate);
	printf("Version:\t%X.%02X\t\t", VGMHead.lngVersion >> 8, VGMHead.lngVersion & 0xFF);
	printf("Loop: ");
	if (VGMHead.lngLoopOffset)
	{
		printf("Yes (");
		PrintMinSec(VGMHead.lngLoopSamples, VGMSampleRate);
		printf(")\n");
	}
	else
	{
		printf("No\n");
	}
	printc("VGM by:\t\t%ls\n", VGMTag.strCreator);
	printc("Notes:\t\t%ls\n", VGMTag.strNotes);
	printf("\n");
	
	printf("Used chips:\t");
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
	{
		ChpClk = GetChipClock(&VGMHead, CurChip, &ChpType);
		if (ChpClk && GetChipClock(&VGMHead, 0x80 | CurChip, NULL))
			ChpClk |= 0x40000000;
		PrintChipStr(CurChip, ChpType, ChpClk);
	}
	printf("\b\b \n");
	printf("\n");
	
	return;
}


#define LOG_SAMPLES	(SampleRate / 5)
static void PlayVGM_UI(void)
{
	INT32 VGMPbSmplCount;
	INT32 PlaySmpl;
	UINT8 KeyCode;
	UINT32 VGMPlaySt;
	UINT32 VGMPlayEnd;
	char WavFileName[MAX_PATH];
	char* TempStr;
	WAVE_16BS* TempBuf;
	UINT8 RetVal;
	UINT32 TempLng;
	bool PosPrint;
	bool LastUninit;
	bool QuitPlay;
	UINT32 PlayTimeEnd;
	
	printf("Initializing ...\r");
	
	PlayVGM();
	
	/*switch(LogToWave)
	{
	case 0x00:
		break;
	case 0x01:
		// Currently there's no record for Hardware FM
		PlayingMode = 0x00;	// Impossible to log at full speed AND use FMPort
		break;
	case 0x02:
		if (PlayingMode == 0x01)
			LogToWave = 0x00;	// Output and log sound (FM isn't logged)
		break;
	}*/
	switch(PlayingMode)
	{
	case 0x00:
		AUDIOBUFFERU = 10;
		break;
	case 0x01:
		AUDIOBUFFERU = 0;	// no AudioBuffers needed
		break;
	case 0x02:
		AUDIOBUFFERU = 5;	// try to sync Hardware/Software Emulator as well as possible
		break;
	}
	if (AUDIOBUFFERU < NEED_LARGE_AUDIOBUFS)
		AUDIOBUFFERU = NEED_LARGE_AUDIOBUFS;
	if (ForceAudioBuf && AUDIOBUFFERU)
		AUDIOBUFFERU = ForceAudioBuf;
	
	switch(FileMode)
	{
	case 0x00:	// VGM
		// RAW Log: no loop, no Creator, System Name set
		IsRAWLog = (! VGMHead.lngLoopOffset && ! wcslen(VGMTag.strCreator) &&
					wcslen(VGMTag.strSystemNameE));
		break;
	case 0x01:	// CMF
		IsRAWLog = false;
		break;
	case 0x02:	// DRO
		IsRAWLog = true;
		break;
	}
	if (! VGMHead.lngTotalSamples)
		IsRAWLog = false;
	
#ifndef WIN32
	changemode(true);
#endif
	
	switch(PlayingMode)
	{
	case 0x00:
	case 0x02:
		if (LogToWave)
		{
			strcpy(WavFileName, VgmFileName);
			TempStr = strrchr(WavFileName, '.');
			if (TempStr == NULL)
				TempStr = WavFileName + strlen(WavFileName);
			strcpy(TempStr, ".wav");
			
			strcpy(SoundLogFile, WavFileName);
		}
		FullBufFill = ! LogToWave;
		
		switch(LogToWave)
		{
		case 0x00:
		case 0x02:
			SoundLogging(LogToWave ? true : false);
			if (LogToWave || (FirstInit || ! StreamStarted))
			{
				// support smooth transistions between songs
				RetVal = StartStream(0x00);
				if (RetVal)
				{
					printf("Error openning Sound Device!\n");
					return;
				}
				StreamStarted = true;
			}
			PauseStream(PausePlay);
			break;
		case 0x01:
			TempBuf = (WAVE_16BS*)malloc(SAMPLESIZE * LOG_SAMPLES);
			if (TempBuf == NULL)
			{
				printf("Allocation Error!\n");
				return;
			}
			
			StartStream(0xFF);
			RetVal = SaveFile(0x00000000, NULL);
			if (RetVal)
			{
				printf("Can't open %s!\n", SoundLogFile);
				return;
			}
			break;
		}
		break;
	case 0x01:
		// PlayVGM() does it all
		FullBufFill = true;
		break;
	}
	FirstInit = false;
	
	VGMPlaySt = VGMPos;
	if (VGMHead.lngGD3Offset)
		VGMPlayEnd = VGMHead.lngGD3Offset;
	else
		VGMPlayEnd = VGMHead.lngEOFOffset;
	VGMPlayEnd -= VGMPlaySt;
	if (! FileMode)
		VGMPlayEnd --;	// EOF Command doesn't count
	PosPrint = true;
	
	PlayTimeEnd = 0;
	QuitPlay = false;
	while(! QuitPlay)
	{
		if (! PausePlay || PosPrint)
		{
			PosPrint = false;
			
			VGMPbSmplCount = SampleVGM2Playback(VGMHead.lngTotalSamples);
			PlaySmpl = VGMPos - VGMPlaySt;
#ifdef WIN32
			printf("Playing %01.2f%%\t", 100.0 * PlaySmpl / VGMPlayEnd);
#else
			// \t doesn't display correctly under Linux
			// but \b causes flickering under Windows
			printf("Playing %01.2f%%   \b\b\b\t", 100.0 * PlaySmpl / VGMPlayEnd);
#endif
			if (LogToWave != 0x01)
			{
				PlaySmpl = (BlocksSent - BlocksPlayed) * SMPL_P_BUFFER;
				PlaySmpl = VGMSmplPlayed - PlaySmpl;
			}
			else
			{
				PlaySmpl = VGMSmplPlayed;
			}
			if (! VGMCurLoop)
			{
				if (PlaySmpl < 0)
					PlaySmpl = 0;
			}
			else
			{
				while(PlaySmpl < SampleVGM2Playback(VGMHead.lngTotalSamples - VGMHead.lngLoopSamples))
					PlaySmpl += SampleVGM2Playback(VGMHead.lngLoopSamples);
			}
			//if (PlaySmpl > VGMPbSmplCount)
			//	PlaySmpl = VGMPbSmplCount;
			PrintMinSec(PlaySmpl, SampleRate);
			printf(" / ");
			PrintMinSec(VGMPbSmplCount, SampleRate);
			printf(" seconds\r");
			
			if (LogToWave == 0x01)
			{
				TempLng = FillBuffer(TempBuf, LOG_SAMPLES);
				if (TempLng)
					SaveFile(TempLng, TempBuf);
				if (EndPlay)
					break;
			}
			else
			{
#ifdef WIN32
				Sleep(20);
#endif
			}
		}
		else
		{
#ifdef WIN32
			Sleep(1);
#endif
		}
#ifndef WIN32
		if (! PausePlay)
			WaveOutLinuxCallBack();
		else
			Sleep(100);
#endif
		
		if (EndPlay)
		{
			if (! PlayTimeEnd)
			{
				PlayTimeEnd = PlayingTime;
				// quitting now terminates the program, so I need some special
				// checks to make sure that the rest of the audio buffer is played
				if (! PLFileCount || CurPLFile >= PLFileCount - 0x01)
				{
					if (FileMode == 0x01)
						PlayTimeEnd += SampleRate << 1;	// Add 2 secs
					PlayTimeEnd += AUDIOBUFFERU * SMPL_P_BUFFER;
				}
			}
			
			if (PlayingTime >= PlayTimeEnd)
				QuitPlay = true;
		}
		if (_kbhit())
		{
			KeyCode = toupper(_getch());
			switch(KeyCode)
			{
#ifdef WIN32
			case 0xE0:	// Special Key
				// Cursor-Key Table
				// Shift + Cursor results in the usual value for the Cursor Key
				// Alt + Cursor results in 0x00 + (0x50 + CursorKey) (0x00 instead of 0xE0)
				//	Key		None	Ctrl
				//	Up		48		8D
				//	Down	50		91
				//	Left	4B		73
				//	Right	4D		74
				KeyCode = _getch();	// Get 2nd Key
#else
			case 0x1B:	// Special Key
				KeyCode = _getch();
				if (KeyCode == 0x1B || KeyCode == 0x00)
				{
					// ESC Key pressed
					QuitPlay = true;
					NextPLCmd = 0xFF;
					break;
				}
				switch(KeyCode)
				{
				case 0x5B:
					// Cursor-Key Table
					//	Key		KeyCode
					//	Up		41
					//	Down	42
					//	Left	44
					//	Right	43
					// Cursor only: CursorKey
					// Ctrl: 0x31 + 0x3B + 0x35 + CursorKey
					// Alt: 0x31 + 0x3B + 0x33 + CursorKey
					
					// Page-Keys: PageKey + 0x7E
					//	PageUp		35
					//	PageDown	36
					KeyCode = _getch();	// Get 2nd Key
					// Convert Cursor Key Code from Linux to Windows
					switch(KeyCode)
					{
					case 0x31:	// Ctrl or Alt key
						KeyCode = _getch();
						if (KeyCode == 0x3B)
						{
							KeyCode = _getch();
							if (KeyCode == 0x35)
							{
								KeyCode = _getch();
								switch(KeyCode)
								{
								case 0x41:
									KeyCode = 0x8D;
									break;
								case 0x42:
									KeyCode = 0x91;
									break;
								case 0x43:
									KeyCode = 0x74;
									break;
								case 0x44:
									KeyCode = 0x73;
									break;
								default:
									KeyCode = 0x00;
									break;
								}
							}
						}
						
						if ((KeyCode & 0xF0) == 0x30)
							KeyCode = 0x00;
						break;
					case 0x35:
						KeyCode = 0x49;
						_getch();
						break;
					case 0x36:
						KeyCode = 0x51;
						_getch();
						break;
					case 0x41:
						KeyCode = 0x48;
						break;
					case 0x42:
						KeyCode = 0x50;
						break;
					case 0x43:
						KeyCode = 0x4D;
						break;
					case 0x44:
						KeyCode = 0x4B;
						break;
					default:
						KeyCode = 0x00;
						break;
					}
				}
#endif
				switch(KeyCode)
				{
				case 0x4B:	// Cursor Left
					PlaySmpl = -5;
					break;
				case 0x4D:	// Cursor Right
					PlaySmpl = 5;
					break;
				case 0x73:	// Ctrl + Cursor Left
					PlaySmpl = -60;
					break;
				case 0x74:	// Ctrl + Cursor Right
					PlaySmpl = 60;
					break;
				case 0x49:	// Page Up
					if (PLFileCount && /*! NextPLCmd &&*/ CurPLFile)
					{
						NextPLCmd = 0x01;
						QuitPlay = true;
					}
					PlaySmpl = 0;
					break;
				case 0x51:	// Page Down
					if (PLFileCount && /*! NextPLCmd &&*/ CurPLFile < PLFileCount - 0x01)
					{
						NextPLCmd = 0x00;
						QuitPlay = true;
					}
					PlaySmpl = 0;
					break;
				default:
					PlaySmpl = 0;
					break;
				}
				if (PlaySmpl)
				{
					SeekVGM(true, PlaySmpl * SampleRate);
					PosPrint = true;
				}
				break;
#ifdef WIN32
			case 0x1B:
				QuitPlay = true;
				NextPLCmd = 0xFF;
				break;
#endif
			case ' ':
				PauseVGM(! PausePlay);
				PosPrint = true;
				break;
			case 'F':	// Fading
				FadeTime = FadeTimeN;
				FadePlay = true;
				break;
			case 'R':	// Restart
				RestartVGM();
				PosPrint = true;
				break;
			case 'B':	// Previous file (Back)
				if (PLFileCount && /*! NextPLCmd &&*/ CurPLFile)
				{
					NextPLCmd = 0x01;
					QuitPlay = true;
				}
				break;
			case 'N':	// Next file
				if (PLFileCount && /*! NextPLCmd &&*/ CurPLFile < PLFileCount - 0x01)
				{
					NextPLCmd = 0x00;
					QuitPlay = true;
				}
				break;
			}
		}
		
		/*if (! PauseThread && FadePlay && (! FadeTime || MasterVol == 0.0f))
		{
			QuitPlay = true;
		}*/
		if (FadeRAWLog && IsRAWLog && ! PausePlay && ! FadePlay && FadeTimeN)
		{
			PlaySmpl = (INT32)VGMHead.lngTotalSamples -
						FadeTimeN * VGMSampleRate / 1500;
			if (VGMSmplPos >= PlaySmpl)
			{
				FadeTime = FadeTimeN;
				FadePlay = true;	// (FadeTime / 1500) ends at 33%
			}
		}
	}
	ThreadNoWait = false;
	
	// Last Uninit: ESC pressed, no playlist, last file in playlist
	LastUninit = (NextPLCmd & 0x80) || ! PLFileCount ||
				(NextPLCmd == 0x00 && CurPLFile >= PLFileCount - 0x01);
	switch(PlayingMode)
	{
	case 0x00:
		switch(LogToWave)
		{
		case 0x00:
		case 0x02:
			if (LastUninit)
			{
				StopStream();
			}
			else
			{
				if (ThreadPauseEnable)
				{
					ThreadPauseConfrm = false;
					PauseThread = true;
					while(! ThreadPauseConfrm)
						Sleep(1);	// Wait until the Thread is finished
				}
				else
				{
					PauseThread = true;
				}
			}
			break;
		case 0x01:
			SaveFile(0xFFFFFFFF, NULL);
			break;
		}
		break;
	case 0x01:
		if (StreamStarted)
			StopStream();
		break;
	case 0x02:
		if (LastUninit)
		{
			StopStream();
#ifdef MIXER_MUTING
#ifdef WIN32
			mixerClose(hmixer);
#else
			close(hmixer);
#endif
#endif
		}
		else
		{
			if (ThreadPauseEnable)
			{
				ThreadPauseConfrm = false;
				PauseThread = true;
				while(! ThreadPauseConfrm)
					Sleep(1);	// Wait until the Thread is finished
				PauseStream(true);
			}
			else
			{
				PauseThread = true;
			}
		}
		break;
	}
#ifndef WIN32
	changemode(false);
#endif
	
	StopVGM();
	
	printf("\nPlaying finished.\n");
	
	return;
}

static INT8 sign(double Value)
{
	if (Value > 0.0)
		return 1;
	else if (Value < 0.0)
		return -1;
	else
		return 0;
}

static long int Round(double Value)
{
	// Alternative:	(fabs(Value) + 0.5) * sign(Value);
	return (long int)(Value + 0.5 * sign(Value));
}

static double RoundSpecial(double Value, double RoundTo)
{
	return (long int)(Value / RoundTo + 0.5 * sign(Value)) * RoundTo;
}

static void PrintMinSec(UINT32 SamplePos, UINT32 SmplRate)
{
	float TimeSec;
	UINT16 TimeMin;
	UINT16 TimeHours;
	
	TimeSec = (float)RoundSpecial(SamplePos / (double)SmplRate, 0.01);
	//TimeSec = SamplePos / (float)SmplRate;
	TimeMin = (UINT16)TimeSec / 60;
	TimeSec -= TimeMin * 60;
	if (! PrintMSHours)
	{
		printf("%02hu:%05.2f", TimeMin, TimeSec);
	}
	else
	{
		TimeHours = TimeMin / 60;
		TimeMin %= 60;
		printf("%hu:%02hu:%05.2f", TimeHours, TimeMin, TimeSec);
	}
	
	return;
}

INLINE INT32 SampleVGM2Playback(INT32 SampleVal)
{
	return (INT32)((INT64)SampleVal * SampleRate / VGMSampleRate);
}

INLINE INT32 SamplePlayback2VGM(INT32 SampleVal)
{
	return (INT32)((INT64)SampleVal * VGMSampleRate / SampleRate);
}
