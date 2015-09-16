#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef _MSC_VER
// This turns command line options on (using getopt.h) unless you are using MSVC / Visual Studio, which doesn't have it.
#define VGM2PCM_HAS_GETOPT
#include <getopt.h>
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
extern bool EndPlay;

extern UINT32 VGMMaxLoop;
extern UINT32 FadeTime;

INLINE int fputBE16(UINT16 Value, FILE* hFile)
{
    int RetVal;
    int ResVal;

    RetVal = fputc((Value & 0xFF00) >> 8, hFile);
    RetVal = fputc((Value & 0x00FF) >> 0, hFile);
    ResVal = (RetVal != EOF) ? 0x02 : 0x00;
    return ResVal;
}

void usage(const char *name) {
    fprintf(stderr, "usage: %s [options] vgm_file pcm_file\n"
                    //"pcm_file can be - for standard output.\n"
                    "\n"
                    "Default options:\n"
                    "--loop-count=%d\n"
                    "--fade-ms=%d\n"
                    "--format=l16\n"
                    "\n", name, VGMMaxLoop, FadeTime);
}

int main(int argc, char *argv[]) {
    UINT8 result;
    WAVE_16BS *sampleBuffer;
    UINT32 bufferedLength;
    FILE *outputFile;

	// Initialize VGMPlay before parsing arguments, so we can set VGMMaxLoop and FadeTime
    VGMPlay_Init();
    VGMPlay_Init2();
    
    int c;
    
    // Parse command line arguments
#ifdef VGM2PCM_HAS_GETOPT
    static struct option long_options[] = {
        {"loop-count", required_argument, NULL, 'l'},
        {"fade-ms", required_argument, NULL, 'f'},
        {"format", required_argument, NULL, 't'},
        {"help", no_argument, NULL, '?'},
        {NULL, 0, NULL, 0}
    };
    while ((c = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (c) {
            case 'l':
                VGMMaxLoop = atoi(optarg);
                if (VGMMaxLoop <= 0) {
                    fputs("Error: loop count must be at least 1.\n", stderr);
                    usage(argv[0]);
                    return 1;
                }
                //fprintf(stderr, "Setting max loops to %u\n", VGMMaxLoop);
                break;
            case 'f':
                FadeTime = atoi(optarg);
                //fprintf(stderr, "Setting fade-out time in milliseconds to %u\n", FadeTime);
                break;
            case 't':
                fprintf(stderr, "Would use format %s if this was implemented\n", optarg);
                break;
            case -1:
                break;
            case '?':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    
    // Pretend for the rest of the program that those options don't exist
    argv[optind-1] = argv[0];
    argc -= optind-1;
    argv += optind-1;
#else
    if (argc < 3) {
        fputs("usage: vgm2pcm vgm_file pcm_file\n", stderr);
        return 1;
    }
#endif

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
