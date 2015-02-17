// This file is part of the VGMPlay program 
// Licensed under the GNU General Public License version 2 (or later)

#ifdef XMAS_EXTRA
#include "XMasFiles/XMasBonus.h"
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

#ifndef SHARE_PREFIX
#define SHARE_PREFIX	"/usr/local"
#endif

#endif

#define APP_NAME	"VGM Player"
#define APP_NAME_L	L"VGM Player"

#define LOG_SAMPLES	(SampleRate / 5)

static void RemoveNewLines(char* String);
static void RemoveQuotationMarks(char* String);
static char* GetLastDirSeparator(const char* FilePath);
static bool IsAbsolutePath(const char* FilePath);
static char* GetFileExtention(const char* FilePath);
static void StandardizeDirSeparators(char* FilePath);
#ifdef WIN32
static void WinNT_Check(void);
#endif
static char* GetAppFileName(void);
static void cls(void);
#ifndef WIN32
static void changemode(bool);
static int _kbhit(void);
static int _getch(void);
#endif
static INT8 stricmp_u(const char *string1, const char *string2);
static INT8 strnicmp_u(const char *string1, const char *string2, size_t count);
static void ReadOptions(const char* AppName);
static bool GetBoolFromStr(const char* TextStr);
#if defined(XMAS_EXTRA)
static bool XMas_Extra(char* FileName, bool Mode);
#endif
#ifndef WIN32
static void ConvertCP1252toUTF8(char** DstStr, const char* SrcStr);
#endif
static bool OpenPlayListFile(const char* FileName);
static bool OpenMusicFile(const char* FileName);
extern bool OpenVGMFile(const char* FileName);
extern bool OpenOtherFile(const char* FileName);

//#ifdef WIN32
//static void printc(const char* format, ...);
//#else
#define	printc	printf
//#endif
static void wprintc(const wchar_t* format, ...);
static void PrintChipStr(UINT8 ChipID, UINT8 SubType, UINT32 Clock);
static const wchar_t* GetTagStrEJ(const wchar_t* EngTag, const wchar_t* JapTag);
static void ShowVGMTag(void);

static void PlayVGM_UI(void);

