// Stream.c: C Source File for Sound Output
//

// Thanks to nextvolume for NetBSD support

#include <stdio.h>
#include "stdbool.h"
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#else
#include <limits.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef __NetBSD__
#include <sys/audioio.h>
#elif defined(__APPLE__)
// nothing
#else
#include <linux/soundcard.h>
#endif
#include <unistd.h>

#endif

#ifdef USE_LIBAO
#ifdef WIN32
#error "Sorry, but this doesn't work yet!"
#endif
#include <ao/ao.h>
#endif

#include "chips/mamedef.h"	// for UINT8 etc.
#include "VGMPlay.h"	// neccessary for VGMPlay_Intf.h
#include "VGMPlay_Intf.h"
#include "Stream.h"

#ifndef WIN32
typedef struct
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
	UINT16 cbSize;
} WAVEFORMATEX;	// from MSDN Help

#define WAVE_FORMAT_PCM	0x0001

#endif

#ifdef WIN32
static DWORD WINAPI WaveOutThread(void* Arg);
static void BufCheck(void);
#else
void WaveOutCallbackFnc(void);
#endif

UINT16 AUDIOBUFFERU = AUDIOBUFFERS;		// used AudioBuffers

WAVEFORMATEX WaveFmt;
extern UINT32 SampleRate;
extern volatile bool PauseThread;
volatile bool StreamPause;
extern bool ThreadPauseEnable;
extern volatile bool ThreadPauseConfrm;

UINT32 BlockLen;
#ifdef WIN32
static HWAVEOUT hWaveOut;
static WAVEHDR WaveHdrOut[AUDIOBUFFERS];
static HANDLE hWaveOutThread;
//static DWORD WaveOutCallbackThrID;
#else
static INT32 hWaveOut;
#endif
static bool WaveOutOpen;
UINT32 BUFFERSIZE;	// Buffer Size in Bytes
UINT32 SMPL_P_BUFFER;
static char BufferOut[AUDIOBUFFERS][BUFSIZE_MAX];
static volatile bool CloseThread;


bool SoundLog;
static FILE* hFile;
UINT32 SndLogLen;

UINT32 BlocksSent;
UINT32 BlocksPlayed;

char SoundLogFile[MAX_PATH];

#ifdef USE_LIBAO
ao_device* dev_ao;
#endif

INLINE int fputLE32(UINT32 Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	return fwrite(&Value, 0x04, 1, hFile);
#else
	int RetVal;
	int ResVal;
	
	RetVal = fputc((Value & 0x000000FF) >>  0, hFile);
	RetVal = fputc((Value & 0x0000FF00) >>  8, hFile);
	RetVal = fputc((Value & 0x00FF0000) >> 16, hFile);
	RetVal = fputc((Value & 0xFF000000) >> 24, hFile);
	ResVal = (RetVal != EOF) ? 0x04 : 0x00;
	return ResVal;
#endif
}

INLINE int fputLE16(UINT16 Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	return fwrite(&Value, 0x02, 1, hFile);
#else
	int RetVal;
	int ResVal;
	
	RetVal = fputc((Value & 0x00FF) >> 0, hFile);
	RetVal = fputc((Value & 0xFF00) >> 8, hFile);
	ResVal = (RetVal != EOF) ? 0x02 : 0x00;
	return ResVal;
#endif
}

