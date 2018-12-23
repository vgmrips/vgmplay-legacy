/*3456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
0000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999*/
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#include "Winamp/wa_ipc.h"
#include "Winamp/in2.h"

#include "stdbool.h"
#include "chips/mamedef.h"
#include "VGMPlay.h"
#include "VGMPlay_Intf.h"
#include "in_vgm.h"

#define DllExport	__declspec(dllexport)
#define SETCHECKBOX(hWnd, DlgID, Check)	SendDlgItemMessage(hWnd, DlgID, BM_SETCHECK, Check, 0)


typedef struct fileinfo_data
{
	UINT32 FileNameAlloc;
	wchar_t* FileName;
	bool ForceReload;
	
	FILETIME FileTime;
	UINT32 FileSize;
	VGM_HEADER Head;
	GD3_TAG Tag;
	
	UINT32 TrackLen;	// length without loops
	UINT32 TotalLen;	// including loops
	UINT32 LoopLen;
	UINT32 DataSize;
	float BitRate;
	float VolGain;
} FINF_DATA;


// Function Prototypes from in_vgm.c
void Config(HWND hWndParent);


// Function Prototypes
void SetInfoDlgFile(const char* FileName);
void SetInfoDlgFileW(const wchar_t* FileName);
UINT32 GetVGZFileSize(const char* FileName);
UINT32 GetVGZFileSizeW(const wchar_t* FileName);
static void CopyWStr(wchar_t** DstStr, const wchar_t* SrcStr);
static void CopyTagData(GD3_TAG* DstTag, const GD3_TAG* SrcTag);
static bool LoadInfoA(const char* FileName, FINF_DATA* FileInf);
static bool LoadInfoW(const wchar_t* FileName, FINF_DATA* FileInf);
static void FixNewLine(wchar_t** TextData);
static void FixSeparators(wchar_t** TextData);
static void TrimWhitespaces(wchar_t* TextData);

static bool CheckFM2413Text(VGM_HEADER* FileHead);
UINT32 FormatVGMTag(const UINT32 BufLen, in_char* Buffer, GD3_TAG* FileTag, VGM_HEADER* FileHead);

static void AppendToStr(char* Buffer, const char* AppendStr, UINT8 Seperator);
static void MakeChipStr(char* Buffer, UINT8 ChipID, UINT8 SubType, UINT32 Clock);
static void PrintTime(char* Buffer, UINT32 MSecTime);
static void GetChipUsageText(char* Buffer, VGM_HEADER* FileHead);
void FormatVGMLength(char* Buffer, FINF_DATA* FileInf);
void PrintTime(char* Buffer, UINT32 MSecTime);
//int GetTrackNumber(const char *filename);
bool LoadPlayingVGMInfo(const char* FileName);
bool LoadPlayingVGMInfoW(const wchar_t* FileName);
void QueueInfoReload(void);

bool GetExtendedFileInfoW(const wchar_t* FileName, const char* MetaType, wchar_t* RetBuffer,
						  int RetBufLen);
const wchar_t* GetTagStringEngJap(const wchar_t* TextEng, const wchar_t* TextJap,
									bool LangMode);
void DisplayTagString(HWND hWndDlg, int DlgItem, const wchar_t* TextEng,
					  const wchar_t* TextJap, bool LangMode);
BOOL CALLBACK FileInfoDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
DllExport int winampGetExtendedFileInfoW(const wchar_t* wfilename, const char* metadata,
										 wchar_t* ret, int retlen);
DllExport int winampGetExtendedFileInfo(const char* filename, const char* metadata, char* ret,
										int retlen);


#define METATAG_AUTHOR	0x01
#define METATAG_LENGTH	0x02
#define METATAG_TITLE	0x03
#define METATAG_ALBUM	0x04
#define METATAG_COMMENT	0x05
#define METATAG_YEAR	0x06
#define METATAG_GENRE	0x07
#define METATAG_UNKNOWN	0xFF


extern VGM_HEADER VGMHead;
extern GD3_TAG VGMTag;


extern UINT32 VGMMaxLoop;
extern UINT32 FadeTime;

static FINF_DATA VGMInfo;
static FINF_DATA PlayVGMInfo;
static const char* FileNameToLoad;
static const wchar_t* FileNameToLoadW;


void SetInfoDlgFile(const char* FileName)
{
	FileNameToLoadW = NULL;
	FileNameToLoad = FileName;
	
	return;
}

void SetInfoDlgFileW(const wchar_t* FileName)
{
	FileNameToLoad = NULL;
	FileNameToLoadW = FileName;
	
	return;
}

UINT32 GetVGZFileSize(const char* FileName)
{
	// returns the size of a compressed VGZ file
	// or 0 it the file is uncompressed or not found
	FILE* hFile;
	UINT32 FileSize;
	UINT16 gzHead;
	
	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
		return 0x00;
	
	fread(&gzHead, 0x02, 0x01, hFile);
	if (gzHead == 0x8B1F)
	{
		// .gz File
		fseek(hFile, 0x00, SEEK_END);
		FileSize = ftell(hFile);
	}
	else
	{
		FileSize = 0x00;
	}
	
	fclose(hFile);
	
	return FileSize;
}

UINT32 GetVGZFileSizeW(const wchar_t* FileName)
{
	// returns the size of a compressed VGZ file
	// or 0 it the file is uncompressed or not found
	FILE* hFile;
	UINT32 FileSize;
	UINT16 gzHead;
	
	hFile = _wfopen(FileName, L"rb");
	if (hFile == NULL)
		return 0x00;
	
	fread(&gzHead, 0x02, 0x01, hFile);
	if (gzHead == 0x8B1F)
	{
		// .gz File
		fseek(hFile, 0x00, SEEK_END);
		FileSize = ftell(hFile);
	}
	else
	{
		FileSize = 0x00;
	}
	
	fclose(hFile);
	
	return FileSize;
}

