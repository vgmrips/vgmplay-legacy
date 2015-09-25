/*
 *  This file is part of VGMPlay <https://github.com/vgmrips/vgmplay>
 *
 *  (c)2015 libertyernie <maybeway36@gmail.com>
 *  Based on vgm2pcm.c:
 *    (c)2015 Francis Gagné <fragag1@gmail.com>
 *    (c)2015 Valley Bell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <fcntl.h>

#ifndef _MSC_VER
// This turns command line options on (using getopt.h) unless you are using MSVC / Visual Studio, which doesn't have it.
#define VGM2WAV_HAS_GETOPT
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

bool WriteSmplChunk;

INLINE int fputLE16(UINT16 Value, FILE* hFile)
{
	int RetVal;
	int ResVal;

	RetVal = fputc((Value & 0x00FF) >> 0, hFile);
	RetVal = fputc((Value & 0xFF00) >> 8, hFile);
	ResVal = (RetVal != EOF) ? 0x02 : 0x00;
	return ResVal;
}

INLINE int fputLE32(UINT32 Value, FILE* hFile)
{
	int RetVal;
	int ResVal;

	RetVal = fputc((Value & 0x000000FF) >> 0, hFile);
	RetVal = fputc((Value & 0x0000FF00) >> 8, hFile);
	RetVal = fputc((Value & 0x00FF0000) >> 16, hFile);
	RetVal = fputc((Value & 0xFF000000) >> 24, hFile);
	ResVal = (RetVal != EOF) ? 0x02 : 0x00;
	return ResVal;
}

void usage(const char *name) {
	fprintf(stderr, "usage: %s [options] vgm_file wav_file\n"
		"wav_file can be - for standard output.\n", name);
#ifdef VGM2WAV_HAS_GETOPT
	fputs("\n"
		"Options:\n"
		"--loop-count {number}\n"
		"--fade-ms {number}\n"
		"--no-smpl-chunk\n"
		"\n", stderr);
#else
	fputs("Options not supported in this build (compiled without getopt.)\n", stderr);
#endif
}

int main(int argc, char *argv[]) {
	WAVE_16BS *sampleBuffer;
	UINT32 bufferedLength;
	FILE *outputFile;

	long int wavRIFFLengthPos;
	long int wavDataLengthPos;
	int sampleBytesWritten = 0;

	// Initialize VGMPlay before parsing arguments, so we can set VGMMaxLoop and FadeTime
	VGMPlay_Init();
	VGMPlay_Init2();

	VGMMaxLoop = 2;
	FadeTime = 5000;
	WriteSmplChunk = true;

	int c;

	// Parse command line arguments
#ifdef VGM2WAV_HAS_GETOPT
	static struct option long_options[] = {
		{ "loop-count", required_argument, NULL, 'l' },
		{ "fade-ms", required_argument, NULL, 'f' },
		{ "no-smpl-chunk", no_argument, NULL, 'S' },
		{ "help", no_argument, NULL, '?' },
		{ NULL, 0, NULL, 0 }
	};
	while ((c = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
		switch (c) {
		case 'l':
			c = atoi(optarg);
			if (c <= 0) {
				fputs("Error: loop count must be at least 1.\n", stderr);
				usage(argv[0]);
				return 1;
			}
			VGMMaxLoop = c;
			//fprintf(stderr, "Setting max loops to %u\n", VGMMaxLoop);
			break;
		case 'f':
			FadeTime = atoi(optarg);
			//fprintf(stderr, "Setting fade-out time in milliseconds to %u\n", FadeTime);
			break;
		case 'S':
			WriteSmplChunk = false;
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
	argv[optind - 1] = argv[0];
	argc -= optind - 1;
	argv += optind - 1;
#endif
	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	if (!OpenVGMFile(argv[1])) {
		fprintf(stderr, "vgm2wav: error: failed to open vgm_file (%s)\n", argv[1]);
		return 1;
	}

	if (argv[2][0] == '-' && argv[2][1] == '\0') {
#ifdef O_BINARY
		setmode(fileno(stdout), O_BINARY);
#endif
		outputFile = stdout;
	} else {
		outputFile = fopen(argv[2], "wb");
		if (outputFile == NULL) {
			fprintf(stderr, "vgm2wav: error: failed to open wav_file (%s)\n", argv[2]);
			return 1;
		}
	}

	if (WriteSmplChunk && VGMHead.lngLoopSamples == 0) {
		WriteSmplChunk = false;
	}

	fwrite("RIFF", 1, 4, outputFile);

	wavRIFFLengthPos = ftell(outputFile);
	fputLE32(-1, outputFile);

	fwrite("WAVE", 1, 4, outputFile);

	fwrite("fmt ", 1, 4, outputFile);
	fputLE32(16, outputFile);
	fputLE16(1, outputFile);
	fputLE16(2, outputFile);
	fputLE32(SampleRate, outputFile);
	fputLE32(SampleRate * 2 * 2, outputFile);
	fputLE16(2 * 2, outputFile);
	fputLE16(16, outputFile);

	if (WriteSmplChunk) {
		fwrite("smpl", 1, 4, outputFile);
		fputLE32(60, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(1, outputFile);
		fputLE32(0, outputFile);

		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
		fputLE32(VGMHead.lngTotalSamples - VGMHead.lngLoopSamples, outputFile);
		fputLE32(VGMHead.lngTotalSamples, outputFile);
		fputLE32(0, outputFile);
		fputLE32(0, outputFile);
	}

	fwrite("data", 1, 4, outputFile);

	wavDataLengthPos = ftell(outputFile);
	fputLE32(-1, outputFile);

	PlayVGM();

	sampleBuffer = (WAVE_16BS*)malloc(SAMPLESIZE * SampleRate);
	if (sampleBuffer == NULL) {
		fprintf(stderr, "vgm2wav: error: failed to allocate %u bytes of memory\n", SAMPLESIZE * SampleRate);
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
				fputLE16(sampleData[currentSample], outputFile);
				sampleBytesWritten += 2;
			}
		}
	}

	fflush(outputFile);
	StopVGM();

	CloseVGMFile();

	VGMPlay_Deinit();

	if (wavRIFFLengthPos >= 0) {
		fseek(outputFile, wavRIFFLengthPos, SEEK_SET);
		if (WriteSmplChunk) {
			fputLE32(sampleBytesWritten + 28 + 68 + 8, outputFile);
		} else {
			fputLE32(sampleBytesWritten + 28 + 8, outputFile);
		}
	}
	if (wavDataLengthPos >= 0) {
		fseek(outputFile, wavDataLengthPos, SEEK_SET);
		fputLE32(sampleBytesWritten, outputFile);
	}

	return 0;
}