UINT8 SaveFile(UINT32 FileLen, const void* TempData)
{
	//char ResultStr[0x100];
	UINT32 DataLen;
	
	if (TempData == NULL)
	{
		switch(FileLen)
		{
		case 0x00000000:
			if (hFile != NULL)
				return 0xD0;	// file already open
			
			SndLogLen = 0;
			hFile = fopen(SoundLogFile,"wb");
			if (hFile == NULL)
				return 0xFF;	// Save Error
			fseek(hFile, 0x00000000, SEEK_SET);
			fputLE32(0x46464952, hFile);	// 'RIFF'
			fputLE32(0x00000000, hFile);	// RIFF chunk length (dummy)
			
			fputLE32(0x45564157, hFile);	// 'WAVE'
			fputLE32(0x20746D66, hFile);	// 'fmt '
			DataLen = 0x00000010;
			fputLE32(DataLen, hFile);		// format chunk legth
			
#ifndef VGM_BIG_ENDIAN
			fwrite(&WaveFmt, DataLen, 1, hFile);
#else
			fputLE16(WaveFmt.wFormatTag,		hFile);	// 0x00
			fputLE16(WaveFmt.nChannels,			hFile);	// 0x02
			fputLE32(WaveFmt.nSamplesPerSec,	hFile);	// 0x04
			fputLE32(WaveFmt.nAvgBytesPerSec,	hFile);	// 0x08
			fputLE16(WaveFmt.nBlockAlign,		hFile);	// 0x0C
			fputLE16(WaveFmt.wBitsPerSample,	hFile);	// 0x0E
			//fputLE16(WaveFmt.cbSize, hFile);			// 0x10 (DataLen is 0x10, so leave this out)
#endif
			
			fputLE32(0x61746164, hFile);	// 'data'
			fputLE32(0x00000000, hFile);	// data chunk length (dummy)
			break;
		case 0xFFFFFFFF:
			if (hFile == NULL)
				return 0x80;	// no file opened
			
			DataLen = SndLogLen * SAMPLESIZE;
			
			fseek(hFile, 0x0028, SEEK_SET);
			fputLE32(DataLen, hFile);			// data chunk length
			fseek(hFile, 0x0004, SEEK_SET);
			fputLE32(DataLen + 0x24, hFile);	// RIFF chunk length
			fclose(hFile);
			hFile = NULL;
			break;
		}
	}
	else
	{
		if (hFile == NULL)
			return 0x80;	// no file opened
		
		//fseek(hFile, 0x00000000, SEEK_END);
		//TempVal[0x0] = ftell(hFile);
		//TempVal[0x1] = fwrite(TempData, 1, FileLen, hFile);
#ifndef VGM_BIG_ENDIAN
		SndLogLen += fwrite(TempData, SAMPLESIZE, FileLen, hFile);
#else
		{
			UINT32 CurSmpl;
			const UINT16* SmplData;
			
			SmplData = (INT16*)TempData;
			DataLen = SAMPLESIZE * FileLen / 0x02;
			for (CurSmpl = 0x00; CurSmpl < DataLen; CurSmpl ++)
				SndLogLen += fputLE16(SmplData[CurSmpl], hFile);
		}
#endif
		//sprintf(ResultStr, "Position:\t%ld\nBytes written:\t%ld\nFile Length:\t%lu\nPointer:\t%p",
		//		TempVal[0], TempVal[1], FileLen, TempData);
		//AfxMessageBox(ResultStr);
	}
	
	return 0x00;
}

UINT8 SoundLogging(UINT8 Mode)
{
	UINT8 RetVal;
	
	RetVal = (UINT8)SoundLog;
	switch(Mode)
	{
	case 0x00:
		SoundLog = false;
		break;
	case 0x01:
		SoundLog = true;
		if (WaveOutOpen && hFile == NULL)
			SaveFile(0x00000000, NULL);
		break;
	case 0xFF:
		break;
	default:
		RetVal = 0xA0;
		break;
	}
	
	return RetVal;
}