static void CopyWStr(wchar_t** DstStr, const wchar_t* SrcStr)
{
	size_t StrLen;
	
	if (SrcStr == NULL)
	{
		*DstStr = NULL;
		return;
	}
	
	StrLen = wcslen(SrcStr) + 0x01;
	*DstStr = (wchar_t*)malloc(StrLen * sizeof(wchar_t));
	wcscpy(*DstStr, SrcStr);
	
	return;
}

static void CopyTagData(GD3_TAG* DstTag, const GD3_TAG* SrcTag)
{
	DstTag->fccGD3 = SrcTag->fccGD3;
	DstTag->lngVersion = SrcTag->lngVersion;
	DstTag->lngTagLength = SrcTag->lngTagLength;
	CopyWStr(&DstTag->strTrackNameE,	SrcTag->strTrackNameE);
	CopyWStr(&DstTag->strTrackNameJ,	SrcTag->strTrackNameJ);
	CopyWStr(&DstTag->strGameNameE,		SrcTag->strGameNameE);
	CopyWStr(&DstTag->strGameNameJ,		SrcTag->strGameNameJ);
	CopyWStr(&DstTag->strSystemNameE,	SrcTag->strSystemNameE);
	CopyWStr(&DstTag->strSystemNameJ,	SrcTag->strSystemNameJ);
	CopyWStr(&DstTag->strAuthorNameE,	SrcTag->strAuthorNameE);
	CopyWStr(&DstTag->strAuthorNameJ,	SrcTag->strAuthorNameJ);
	CopyWStr(&DstTag->strReleaseDate,	SrcTag->strReleaseDate);
	CopyWStr(&DstTag->strCreator,		SrcTag->strCreator);
	CopyWStr(&DstTag->strNotes,			SrcTag->strNotes);
	
	return;
}

static bool LoadInfoA(const char* FileName, FINF_DATA* FileInf)
{
	size_t FNSize;
	wchar_t* FileNameW;
	bool RetVal;
	
	FNSize = mbstowcs(NULL, FileName, 0);
	if (FNSize == -1)
		return false;
	FNSize ++;
	
	FileNameW = (wchar_t*)malloc(FNSize * sizeof(wchar_t));
	mbstowcs(FileNameW, FileName, FNSize);
	
	RetVal = LoadInfoW(FileNameW, FileInf);
	
	free(FileNameW);
	return RetVal;
}

static bool LoadInfoW(const wchar_t* FileName, FINF_DATA* FileInf)
{
	HANDLE hFile;
	FILETIME fileWrtTime;
	VGM_HEADER* FH;
	UINT32 StrSize;
	INT32 TempSLng;
	
	hFile = CreateFileW(FileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		GetFileTime(hFile, NULL, NULL, &fileWrtTime);
		CloseHandle(hFile);	hFile = NULL;
	}
	else
	{
		fileWrtTime.dwLowDateTime = 0x00;
		fileWrtTime.dwHighDateTime = 0x00;
	}
	
	if (! FileInf->ForceReload && ! Options.NoInfoCache)
	{
		if (FileInf->FileName != NULL && ! _wcsicmp(FileInf->FileName, FileName))
		{
			// We just loaded that file.
			if (CompareFileTime(&fileWrtTime, &FileInf->FileTime) == 0)
				return true;	// The file wasn't changed, so don't reload.
		}
	}
	FileInf->ForceReload = false;
	
	StrSize = wcslen(FileName) + 1;
	if (FileInf->FileNameAlloc < StrSize)
	{
		FileInf->FileName = (wchar_t*)realloc(FileInf->FileName, StrSize * sizeof(wchar_t));
		FileInf->FileNameAlloc = StrSize;
	}
	wcscpy(FileInf->FileName, FileName);
	
	FreeGD3Tag(&FileInf->Tag);
	
	if (FileInf != &PlayVGMInfo)
	{
		if (PlayVGMInfo.FileName != NULL && ! _wcsicmp(PlayVGMInfo.FileName, FileName))
		{
			if (PlayVGMInfo.ForceReload || Options.NoInfoCache)
				LoadInfoW(PlayVGMInfo.FileName, &PlayVGMInfo);
			
			// copy all info from PlayVGMInfo to current structure
			// (advanced caching) ;)
			FileInf->FileTime = PlayVGMInfo.FileTime;
			FileInf->FileSize = PlayVGMInfo.FileSize;
			FileInf->Head = PlayVGMInfo.Head;
			CopyTagData(&FileInf->Tag, &PlayVGMInfo.Tag);
			FileInf->TrackLen = PlayVGMInfo.TrackLen;
			FileInf->TotalLen = PlayVGMInfo.TotalLen;
			FileInf->LoopLen = PlayVGMInfo.LoopLen;
			FileInf->DataSize = PlayVGMInfo.DataSize;
			FileInf->BitRate = PlayVGMInfo.BitRate;
			FileInf->VolGain = PlayVGMInfo.VolGain;
			
			return true;
		}
		
		FileInf->FileSize = GetVGMFileInfoW(FileInf->FileName, &FileInf->Head, &FileInf->Tag);
	}
	else
	{
		FileInf->FileSize = VGMHead.lngEOFOffset;
		FileInf->Head = VGMHead;
		CopyTagData(&FileInf->Tag, &VGMTag);
	}
	
	if (! FileInf->FileSize)
	{
		FileInf->FileTime.dwLowDateTime = 0x00;
		FileInf->FileTime.dwHighDateTime = 0x00;
		FileInf->FileSize = 0x00;
		memset(&FileInf->Head, 0x00, sizeof(VGM_HEADER));
		memset(&FileInf->Tag, 0x00, sizeof(GD3_TAG));
		
		FileInf->TrackLen = 0x00;
		FileInf->TotalLen = 0x00;
		FileInf->LoopLen = 0x00;
		FileInf->DataSize = 0x00;
		FileInf->BitRate = 0.0f;
		FileInf->VolGain = 0.0f;
		
		FileInf->Tag.strNotes = (wchar_t*)malloc(16 * sizeof(wchar_t));
		if (GetGZFileLengthW(FileName) == 0xFFFFFFFF)
			wcscpy(FileInf->Tag.strNotes, L"File not found!");
		else
			wcscpy(FileInf->Tag.strNotes, L"File invalid!");
		return false;
	}
	FileInf->FileTime = fileWrtTime;
	
	FH = &FileInf->Head;
	FileInf->TrackLen = CalcSampleMSecExt(FH->lngTotalSamples, 0x02, FH);
	FileInf->LoopLen = CalcSampleMSecExt(FH->lngLoopSamples, 0x02, FH);
	if (! FH->lngLoopSamples)
	{
		FileInf->TotalLen = CalcSampleMSecExt(FH->lngTotalSamples, 0x02, FH) +
							Options.PauseNL;
	}
	else
	{
		if (! VGMMaxLoop)	// infinite looping
		{
			FileInf->TotalLen = -1000;
		}
		else
		{
			TempSLng = (VGMMaxLoop * FH->bytLoopModifier + 0x08) / 0x10 - FH->bytLoopBase;
			if (TempSLng < 0x01)
				TempSLng = 0x01;
			FileInf->TotalLen = CalcSampleMSecExt(FH->lngTotalSamples + (INT64)(TempSLng - 1) *
								FH->lngLoopSamples, 0x02, FH) + FadeTime + Options.PauseLp;
		}
	}
	
	StrSize = GetVGZFileSizeW(FileInf->FileName);
	if (! StrSize)
	{
		if (FH->lngGD3Offset)
			FileInf->DataSize = FH->lngGD3Offset - FH->lngDataOffset;
		else
			FileInf->DataSize = FH->lngEOFOffset - FH->lngDataOffset;
	}
	else
	{
		FileInf->FileSize = StrSize;
		FileInf->DataSize = StrSize;
	}
	
	// 1 Bytes = 8 Bits
	// 1 sec = 1000 ms
	// -> (bytes * 8) / (ms / 1000) = bytes * 8000 / ms
	FileInf->BitRate = (INT64)FileInf->DataSize * 8000 / (float)FileInf->TrackLen;
	
	if (FH->bytVolumeModifier <= VOLUME_MODIF_WRAP)
		TempSLng = FH->bytVolumeModifier;
	else if (FH->bytVolumeModifier == (VOLUME_MODIF_WRAP + 0x01))
		TempSLng = VOLUME_MODIF_WRAP - 0x100;
	else
		TempSLng = FH->bytVolumeModifier - 0x100;
	FileInf->VolGain = (float)pow(2.0, TempSLng / (double)0x20);
	
	if (Options.TrimWhitespc)
	{
		TrimWhitespaces(FileInf->Tag.strTrackNameE);
		TrimWhitespaces(FileInf->Tag.strTrackNameJ);
		TrimWhitespaces(FileInf->Tag.strGameNameE);
		TrimWhitespaces(FileInf->Tag.strGameNameJ);
		TrimWhitespaces(FileInf->Tag.strSystemNameE);
		TrimWhitespaces(FileInf->Tag.strSystemNameJ);
		TrimWhitespaces(FileInf->Tag.strAuthorNameE);
		TrimWhitespaces(FileInf->Tag.strAuthorNameJ);
		TrimWhitespaces(FileInf->Tag.strReleaseDate);
		TrimWhitespaces(FileInf->Tag.strCreator);
		TrimWhitespaces(FileInf->Tag.strNotes);
	}
	if (Options.StdSeparators)
	{
		FixSeparators(&FileInf->Tag.strAuthorNameE);
		FixSeparators(&FileInf->Tag.strAuthorNameJ);
	}
	
	if (! FileInf->Tag.lngVersion)
	{
		FileInf->Tag.strNotes = (wchar_t*)malloc(22 * sizeof(wchar_t));
		wcscpy(FileInf->Tag.strNotes, L"No GD3 Tag available.");
	}
	else
	{
		FixNewLine(&FileInf->Tag.strNotes);
	}
	
	return true;
}

