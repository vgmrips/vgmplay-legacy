// Stream.h: Header File for constants and structures related to Sound Output
//

#ifdef WIN32
#include <mmsystem.h>
#else
#define MAX_PATH	PATH_MAX
#endif

#define SAMPLESIZE		sizeof(WAVE_16BS)
#define BUFSIZE_MAX		0x1000		// Maximum Buffer Size in Bytes
#ifndef WIN32
#define BUFSIZELD		11			// Buffer Size
#endif
#define AUDIOBUFFERS	200			// Maximum Buffer Count
//	Windows:	BUFFERSIZE = SampleRate / 100 * SAMPLESIZE (44100 / 100 * 4 = 1764)
//				1 Audio-Buffer = 10 msec, Min: 5
//				Win95- / WinVista-safe: 500 msec
//	Linux:		BUFFERSIZE = 1 << BUFSIZELD (1 << 11 = 2048)
//				1 Audio-Buffer = 11.6 msec

UINT8 SaveFile(UINT32 FileLen, const void* TempData);
UINT8 SoundLogging(UINT8 Mode);
UINT8 StartStream(UINT8 DeviceID);
UINT8 StopStream(void);
void PauseStream(bool PauseOn);