UINT8 StartStream(UINT8 DeviceID)
{
	UINT32 RetVal;
#ifdef USE_LIBAO
	ao_sample_format ao_fmt;
#else
#ifdef WIN32
	UINT16 Cnt;
	HANDLE WaveOutThreadHandle;
	DWORD WaveOutThreadID;
	//char TestStr[0x80];
#elif defined(__NetBSD__)
	struct audio_info AudioInfo;
#else
	UINT32 ArgVal;
#endif
#endif	// ! USE_LIBAO
	
	if (WaveOutOpen)
		return 0xD0;	// Thread is already active
	
	// Init Audio
	WaveFmt.wFormatTag = WAVE_FORMAT_PCM;
	WaveFmt.nChannels = 2;
	WaveFmt.nSamplesPerSec = SampleRate;
	WaveFmt.wBitsPerSample = 16;
	WaveFmt.nBlockAlign = WaveFmt.wBitsPerSample * WaveFmt.nChannels / 8;
	WaveFmt.nAvgBytesPerSec = WaveFmt.nSamplesPerSec * WaveFmt.nBlockAlign;
	WaveFmt.cbSize = 0;
	if (DeviceID == 0xFF)
		return 0x00;
	
#if defined(WIN32) || defined(USE_LIBAO)
	BUFFERSIZE = SampleRate / 100 * SAMPLESIZE;
	if (BUFFERSIZE > BUFSIZE_MAX)
		BUFFERSIZE = BUFSIZE_MAX;
#else
	BUFFERSIZE = 1 << BUFSIZELD;
#endif
	SMPL_P_BUFFER = BUFFERSIZE / SAMPLESIZE;
	if (AUDIOBUFFERU > AUDIOBUFFERS)
		AUDIOBUFFERU = AUDIOBUFFERS;
	
	PauseThread = true;
	ThreadPauseConfrm = false;
	CloseThread = false;
	StreamPause = false;
	
#ifndef USE_LIBAO
#ifdef WIN32
	ThreadPauseEnable = true;
	WaveOutThreadHandle = CreateThread(NULL, 0x00, &WaveOutThread, NULL, 0x00,
										&WaveOutThreadID);
	if(WaveOutThreadHandle == NULL)
		return 0xC8;		// CreateThread failed
	CloseHandle(WaveOutThreadHandle);
	
	RetVal = waveOutOpen(&hWaveOut, ((UINT)DeviceID - 1), &WaveFmt, 0x00, 0x00, CALLBACK_NULL);
	if(RetVal != MMSYSERR_NOERROR)
#else
	ThreadPauseEnable = false;
#ifdef __NetBSD__
	hWaveOut = open("/dev/audio", O_WRONLY);
#else
	hWaveOut = open("/dev/dsp", O_WRONLY);
#endif
	if (hWaveOut < 0)
#endif
#else	// ifdef USE_LIBAO
	ao_initialize();
	
	ThreadPauseEnable = false;
	ao_fmt.bits = WaveFmt.wBitsPerSample;
	ao_fmt.rate = WaveFmt.nSamplesPerSec;
	ao_fmt.channels = WaveFmt.nChannels;
	ao_fmt.byte_format = AO_FMT_NATIVE;
	ao_fmt.matrix = NULL;
	
	dev_ao = ao_open_live(ao_default_driver_id(), &ao_fmt, NULL);
	if (dev_ao == NULL)
#endif
	{
		CloseThread = true;
		return 0xC0;		// waveOutOpen failed
	}
	WaveOutOpen = true;
	
	//sprintf(TestStr, "Buffer 0,0:\t%p\nBuffer 0,1:\t%p\nBuffer 1,0:\t%p\nBuffer 1,1:\t%p\n",
	//		&BufferOut[0][0], &BufferOut[0][1], &BufferOut[1][0], &BufferOut[1][1]);
	//AfxMessageBox(TestStr);
#ifndef USE_LIBAO
#ifdef WIN32
	for (Cnt = 0x00; Cnt < AUDIOBUFFERU; Cnt ++)
	{
		WaveHdrOut[Cnt].lpData = BufferOut[Cnt];	// &BufferOut[Cnt][0x00];
		WaveHdrOut[Cnt].dwBufferLength = BUFFERSIZE;
		WaveHdrOut[Cnt].dwBytesRecorded = 0x00;
		WaveHdrOut[Cnt].dwUser = 0x00;
		WaveHdrOut[Cnt].dwFlags = 0x00;
		WaveHdrOut[Cnt].dwLoops = 0x00;
		WaveHdrOut[Cnt].lpNext = NULL;
		WaveHdrOut[Cnt].reserved = 0x00;
		RetVal = waveOutPrepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
		WaveHdrOut[Cnt].dwFlags |= WHDR_DONE;
	}
#elif defined(__NetBSD__)
	AUDIO_INITINFO(&AudioInfo);
	
	AudioInfo.mode = AUMODE_PLAY;
	AudioInfo.play.sample_rate = WaveFmt.nSamplesPerSec;
	AudioInfo.play.channels = WaveFmt.nChannels;
	AudioInfo.play.precision = WaveFmt.wBitsPerSample;
	AudioInfo.play.encoding = AUDIO_ENCODING_SLINEAR;
	
	RetVal = ioctl(hWaveOut, AUDIO_SETINFO, &AudioInfo);
	if (RetVal)
		printf("Error setting audio information!\n");
#else
	ArgVal = (AUDIOBUFFERU << 16) | BUFSIZELD;
	RetVal = ioctl(hWaveOut, SNDCTL_DSP_SETFRAGMENT, &ArgVal);
	if (RetVal)
		printf("Error setting Fragment Size!\n");
	ArgVal = AFMT_S16_NE;
	RetVal = ioctl(hWaveOut, SNDCTL_DSP_SETFMT, &ArgVal);
	if (RetVal)
		printf("Error setting Format!\n");
	ArgVal = WaveFmt.nChannels;
	RetVal = ioctl(hWaveOut, SNDCTL_DSP_CHANNELS, &ArgVal);
	if (RetVal)
		printf("Error setting Channels!\n");
	ArgVal = WaveFmt.nSamplesPerSec;
	RetVal = ioctl(hWaveOut, SNDCTL_DSP_SPEED, &ArgVal);
	if (RetVal)
		printf("Error setting Sample Rate!\n");
#endif
#endif	// USE_LIBAO
	
	if (SoundLog)
		SaveFile(0x00000000, NULL);
	
	PauseThread = false;
	
	return 0x00;
}