static void FixNewLine(wchar_t** TextData)
{
	// Note: Reallocates memory for final string
	wchar_t* TempStr;
	wchar_t* SrcStr;
	wchar_t* DstStr;
	UINT32 StrSize;
	
	if (TextData == NULL || *TextData == NULL || ! wcslen(*TextData))
		return;
	
	// fix Notes-Tag (CR -> CRLF)
	SrcStr = *TextData;
	StrSize = 0x00;
	while(*SrcStr != L'\0')
	{
		if (*SrcStr == L'\n')
			StrSize ++;	// count CR characters twice (for additional LF character)
		SrcStr ++;
		StrSize ++;
	}
	StrSize ++;	// final \0 character
	TempStr = (wchar_t*)malloc(StrSize * sizeof(wchar_t));
	
	SrcStr = *TextData;
	DstStr = TempStr;
	while(*SrcStr != L'\0')
	{
		if (*SrcStr == L'\n')
		{
			*DstStr = L'\r';
			DstStr ++;
		}
		*DstStr = *SrcStr;
		SrcStr ++;
		DstStr ++;
	}
	*DstStr = L'\0';
	free(*TextData);
	*TextData = TempStr;
	
	return;
}

#define GOODSEP_CHR		L','

static void FixSeparators(wchar_t** TextData)
{
	// Note: Reallocates memory for final string
	const wchar_t BADSEPS[] =
	{	L';', L'/', L'\\', L'&', L'\xFF0C', L'\xFF0F', L'\xFF3C', 0x0000};
	
	wchar_t* TempStr;
	wchar_t* SrcStr;
	wchar_t* DstStr;
	const wchar_t* ChkStr;
	UINT32 StrSize;
	UINT32 SpcWrt;
	bool WroteSep;
	
	if (TextData == NULL || *TextData == NULL || ! wcslen(*TextData))
		return;
	
	// fix Author-Tag (a;b;c -> a, b, c)
	SrcStr = *TextData;
	StrSize = 0x00;
	WroteSep = false;
	while(*SrcStr != L'\0')
	{
		if (WroteSep && ! (*(SrcStr + 1) == L' ' || *(SrcStr + 1) == 0x3000))
			StrSize ++;	// need additional Space character
		
		ChkStr = BADSEPS;
		while(*ChkStr != 0x0000)
		{
			if (*SrcStr == *ChkStr)
			{
				// replace bad with good chars
				*SrcStr = GOODSEP_CHR;
				break;
			}
			ChkStr ++;
		}
		if (*SrcStr == GOODSEP_CHR)
			WroteSep = true;
		SrcStr ++;
		StrSize ++;
	}
	StrSize ++;	// final \0 character
	TempStr = (wchar_t*)malloc(StrSize * sizeof(wchar_t));
	
	SrcStr = *TextData;
	DstStr = TempStr;
	SpcWrt = 0;
	WroteSep = false;
	while(*SrcStr != L'\0')
	{
		if (*SrcStr == GOODSEP_CHR)
		{
			WroteSep = true;
			// trim spaces left of the seperator
			DstStr -= SpcWrt;
			SpcWrt = 0;
		}
		else
		{
			if (*SrcStr == L' ' || *SrcStr == 0x3000)
				SpcWrt ++;
			else
				SpcWrt = 0;
			if (WroteSep && ! SpcWrt)
			{
				// insert space after the seperator
				*DstStr = L' ';
				DstStr ++;
				SpcWrt ++;
			}
			WroteSep = false;
		}
		*DstStr = *SrcStr;
		SrcStr ++;
		DstStr ++;
	}
	*DstStr = L'\0';
	free(*TextData);
	*TextData = TempStr;
	
	return;
}

