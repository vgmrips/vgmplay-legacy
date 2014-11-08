// Stream.c: C Source File for Sound Output
//

#include <stdio.h>
#include "stdbool.h"
#include <malloc.h>

#ifdef WIN32
#include <windows.h>
#else
#include <limits.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <unistd.h>

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
extern bool PauseThread;
extern bool ThreadPauseEnable;
extern bool ThreadPauseConfrm;

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
static bool CloseThread;


bool SoundLog;
static FILE* hFile;
UINT32 SndLogLen;

UINT32 BlocksSent;
UINT32 BlocksPlayed;

char SoundLogFile[MAX_PATH];

UINT8 SaveFile(UINT32 FileLen, void* TempData)
{
	//char ResultStr[0x100];
	long int TempVal[0x2];
	
	if (TempData == NULL)
	{
		switch (FileLen)
		{
		/*case 0x80000000:
			SndLogLen = 0;
			hFile = fopen("SoundLog.wav","wb");
			fclose(hFile);
			break;*/
		case 0x00000000:
			SndLogLen = 0;
			//hFile = fopen("SoundLog.wav","wb");
			hFile = fopen(SoundLogFile,"wb");
			if (hFile == NULL)
			{
				// Save Error
				return 0xFF;
			}
			fseek(hFile, 0x00000000, SEEK_SET);
			TempVal[0x0] = 0x46464952;
			TempVal[0x1] = 0x00000000;
			fwrite(&TempVal[0x0], 4, 2, hFile);
			TempVal[0x0] = 0x45564157;
			TempVal[0x1] = 0x20746D66;
			fwrite(&TempVal[0x0], 4, 2, hFile);
			TempVal[0x0] = 0x00000010;
			fwrite(&TempVal[0x0], 4, 1, hFile);
			fwrite(&WaveFmt, TempVal[0x0], 1, hFile);
			TempVal[0x0] = 0x61746164;
			TempVal[0x1] = 0x00000000;
			fwrite(&TempVal[0x0], 4, 2, hFile);
			break;
		case 0xFFFFFFFF:
			TempVal[0x1] = SndLogLen * SAMPLESIZE;
			TempVal[0x0] = TempVal[0x1] + 0x00000024;
			fseek(hFile, 0x0004, SEEK_SET);
			fwrite(&TempVal[0x0], 4, 1, hFile);
			fseek(hFile, 0x0028, SEEK_SET);
			fwrite(&TempVal[0x1], 4, 1, hFile);
			fclose(hFile);
			hFile = NULL;
			break;
		}
	}
	else
	{
		//fseek(hFile, 0x00000000, SEEK_END);
		//TempVal[0x0] = ftell(hFile);
		//TempVal[0x1] = fwrite(TempData, 1, FileLen, hFile);
		SndLogLen += fwrite(TempData, SAMPLESIZE, FileLen, hFile);
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
	switch (Mode)
	{
	case 0x00:
		SoundLog = false;
		break;
	case 0x01:
		SoundLog = true;
		if (WaveOutOpen && hFile == NULL)
		{
			SaveFile(0x00000000, NULL);
		}
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
#ifdef WIN32
	UINT16 Cnt;
	HANDLE WaveOutThreadHandle;
	DWORD WaveOutThreadID;
	//char TestStr[0x80];
#else
	UINT32 ArgVal;
#endif
	
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
	
#ifdef WIN32
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
	
#ifdef WIN32
	ThreadPauseEnable = true;
	WaveOutThreadHandle = CreateThread(NULL, 0x00, &WaveOutThread, NULL, 0x00,
										&WaveOutThreadID);
	if(WaveOutThreadHandle == NULL)
	{
		return 0xC8;		// CreateThread failed
	}
	CloseHandle(WaveOutThreadHandle);
	
	RetVal = waveOutOpen(&hWaveOut, ((UINT)DeviceID - 1), &WaveFmt, 0x00, 0x00, CALLBACK_NULL);
	if(RetVal != MMSYSERR_NOERROR)
#else
	ThreadPauseEnable = false;
	hWaveOut = open("/dev/dsp", O_WRONLY);
	if (hWaveOut < 0)
#endif
	{
		CloseThread = true;
		return 0xC0;		// waveOutOpen failed
	}
	WaveOutOpen = true;
	
	//sprintf(TestStr, "Buffer 0,0:\t%p\nBuffer 0,1:\t%p\nBuffer 1,0:\t%p\nBuffer 1,1:\t%p\n",
	//		&BufferOut[0][0], &BufferOut[0][1], &BufferOut[1][0], &BufferOut[1][1]);
	//AfxMessageBox(TestStr);
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
	
	if (SoundLog)
	{
		SaveFile(0x00000000, NULL);
	}
	
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
	{
		SaveFile(0xFFFFFFFF, NULL);
	}
	WaveOutOpen = false;
	
#ifdef WIN32
	RetVal = waveOutReset(hWaveOut);
	for (Cnt = 0x00; Cnt < AUDIOBUFFERU; Cnt ++)
	{
		RetVal = waveOutUnprepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
	}
	RetVal = waveOutClose(hWaveOut);
	if(RetVal != MMSYSERR_NOERROR)
	{
		return 0xC4;		// waveOutClose failed  -- but why ???
	}
#else
	close(hWaveOut);
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
#else
	PauseThread = PauseOn;
#endif
	
	return;
}

//void FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize)
// moved to VGMPlay.c

#ifdef WIN32

static DWORD WINAPI WaveOutThread(void* Arg)
{
#ifdef NDEBUG
	UINT32 RetVal;
#endif
	UINT16 CurBuf;
	WAVE_16BS* TempBuf;
	//char TestStr[0x80];
	
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
	while (! CloseThread)
	{
		while (PauseThread && ! CloseThread)
		{
			ThreadPauseConfrm = true;
			Sleep(1);
		}
		if (CloseThread)
			break;
		
		BufCheck();
		for (CurBuf = 0x00; CurBuf < AUDIOBUFFERU; CurBuf ++)
		{
			if (WaveHdrOut[CurBuf].dwFlags & WHDR_DONE)
			{
				TempBuf = (WAVE_16BS*)WaveHdrOut[CurBuf].lpData;
				
				if (WaveHdrOut[CurBuf].dwUser & 0x01)
					BlocksPlayed ++;
				else
					WaveHdrOut[CurBuf].dwUser |= 0x01;
				FillBuffer(TempBuf, SMPL_P_BUFFER);
				waveOutWrite(hWaveOut, &WaveHdrOut[CurBuf], sizeof(WAVEHDR));
				if (SoundLog && hFile != NULL)
					SaveFile(SMPL_P_BUFFER, TempBuf);
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
	
	if (! WaveOutOpen)
		return;	// Device not opened
	
	CurBuf = BlocksSent % AUDIOBUFFERU;
	TempBuf = (WAVE_16BS*)BufferOut[CurBuf];
	
	FillBuffer(TempBuf, SMPL_P_BUFFER);
	RetVal = write(hWaveOut, TempBuf, BUFFERSIZE);
	if (SoundLog && hFile != NULL)
		SaveFile(SMPL_P_BUFFER, TempBuf);
	BlocksSent ++;
	BlocksPlayed ++;
	
	return;
}

#endif
