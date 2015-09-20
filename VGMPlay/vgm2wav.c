#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

    if (argc < 3) {
        fputs("usage: vgm2pcm vgm_file pcm_file\n", stderr);
        return 1;
    }

    VGMPlay_Init();
    VGMPlay_Init2();

    if (!OpenVGMFile(argv[1])) {
        fprintf(stderr, "vgm2pcm: error: failed to open vgm_file (%s)\n", argv[1]);
        return 1;
    }

    outputFile = fopen(argv[2], "wb");
    if (outputFile == NULL) {
        fprintf(stderr, "vgm2pcm: error: failed to open pcm_file (%s)\n", argv[2]);
        return 1;
    }

    PlayVGM();

    sampleBuffer = (WAVE_16BS*)malloc(SAMPLESIZE * SampleRate);
    if (sampleBuffer == NULL) {
        fprintf(stderr, "vgm2pcm: error: failed to allocate %u bytes of memory\n", SAMPLESIZE * SampleRate);
        return 1;
    }

    while (!EndPlay) {
        UINT32 bufferSize = SampleRate;
        bufferedLength = FillBuffer(sampleBuffer, bufferSize);
        if (bufferedLength) {
            UINT32 numberOfSamples;
            UINT32 currentSample;
            const UINT16* sampleData;

            sampleData = (INT16*)sampleBuffer;
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