static void TrimWhitespaces(wchar_t* TextData)
{
	// Note: Writes to argument string
	wchar_t* CurStr;
	
	if (TextData == NULL || ! wcslen(TextData))
		return;
	
	// trim whitespace at end
	CurStr = TextData + wcslen(TextData);
	CurStr --;
	while(iswspace(*CurStr))
		CurStr --;
	CurStr ++;
	*CurStr = L'\0';
	
	// trim whitespace at start
	CurStr = TextData;
	while(iswspace(*CurStr))
		CurStr ++;
	wcscpy(TextData, CurStr);
	
	return;
}


static bool CheckFM2413Text(VGM_HEADER* FileHead)
{
	UINT8 CurChip;
	UINT32 Clocks;
	
	if (! Options.AppendFM2413 || FileHead == NULL || ! GetChipClock(FileHead, 0x01, NULL))
		return false;
	
	Clocks = 0x00;
	for (CurChip = 0x02; CurChip < CHIP_COUNT; CurChip ++)
		Clocks |= GetChipClock(FileHead, CurChip, NULL);
	
	return (! Clocks);
}

UINT32 FormatVGMTag(const UINT32 BufLen, in_char* Buffer, GD3_TAG* FileTag, VGM_HEADER* FileHead)
{
	const in_char* BufEnd;
	in_char* CurBuf;
	const char* FormatStr;
	const wchar_t* TagStr;
	const wchar_t* TagStrE;
	const wchar_t* TagStrJ;
	bool TempFlag;
	bool AppendFM;
	size_t CnvBytes;
	
	BufEnd = Buffer + BufLen - 1;
	CurBuf = Buffer;
	FormatStr = Options.TitleFormat;
	
	while(CurBuf < BufEnd && *FormatStr != '\0')
	{
		if (*FormatStr == '%')
		{
			FormatStr ++;
			TempFlag = false;	// Is Unknown Char
			AppendFM = false;
			switch(toupper(*FormatStr))
			{
			case 'T':	// Track Title
				TagStrE = FileTag->strTrackNameE;
				TagStrJ = FileTag->strTrackNameJ;
				break;
			case 'A':	// Author
				TagStrE = FileTag->strAuthorNameE;
				TagStrJ = FileTag->strAuthorNameJ;
				break;
			case 'G':	// Game Name
				TagStrE = FileTag->strGameNameE;
				TagStrJ = FileTag->strGameNameJ;
				AppendFM = CheckFM2413Text(FileHead);
				break;
			case 'D':	// Release Date
				TagStrE = FileTag->strReleaseDate;
				TagStrJ = NULL;
				break;
			case 'S':	// System Name
				TagStrE = FileTag->strSystemNameE;
				TagStrJ = FileTag->strSystemNameJ;
				break;
			case 'C':	// File Creator
				TagStrE = FileTag->strCreator;
				TagStrJ = NULL;
				break;
			default:
				TempFlag = true;
				break;
			}
			if (TempFlag)
			{
#ifndef UNICODE_INPUT_PLUGIN
				*CurBuf = *FormatStr;
				FormatStr ++;	CurBuf ++;
#else
				CnvBytes = MultiByteToWideChar(CP_THREAD_ACP, 0x00, FormatStr, 1, CurBuf,
												BufEnd - CurBuf);
				FormatStr ++;	CurBuf += CnvBytes;
#endif
			}
			else
			{
				FormatStr ++;
				if (toupper(*FormatStr) == 'J')
				{
					TempFlag = true;
					FormatStr ++;
				}
				TagStr = GetTagStringEngJap(TagStrE, TagStrJ, TempFlag);
				if (TagStr == NULL)
					TagStr = L"";
#ifndef UNICODE_INPUT_PLUGIN
				CnvBytes = WideCharToMultiByte(CP_THREAD_ACP, 0x00, TagStr, -1, CurBuf,
												BufEnd - CurBuf, NULL, NULL);
				if (CnvBytes)
					CurBuf += CnvBytes - 1;	// It counts the \0 ternimator, too.
				
				// The problem with the ANSI C function is, that it simply stops
				// converting the string, instead of inserting '?', which would be nicer,
				// at least in this case.
				/*TagStr = GetTagStringEngJap(TagStrE, TagStrJ, TempFlag);
				CnvBytes = wcstombs(CurBuf, TagStr, BufEnd - CurBuf);
				if (CnvBytes == -1)
				{
					TagStr = GetTagStringEngJap(TagStrE, TagStrJ, ! TempFlag);
					CnvBytes = wcstombs(CurBuf, TagStr, BufEnd - CurBuf);
				}
				if (CnvBytes != -1)
					CurBuf += CnvBytes - 1;*/
				
				if (AppendFM && (BufEnd - CurBuf) >= 0x05+1)
				{
					strcpy(CurBuf, " (FM)");
					CurBuf += 0x05;
				}
#else
				CnvBytes = wcslen(TagStr);
				if (CnvBytes < BufEnd - CurBuf)
				{
					wcscpy(CurBuf, TagStr);
					CurBuf += CnvBytes;
				}
				else
				{
					CnvBytes = (BufEnd - CurBuf) - 1;
					wcsncpy(CurBuf, TagStr, CnvBytes);
					CurBuf += CnvBytes;
				}
				if (AppendFM && (BufEnd - CurBuf) >= 0x05+1)
				{
					wcscpy(CurBuf, L" (FM)");
					CurBuf += 0x05;
				}
#endif
			}
		}
		else
		{
#ifndef UNICODE_INPUT_PLUGIN
			*CurBuf = *FormatStr;
			FormatStr ++;	CurBuf ++;
#else
			CnvBytes = MultiByteToWideChar(CP_THREAD_ACP, 0x00, FormatStr, 1, CurBuf,
											BufEnd - CurBuf);
			FormatStr ++;	CurBuf += CnvBytes;
#endif
		}
	}
#ifndef UNICODE_INPUT_PLUGIN
	*CurBuf = '\0';
#else
	*CurBuf = L'\0';
#endif
	
	return Buffer - CurBuf;
}


