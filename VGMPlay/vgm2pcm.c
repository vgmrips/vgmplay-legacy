#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef WIN32
#include <windows.h>
#else
#include <limits.h>
#define MAX_PATH PATH_MAX
#include <unistd.h>
#include <libgen.h>
#endif

#ifdef WIN32
#define DIR_CHR     '\\'
#define DIR_STR     "\\"
#define QMARK_CHR   '\"'
#else
#define DIR_CHR     '/'
#define DIR_STR     "/"
#define QMARK_CHR   '\''
#endif

#include "chips/mamedef.h"
#include "stdbool.h"
#include "VGMPlay.h"
#include "VGMPlay_Intf.h"

#define SAMPLESIZE sizeof(WAVE_16BS)

UINT8 CmdList[0x100]; // used by VGMPlay.c and VGMPlay_AddFmts.c
bool ErrorHappened;   // used by VGMPlay.c and VGMPlay_AddFmts.c
extern VGM_HEADER VGMHead;
extern UINT32 SampleRate;
extern UINT32 VGMMaxLoopM;
extern UINT32 FadeTime;
extern bool EndPlay;
extern char *AppPaths[8];
static char AppPathBuffer[MAX_PATH * 2];

static char* GetAppFileName(void)
{
	char* AppPath;
	int RetVal;

	AppPath = (char*)malloc(MAX_PATH * sizeof(char));
#ifdef WIN32
	RetVal = GetModuleFileName(NULL, AppPath, MAX_PATH);
	if (! RetVal)
		AppPath[0] = '\0';
#else
	RetVal = readlink("/proc/self/exe", AppPath, MAX_PATH);
	if (RetVal == -1)
		AppPath[0] = '\0';
#endif

	return AppPath;
}

INLINE int fputBE16(UINT16 Value, FILE* hFile)
{
	int RetVal;
	int ResVal;

	RetVal = fputc((Value & 0xFF00) >> 8, hFile);
	RetVal = fputc((Value & 0x00FF) >> 0, hFile);
	ResVal = (RetVal != EOF) ? 0x02 : 0x00;
	return ResVal;
}

int main(int argc, char *argv[]) {
	UINT8 result;
	WAVE_16BS *sampleBuffer;
	UINT32 bufferedLength;
	FILE *outputFile;
	char *AppName;
	char* AppPathPtr;
	const char *StrPtr;
	UINT8 CurPath;
	UINT32 ChrPos;

	if (argc < 3) {
		fputs("usage: vgm2pcm vgm_file pcm_file\n", stderr);
		return 1;
	}

	VGMPlay_Init();
	// Path 2: exe's directory
	AppPathPtr = AppPathBuffer;
	AppName = GetAppFileName(); // "C:\VGMPlay\VGMPlay.exe"
	// Note: GetAppFileName always returns native directory separators.
	StrPtr = strrchr(AppName, DIR_CHR);
	if (StrPtr != NULL)
	{
		ChrPos = StrPtr + 1 - AppName;
		strncpy(AppPathPtr, AppName, ChrPos);
		AppPathPtr[ChrPos] = 0x00;  // "C:\VGMPlay\"
		AppPaths[CurPath] = AppPathPtr;
		CurPath ++;
		AppPathPtr += ChrPos + 1;
	}
	VGMPlay_Init2();

	if (!OpenVGMFile(argv[1])) {
		fprintf(stderr, "vgm2pcm: error: failed to open vgm_file (%s)\n", argv[1]);
		return 1;
	}

	if(!strcmp(argv[2], "-")) {
		outputFile = stdout;
	} else {
		outputFile = fopen(argv[2], "wb");
		if (outputFile == NULL) {
			fprintf(stderr, "vgm2pcm: error: failed to open pcm_file (%s)\n", argv[2]);
			return 1;
		}
	}

	PlayVGM();

	sampleBuffer = (WAVE_16BS*)malloc(SAMPLESIZE * SampleRate);
	if (sampleBuffer == NULL) {
		fprintf(stderr, "vgm2pcm: error: failed to allocate %lu bytes of memory\n", SAMPLESIZE * SampleRate);
		return 1;
	}

	while (!EndPlay) {
		UINT32 bufferSize = SampleRate;
		bufferedLength = FillBuffer(sampleBuffer, bufferSize);
		if (bufferedLength) {
			UINT32 numberOfSamples;
			UINT32 currentSample;
			const UINT16* sampleData;

			sampleData = (UINT16*)sampleBuffer;
			numberOfSamples = SAMPLESIZE * bufferedLength / 0x02;
			for (currentSample = 0x00; currentSample < numberOfSamples; currentSample++) {
				fputBE16(sampleData[currentSample], outputFile);
			}
		}
	}

	StopVGM();

	CloseVGMFile();

	VGMPlay_Deinit();

	return 0;
}
