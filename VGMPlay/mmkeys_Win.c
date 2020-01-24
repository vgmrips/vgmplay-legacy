#include <Windows.h>
#include <stdio.h>

#include "chips/mamedef.h"
#include "mmkeys.h"

#ifndef VK_MEDIA_NEXT_TRACK
// from WinUser.h
#define VK_MEDIA_NEXT_TRACK    0xB0
#define VK_MEDIA_PREV_TRACK    0xB1
#define VK_MEDIA_STOP          0xB2
#define VK_MEDIA_PLAY_PAUSE    0xB3
#endif

static DWORD idThread = 0;
static HANDLE hThread = NULL;
static HANDLE hEvent = NULL;
static mmkey_cbfunc evtCallback = NULL;

static DWORD WINAPI KeyMessageThread(void* args)
{
	MSG msg;
	BOOL retValB;
	
	// enforce creation of message queue
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	
	retValB = RegisterHotKey(NULL, MMKEY_PLAY, 0, VK_MEDIA_PLAY_PAUSE);
	retValB = RegisterHotKey(NULL, MMKEY_PREV, 0, VK_MEDIA_PREV_TRACK);
	retValB = RegisterHotKey(NULL, MMKEY_NEXT, 0, VK_MEDIA_NEXT_TRACK);
	
	SetEvent(hEvent);
	
	while(retValB = GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == WM_HOTKEY)
		{
			if (evtCallback != NULL)
				evtCallback((UINT8)msg.wParam);
		}
	}
	
	UnregisterHotKey(NULL, MMKEY_PLAY);
	UnregisterHotKey(NULL, MMKEY_PREV);
	UnregisterHotKey(NULL, MMKEY_NEXT);
	
	return 0;
}

UINT8 MultimediaKeyHook_Init(void)
{
	if (hThread != NULL)
		return 0x01;
	
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hThread = CreateThread(NULL, 0x00, &KeyMessageThread, NULL, 0x00, &idThread);
	if (hThread == NULL)
		return 0xFF;		// CreateThread failed
	
	WaitForSingleObject(hEvent, INFINITE);
	
	return 0x00;
}

void MultimediaKeyHook_Deinit(void)
{
	if (hThread == NULL)
		return;
	
	PostThreadMessage(idThread, WM_QUIT, 0, 0);
	WaitForSingleObject(hThread, INFINITE);
	
	CloseHandle(hThread);
	hThread = NULL;
	
	return;
}

void MultimediaKeyHook_SetCallback(mmkey_cbfunc callbackFunc)
{
	evtCallback = callbackFunc;
	
	return;
}