static void AppendToStr(char* Buffer, const char* AppendStr, UINT8 Seperator)
{
	// Seperator Mask:
	//	01 - Space
	//	02 - Comma
	char* BufPnt;
	
	if (AppendStr == NULL)
		return;
	
	if (*Buffer == '\0')
	{
		strcpy(Buffer, AppendStr);
	}
	else
	{
		BufPnt = Buffer + strlen(Buffer);
		if (Seperator & 0x02)
		{
			*BufPnt = ',';
			BufPnt ++;
		}
		if (Seperator & 0x01)
		{
			*BufPnt = ' ';
			BufPnt ++;
		}
		strcpy(BufPnt, AppendStr);
	}
	
	return;
}

static void MakeChipStr(char* Buffer, UINT8 ChipID, UINT8 SubType, UINT32 Clock)
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
	
	if (! (Clock & 0x40000000))
	{
		AppendToStr(Buffer, GetAccurateChipName(ChipID, SubType), 0x03);
	}
	else
	{
		AppendToStr(Buffer, "2x", 0x03);
		AppendToStr(Buffer, GetAccurateChipName(ChipID, SubType), 0x00);
	}
	
	return;
}

static void GetChipUsageText(char* Buffer, VGM_HEADER* FileHead)
{
	UINT8 CurChip;
	UINT32 ChpClk;
	UINT8 ChpType;
	
	*Buffer = '\0';
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
	{
		ChpClk = GetChipClock(FileHead, CurChip, &ChpType);
		if (ChpClk && GetChipClock(FileHead, 0x80 | CurChip, NULL))
			ChpClk |= 0x40000000;
		MakeChipStr(Buffer, CurChip, ChpType, ChpClk);
	}
	
	return;
}

static void PrintTime(char* Buffer, UINT32 MSecTime)
{
	UINT16 MSecs;
	UINT16 Secs;
	UINT16 Mins;
	UINT16 Hours;
	UINT32 Days;
	
	MSecs = MSecTime % 1000;
	MSecTime /= 1000;
	Secs = MSecTime % 60;
	MSecTime /= 60;
	Mins = MSecTime % 60;
	MSecTime /= 60;
	Hours = MSecTime % 24;
	MSecTime /= 24;
	Days = MSecTime;
	
	// Maximum Output: 4 294 967 295 -> "49d + 17:02:47.29" (17 chars + \0)
	if (Days)
	{
		// just for fun
		//				 1d +  2 :  34 :  56 . 78
		sprintf(Buffer, "%ud + %hu:%02hu:%02hu.%02hu", Days, Hours, Mins, Secs, MSecs / 10);
	}
	else if (Hours)
	{
		// unlikely for single VGMs, but possible
		//				  1 :  23 :  45 . 67
		sprintf(Buffer, "%hu:%02hu:%02hu.%02hu", Hours, Mins, Secs, MSecs / 10);
	}
	else if (Mins)
	{
		//				  1 :  23 . 45
		sprintf(Buffer, "%hu:%02hu.%02hu", Mins, Secs, MSecs / 10);
	}
	else
	{
		//				  1 . 23
		sprintf(Buffer, "%hu.%02hu", Secs, MSecs / 10);
	}

	return;
}

void FormatVGMLength(char* Buffer, FINF_DATA* FileInf)
{
	UINT32 IntroLen;
	char TempBuf1[0x20];
	char TempBuf2[0x20];

	PrintTime(Buffer, FileInf->TrackLen);
	if (! FileInf->LoopLen)
	{
		strcat(Buffer, " (no loop)");
	}
	else
	{
		IntroLen = FileInf->TrackLen - FileInf->LoopLen;
		if (IntroLen < 500)
		{
			// no intro (or only a very small intro for song init)
			strcat(Buffer, " (looped)");
		}
		else
		{
			PrintTime(TempBuf1, IntroLen);
			PrintTime(TempBuf2, FileInf->LoopLen);
			sprintf(Buffer, "%s (%s intro and %s loop)", Buffer, TempBuf1, TempBuf2);
		}
	}

	return;
}

