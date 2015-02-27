/*
 *  This file is part of VGMPlay <https://github.com/vgmrips/vgmplay>
 *
 *  (c)2015 Francis Gagné <fragag1@gmail.com>
 *  (c)2015 Valley Bell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "stdbool.h"
#include "utils.h"
#include "VGMPlay.h"
#include "VGMPlay_Intf.h"

#define SAMPLESIZE sizeof(WAVE_16BS)
extern VGM_HEADER VGMHead;
extern UINT32 SampleRate;
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
        fprintf(stderr, "usage: %s vgm_file pcm_file\n", argv[0]);
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