UINT8 StopStream(void)
{
	UINT32 RetVal;
#ifdef WIN32
	UINT16 Cnt;
#endif
	
	if (! WaveOutOpen)
		return 0xD8;	// Thread is not active
	
	CloseThread = true;
#ifdef WIN32
	for (Cnt = 0; Cnt < 100; Cnt ++)
	{
		Sleep(1);
		if (hWaveOutThread == NULL)
			break;
	}
#endif
	if (hFile != NULL)
		SaveFile(0xFFFFFFFF, NULL);
	WaveOutOpen = false;
	
#ifndef USE_LIBAO
#ifdef WIN32
	RetVal = waveOutReset(hWaveOut);
	for (Cnt = 0x00; Cnt < AUDIOBUFFERU; Cnt ++)
		RetVal = waveOutUnprepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
	
	RetVal = waveOutClose(hWaveOut);
	if(RetVal != MMSYSERR_NOERROR)
		return 0xC4;		// waveOutClose failed  -- but why ???
#else
	close(hWaveOut);
#endif
#else	// ifdef USE_LIBAO
	ao_close(dev_ao);
	
	ao_shutdown();
#endif
	
	return 0x00;
}

void PauseStream(bool PauseOn)
{
	UINT32 RetVal;
	
	if (! WaveOutOpen)
		return;	// Thread is not active
	
#ifdef WIN32
	switch(PauseOn)
	{
	case true:
		RetVal = waveOutPause(hWaveOut);
		break;
	case false:
		RetVal = waveOutRestart(hWaveOut);
		break;
	}
	StreamPause = PauseOn;
#else
	PauseThread = PauseOn;
#endif
	
	return;
}

//UINT32 FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize)
// moved to VGMPlay.c

#ifdef WIN32

static DWORD WINAPI WaveOutThread(void* Arg)
{
#ifdef NDEBUG
	UINT32 RetVal;
#endif
	UINT16 CurBuf;
	WAVE_16BS* TempBuf;
	UINT32 WrtSmpls;
	//char TestStr[0x80];
	bool DidBuffer;	// a buffer was processed
	
	hWaveOutThread = GetCurrentThread();
#ifdef NDEBUG
	RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_TIME_CRITICAL);
	if (! RetVal)
	{
		// Error by setting priority
		// try a lower priority, because too low priorities cause sound stuttering
		RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_HIGHEST);
	}
#endif
	
	BlocksSent = 0x00;
	BlocksPlayed = 0x00;
	while(! CloseThread)
	{
		while(PauseThread && ! CloseThread && ! (StreamPause && DidBuffer))
		{
			ThreadPauseConfrm = true;
			Sleep(1);
		}
		if (CloseThread)
			break;
		
		BufCheck();
		DidBuffer = false;
		for (CurBuf = 0x00; CurBuf < AUDIOBUFFERU; CurBuf ++)
		{
			if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
			{
				TempBuf = (WAVE_16BS*)WaveHdrOut[CurBuf].lpData;
				
				if (WaveHdrOut[CurBuf].dwUser & 0x01)
					BlocksPlayed ++;
				else
					WaveHdrOut[CurBuf].dwUser |= 0x01;
				
				WrtSmpls = FillBuffer(TempBuf, SMPL_P_BUFFER);
				
				WaveHdrOut[CurBuf].dwBufferLength = WrtSmpls * SAMPLESIZE;
				waveOutWrite(hWaveOut, &WaveHdrOut[CurBuf], sizeof(WAVEHDR));
				if (SoundLog && hFile != NULL)
					SaveFile(WrtSmpls, TempBuf);
				
				DidBuffer = true;
				BlocksSent ++;
				BufCheck();
				//CurBuf = 0x00;
				//break;
			}
			if (CloseThread)
				break;
		}
		Sleep(1);
	}
	
	hWaveOutThread = NULL;
	return 0x00000000;
}

static void BufCheck(void)
{
	UINT16 CurBuf;
	
	for (CurBuf = 0x00; CurBuf < AUDIOBUFFERU; CurBuf ++)
	{
		if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
		{
			if (WaveHdrOut[CurBuf].dwUser & 0x01)
			{
				WaveHdrOut[CurBuf].dwUser &= ~0x01;
				BlocksPlayed ++;
			}
		}
	}
	
	return;
}

#else	// #ifndef WIN32

void WaveOutLinuxCallBack(void)
{
	UINT32 RetVal;
	UINT16 CurBuf;
	WAVE_16BS* TempBuf;
	UINT32 WrtSmpls;
	
	if (! WaveOutOpen)
		return;	// Device not opened
	
	CurBuf = BlocksSent % AUDIOBUFFERU;
	TempBuf = (WAVE_16BS*)BufferOut[CurBuf];
	
	WrtSmpls = FillBuffer(TempBuf, SMPL_P_BUFFER);
	
#ifndef USE_LIBAO
	RetVal = write(hWaveOut, TempBuf, WrtSmpls * SAMPLESIZE);
#else
	RetVal = ao_play(dev_ao, (char*)TempBuf, WrtSmpls * SAMPLESIZE);
#endif
	if (SoundLog && hFile != NULL)
		SaveFile(WrtSmpls, TempBuf);
	BlocksSent ++;
	BlocksPlayed ++;
	
	return;
}

#endif