//-----------------------------------------------------------------
// Get the track number for a given filename
// by trying to find its M3U playlist
//-----------------------------------------------------------------
/*int GetTrackNumber(const char *filename)
{
	return 0;
	
	// assumes a filename "Streets of Rage II - Never Return Alive.vgz"
	// will have a playlist "Streets of Rage II.m3u"
	// returns track number, or 0 for not found
	FILE *f;
	char *playlist;
	char *p;
	char *fn;
	char buff[1024]; // buffer for reading lines from file
	int number=0;
	int linenumber;

	playlist=malloc(strlen(filename)+16); // plenty of space in all weird cases

	if(playlist) {
		p=strrchr(filename,'\\'); // p points to the last \ in the filename
		if(p){
			// isolate filename
			fn=malloc(strlen(p));
			if(fn) {
				strcpy(fn,p+1);

				while(number==0) {
					p=strstr(p," - "); // find first " - " after current position of p
					if(p) {
						strncpy(playlist,filename,p-filename); // copy filename up until the " - "
						strcpy(playlist+(p-filename),".m3u"); // terminate it with a ".m3u\0"

						f=fopen(playlist,"r"); // try to open file
						if(f) {
							linenumber=1;
							// read through file, a line at a time
							while(fgets(buff,1024,f) && (number==0)) {		// fgets will read in all characters up to and including the line break
								if(strnicmp(buff,fn,strlen(fn))==0) {
									// found it!
									number=linenumber;
								}
								if((strlen(buff)>3)&&(buff[0]!='#')) linenumber++; // count lines that are (probably) valid but not #EXTINF lines
							}

							fclose(f);
						}
						p++; // make it not find this " - " next iteration
					} else break;
				}
				free(fn);
			}
		}
		free(playlist);
	}
	return number;
}*/

bool LoadPlayingVGMInfo(const char* FileName)
{
	size_t FNSize;
	wchar_t* FileNameW;
	bool RetVal;
	
	if (FileName == NULL)
	{
		RetVal = LoadPlayingVGMInfoW(NULL);
	}
	else
	{
		FNSize = mbstowcs(NULL, FileName, 0);
		if (FNSize == -1)
			return LoadPlayingVGMInfoW(NULL);
		FNSize ++;
		
		FileNameW = (wchar_t*)malloc(FNSize * sizeof(wchar_t));
		mbstowcs(FileNameW, FileName, FNSize);
		
		RetVal = LoadPlayingVGMInfoW(FileNameW);
		
		free(FileNameW);
	}
	
	return true;
}

bool LoadPlayingVGMInfoW(const wchar_t* FileName)
{
	bool RetVal;
	
	if (FileName == NULL && PlayVGMInfo.FileName != NULL)
	{
		PlayVGMInfo.FileName[0x00] = L'\0';
		return true;
	}
	
	// load new file only if neccessary
	if (PlayVGMInfo.FileName == NULL || _wcsicmp(PlayVGMInfo.FileName, FileName))
		RetVal = LoadInfoW(FileName, &PlayVGMInfo);
	else
		RetVal = true;
	
	if (! PlayVGMInfo.FileSize)
		return false;
	
	return true;
}

void QueueInfoReload(void)
{
	PlayVGMInfo.ForceReload = true;
	VGMInfo.ForceReload = true;
	
	return;
}


// GetExtendedFileInfoW worker function
bool GetExtendedFileInfoW(const wchar_t* FileName, const char* MetaType, wchar_t* RetBuffer,
						  int RetBufLen)
{
	/* Metadata Tag List:
		"track", "title", "artist", "album", "year", "comment", "genre", "length",
		"type", "family", "formatinformation", "gain", "bitrate", "vbr", "stereo", ...
	*/
	int TagIdx;
	FINF_DATA* UseInfo;
	GD3_TAG* FileTag;
	
	if (RetBuffer == NULL || ! RetBufLen)
		return false;
	
	// default to a blank string
	*RetBuffer = L'\0';
	
	// General Meta Tags
	if (! stricmp(MetaType, "type"))
	{
		// Data Type (Audio/Video/...)
		_snwprintf(RetBuffer, RetBufLen, L"%d", Options.MLFileType);
		return true;
	}
	else if (! stricmp(MetaType, "family"))	// Winamp 5.5+ only
	{
		const wchar_t* FileExt;
		
		FileExt = wcsrchr(FileName, L'.');
		if (FileExt != NULL)
		{
			FileExt ++;
			if (! wcsicmp(FileExt, L"vgm") || ! wcsicmp(FileExt, L"vgz"))
			{
				wcsncpy(RetBuffer, L"Video Game Music File", RetBufLen);
				return true;
			}	
		}
		return false;
	}
	else if (! stricmp(MetaType, "track"))
	{
		return false;
		/*// Track Number
		int TrackNo;
		
		TrackNo = GetTrackNumber(FileName);
		if (TrackNo)
		{
			_snwprintf(RetBuffer, RetBufLen, L"%d", TrackNo);
			return true;
		}
		else
		{
			return false;
		}*/
	}
	
	if (PlayVGMInfo.FileName != NULL && ! _wcsicmp(PlayVGMInfo.FileName, FileName))
	{
		UseInfo = &PlayVGMInfo;
	}
	else
	{
		// load new file only if neccessary
		if (VGMInfo.FileName == NULL || _wcsicmp(VGMInfo.FileName, FileName))
		{
			if (! LoadInfoW(FileName, &VGMInfo))
				return false;	// Loading failed
		}
		UseInfo = &VGMInfo;
	}
	if (! UseInfo->FileSize)
		return false;
	
	if (! stricmp(MetaType, "artist"))
		TagIdx = METATAG_AUTHOR;
	else if (! stricmp(MetaType, "length"))
		TagIdx = METATAG_LENGTH;
	else if (! stricmp(MetaType, "title"))
		TagIdx = METATAG_TITLE;
	else if (! stricmp(MetaType, "album"))
		TagIdx = METATAG_ALBUM;	// return Game Name
	else if (! stricmp(MetaType, "comment"))
		TagIdx = METATAG_COMMENT;
	else if (! stricmp(MetaType, "year") || ! stricmp(MetaType, "date"))
		TagIdx = METATAG_YEAR;
	else if (! stricmp(MetaType, "genre"))
		TagIdx = METATAG_GENRE;	// return System (?)
	else
	{
		TagIdx = METATAG_UNKNOWN;
#ifdef _DEBUG
		{
			char DebugStr[0x80];
			
			// debug: get metadata types
			sprintf(DebugStr, "GetExtendedFileInfo: Unknown metadata type: \"%s\"\n", MetaType);
			MessageBox(NULL, DebugStr, "in_vgm Warning", MB_ICONEXCLAMATION);
		}
#endif
	}
		
	switch(TagIdx)
	{
	case METATAG_LENGTH:
		_snwprintf(RetBuffer, RetBufLen, L"%d", UseInfo->TotalLen);
		break;
	/*case METATAG_GENRE:
		wcsncpy(RetBuffer, L"Game", RetBufLen);
		break;*/
	case METATAG_TITLE:
	case METATAG_ALBUM:
	case METATAG_GENRE:
	case METATAG_AUTHOR:
	case METATAG_YEAR:
	//case METATAG_RIPPER:
	case METATAG_COMMENT:
		FileTag = &UseInfo->Tag;
		
		if (TagIdx == METATAG_YEAR)
		{
			wchar_t* lastslash;
			
			if (FileTag->strReleaseDate == NULL || ! wcslen(FileTag->strReleaseDate))
				return false;
			
			// Note: Date formatting is almost unchanged from Maxim's old in_vgm 0.35
			// try to detect various date formats
			// Not yyyy/mm/dd:
			//    nn/nn/yy    n/n/yy    nn/n/yy    n/nn/yy
			//    nn/nn/yyyy  n/n/yyyy  nn/n/yyyy  n/nn/yyyy
			// Should be:
			//    yyyy
			//    yyyy/mm
			//    yyyy/mm/dd
			lastslash = wcsrchr(FileTag->strReleaseDate, L'/');
			if (lastslash != NULL)
			{
				long year = wcstol(lastslash + 1, NULL, 10);
				if (year > 31) // looks like a year
				{
					if (year < 100) // 2-digit, yuck
					{
						year += 1900;
						if (year < 1960)
							// not many sound chips around then, due to lack of transistors
							year += 100;
					}
					_snwprintf(RetBuffer, RetBufLen, L"%d", year);
				}
			}
			if (*RetBuffer == L'\0')
			{
				// else, try the first bit
				wcsncpy(RetBuffer, FileTag->strReleaseDate, 4);
				RetBuffer[4] = L'\0';
			}
		}
		else
		{
			const wchar_t* TagStr;
			
			switch(TagIdx)
			{
			case METATAG_TITLE:
				TagStr = GetTagStringEngJap(FileTag->strTrackNameE, FileTag->strTrackNameJ,
											Options.JapTags);
				break;
			case METATAG_ALBUM:
				TagStr = GetTagStringEngJap(FileTag->strGameNameE, FileTag->strGameNameJ,
											Options.JapTags);
				break;
			case METATAG_GENRE:
				TagStr = GetTagStringEngJap(FileTag->strSystemNameE, FileTag->strSystemNameJ,
											Options.JapTags);
				break;
			case METATAG_AUTHOR:
				TagStr = GetTagStringEngJap(FileTag->strAuthorNameE, FileTag->strAuthorNameJ,
											Options.JapTags);
				break;
			case METATAG_COMMENT:
				TagStr = FileTag->strNotes;
				break;
			default:
				TagStr = NULL;
				break;
			}
			
			if (TagStr != NULL)
				wcsncpy(RetBuffer, TagStr, RetBufLen);
			
			if (TagIdx == METATAG_ALBUM && CheckFM2413Text(&UseInfo->Head))
				wcscat(RetBuffer, L" (FM)");
			break;
		}
		break;
	default:
		// do nothing
		break;
	}
	
	// empty buffer is a bad result
	return (*RetBuffer != L'\0') ? true : false;
}

