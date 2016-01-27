// VGMPlay_Intf.h: VGMPlay Interface Header File
//

//#define NO_WCHAR_FILENAMES
#ifndef WIN32
// Linux uses UTF-8 Unicode and has no special wide-character file routines.
#define NO_WCHAR_FILENAMES
#endif

typedef struct waveform_16bit_stereo
{
	INT16 Left;
	INT16 Right;
} WAVE_16BS;

typedef struct waveform_32bit_stereo
{
	INT32 Left;
	INT32 Right;
} WAVE_32BS;


void VGMPlay_Init(void);
void VGMPlay_Init2(void);
void VGMPlay_Deinit(void);
char* FindFile(const char* FileName);
char* FindFile_List(const char** FileNameList);

UINT32 GetGZFileLength(const char* FileName);
bool OpenVGMFile(const char* FileName);
void CloseVGMFile(void);

void FreeGD3Tag(GD3_TAG* TagData);
UINT32 GetVGMFileInfo(const char* FileName, VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag);
UINT32 CalcSampleMSec(UINT64 Value, UINT8 Mode);
UINT32 CalcSampleMSecExt(UINT64 Value, UINT8 Mode, VGM_HEADER* FileHead);
const char* GetChipName(UINT8 ChipID);
const char* GetAccurateChipName(UINT8 ChipID, UINT8 SubType);
UINT32 GetChipClock(VGM_HEADER* FileHead, UINT8 ChipID, UINT8* RetSubType);

#ifndef NO_WCHAR_FILENAMES
UINT32 GetGZFileLengthW(const wchar_t* FileName);
bool OpenVGMFileW(const wchar_t* FileName);
UINT32 GetVGMFileInfoW(const wchar_t* FileName, VGM_HEADER* RetVGMHead, GD3_TAG* RetGD3Tag);
#endif

INT32 SampleVGM2Playback(INT32 SampleVal);
INT32 SamplePlayback2VGM(INT32 SampleVal);

void PlayVGM(void);
void StopVGM(void);
void RestartVGM(void);
void PauseVGM(bool Pause);
void SeekVGM(bool Relative, INT32 PlayBkSamples);
void RefreshMuting(void);
void RefreshPanning(void);
void RefreshPlaybackOptions(void);

UINT32 FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize);