const wchar_t* GetTagStringEngJap(const wchar_t* TextEng, const wchar_t* TextJap,
									bool LangMode)
{
	const wchar_t* Text;
	
	if (TextEng == NULL || ! wcslen(TextEng))
	{
		Text = TextJap;
	}
	else if (TextJap == NULL || ! wcslen(TextJap))
	{
		Text = TextEng;
	}
	else
	{
		if (! LangMode)
			Text = TextEng;
		else
			Text = TextJap;
	}
	
	if (Text == NULL || ! wcslen(Text))
		return NULL;
	else
		return Text;
}

void DisplayTagString(HWND hWndDlg, int DlgItem, const wchar_t* TextEng,
					  const wchar_t* TextJap, bool LangMode)
{
	const wchar_t* Text;
	BOOL RetVal;
	
	if (Options.TagFallback)
		Text = GetTagStringEngJap(TextEng, TextJap, LangMode);
	else
		Text = LangMode ? TextJap : TextEng;
	if (Text == NULL)
	{
		RetVal = SetDlgItemTextA(hWndDlg, DlgItem, "");
		return;
	}
	
	RetVal = SetDlgItemTextW(hWndDlg, DlgItem, Text);
	if (! RetVal)
	{
		// Unicode version failed, try ANSI version
		char ANSIStr[0x400];
		
		wcstombs(ANSIStr, Text, 0x400);
		/*RetVal = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, Text, -1, ANSIStr, 0x400,
									NULL, NULL);
		if (! RetVal)
			return;	// Unicode -> ANSI failed*/
		RetVal = SetDlgItemTextA(hWndDlg, DlgItem, ANSIStr);
	}
	
	return;
}

BOOL CALLBACK FileInfoDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
	{
		char TempStr[0x100];
		//----------------------------------------------------------------
		// Initialise dialogue
		//----------------------------------------------------------------
		
		/*int i;
		char tempstr[1024];
		
		if ( IsURL( FileInfo_Name ) ) {
			// Filename is a URL
			if (
				CurrentURLFilename && CurrentURL && (
					( strcmp( FileInfo_Name, CurrentURLFilename ) == 0) ||
					( strcmp( FileInfo_Name, CurrentURL				 ) == 0)
				)
			)
			{
				// If it's the current file, look at the temp file
				// but display the URL
				SetDlgItemText(hWndDlg, ebFilename, CurrentURL );
				FileInfo_Name = CurrentURLFilename;
			}
			else
			{
				// If it's not the current file, no info
				SetDlgItemText(hWndDlg, ebFilename, FileInfo_Name );
				SetDlgItemText(hWndDlg, ebNotes, "Information unavailable for this URL" );
				DISABLECONTROL(hWndDlg, BrwsrInfoButton );
				return TRUE;
			}
		} else {
			// Filename is not a URL
			SetDlgItemText(hWndDlg, ebFilename, FileInfo_Name );
		}*/
		
		// Load File Info
		if (FileNameToLoadW != NULL)
			LoadInfoW(FileNameToLoadW, &VGMInfo);
		else
			LoadInfoA(FileNameToLoad, &VGMInfo);
		
		SetDlgItemTextW(hWndDlg, VGMFileText, VGMInfo.FileName);
		
		if (VGMInfo.FileSize)
		{
			sprintf(TempStr, "%X.%02X", VGMInfo.Head.lngVersion >> 8,
										VGMInfo.Head.lngVersion & 0xFF);
			SetDlgItemText(hWndDlg, VGMVerText, TempStr);
			
			sprintf(TempStr, "%.2f", VGMInfo.VolGain);
			SetDlgItemText(hWndDlg, VGMGainText, TempStr);
			
			GetChipUsageText(TempStr, &VGMInfo.Head);
			SetDlgItemText(hWndDlg, ChipUseText, TempStr);
			
			sprintf(TempStr, "%d bytes (%.2f kbps)", VGMInfo.FileSize, VGMInfo.BitRate / 1000);
			SetDlgItemText(hWndDlg, VGMSizeText, TempStr);
			
			FormatVGMLength(TempStr, &VGMInfo);
			SetDlgItemText(hWndDlg, VGMLenText, TempStr);
		}
		else
		{
			SetDlgItemText(hWndDlg, VGMVerText, "");
			SetDlgItemText(hWndDlg, VGMGainText, "");
			SetDlgItemText(hWndDlg, ChipUseText, "");
			SetDlgItemText(hWndDlg, VGMSizeText, "");
			SetDlgItemText(hWndDlg, VGMLenText, "");
		}
		
		// display language independent tags
		DisplayTagString(hWndDlg, GameDateText, VGMInfo.Tag.strReleaseDate, NULL, false);
		DisplayTagString(hWndDlg, VGMCrtText, VGMInfo.Tag.strCreator, NULL, false);
		DisplayTagString(hWndDlg, VGMNotesText, VGMInfo.Tag.strNotes, NULL, false);
		
		// trigger English or Japanese for other fields
		SETCHECKBOX(hWndDlg, LangEngCheck + Options.JapTags, TRUE);
		PostMessage(hWndDlg, WM_COMMAND, LangEngCheck + Options.JapTags, 0);
		
		return TRUE;
	}
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			//Options.JapTags = IsDlgButtonChecked(hWndDlg, LangJapCheck);
			//EndDialog(hWndDlg, 0);	// return 0 = OK
			EndDialog(hWndDlg, 1);	// returning 0 can cause bugs with the playlist
			return TRUE;
		case IDCANCEL:	// [X] button, ESC, Alt + F4
			EndDialog(hWndDlg, 1);	// return 1 = Cancel, stops further dialogues being opened
			return TRUE;
		case ConfigPluginButton:
			Config(hWndDlg);
			break;
		case LangEngCheck:
		case LangJapCheck:
		{
			bool JapMode;
			
			JapMode = (LOWORD(wParam) == LangJapCheck);
			DisplayTagString(hWndDlg, TrkTitleText, VGMInfo.Tag.strTrackNameE,
													VGMInfo.Tag.strTrackNameJ, JapMode);
			DisplayTagString(hWndDlg, TrkAuthorText, VGMInfo.Tag.strAuthorNameE,
													VGMInfo.Tag.strAuthorNameJ, JapMode);
			DisplayTagString(hWndDlg, GameNameText, VGMInfo.Tag.strGameNameE,
													VGMInfo.Tag.strGameNameJ, JapMode);
			DisplayTagString(hWndDlg, GameSysText, VGMInfo.Tag.strSystemNameE,
													VGMInfo.Tag.strSystemNameJ, JapMode);
			break;
		}
		case BrwsrInfoButton:
			//InfoInBrowser(FileInfo_Name, UseMB, TRUE);
			break;
		default:
			break;
		}
		break;
	}
	
	return FALSE;	// FALSE to signify message not processed
}


// ------------------------
// Winamp Export Functions
// ------------------------
DllExport int winampGetExtendedFileInfoW(const wchar_t* wfilename, const char* metadata,
										 wchar_t* ret, int retlen)
{
	// called by Winamp 5.3 and higher
	bool RetVal;
	
	RetVal = GetExtendedFileInfoW(wfilename, metadata, ret, retlen);
#if 0
	{
		wchar_t MsgStr[MAX_PATH * 2];
		swprintf(MsgStr, L"file: %s\nMetadata: %hs\nResult: %ls", wfilename, metadata, ret);
		MessageBoxW(NULL, MsgStr, L"GetExtFileInfoW", MB_ICONINFORMATION | MB_OK);
	}
#endif
	
	return RetVal;
}

DllExport int winampGetExtendedFileInfo(const char* filename, const char* metadata,
										char* ret, int retlen)
{
	// called by Winamp versions until 5.24
	size_t FNSize;
	wchar_t* FileNameW;
	wchar_t* wRetStr;
	bool RetVal;
	
	FNSize = mbstowcs(NULL, filename, 0);
	if (FNSize == -1)
		return 0;
	FNSize ++;
	
	FileNameW = (wchar_t*)malloc(FNSize * sizeof(wchar_t));
	mbstowcs(FileNameW, filename, FNSize);
	
	wRetStr = (wchar_t*)malloc(retlen * sizeof(wchar_t));
	
	RetVal = GetExtendedFileInfoW(FileNameW, metadata, wRetStr, retlen);
	
	if (RetVal)
		//WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wret, -1, ret, retlen, NULL, NULL);
		wcstombs(ret, wRetStr, retlen);
	
	free(FileNameW);
	free(wRetStr);
#if 0
	{
		char MsgStr[MAX_PATH * 2];
		sprintf(MsgStr, "file: %s\nMetadata: %s\nResult: %s", filename, metadata, ret);
		MessageBoxA(NULL, MsgStr, "GetExtFileInfoA", MB_ICONINFORMATION | MB_OK);
	}
#endif
	
	return RetVal;
}
