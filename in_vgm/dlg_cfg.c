/*3456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
0000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999*/
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "stdbool.h"
#include "chips/mamedef.h"	// for (U)INTxx types

#include "VGMPlay.h"
#include "VGMPlay_Intf.h"
#include "in_vgm.h"


#define HIDE_VGM7Z

#define TAB_ICON_WIDTH	16
#define TAB_ICON_TRANSP	0xFF00FF


// Dialogue box tabsheet handler
#define NUM_CFG_TABS	5
#define CfgPlayback		hWndCfgTab[0]
#define CfgTags			hWndCfgTab[1]
#define Cfg7z			hWndCfgTab[2]
#define CfgMuting		hWndCfgTab[3]
#define CfgOptPan		hWndCfgTab[4]

// sadly I can't use DLG_ITEM in another define
//#define DLG_ITEM						GetDlgItem(hWnd, DlgID)
#define SLIDER_GETPOS(hWnd, DlgID)	(UINT16) \
										SendDlgItemMessage(hWnd, DlgID, TBM_GETPOS, 0, 0)
#define SLIDER_SETPOS(hWnd, DlgID, Pos)	SendDlgItemMessage(hWnd, DlgID, TBM_SETPOS, TRUE, Pos)
#define CTRL_DISABLE(hWnd, DlgID)		EnableWindow(GetDlgItem(hWnd, DlgID), FALSE)
#define CTRL_ENABLE(hWnd, DlgID)		EnableWindow(GetDlgItem(hWnd, DlgID), TRUE)
#define CTRL_HIDE(hWnd, DlgID)			ShowWindow(GetDlgItem(hWnd, DlgID), SW_HIDE)
#define CTRL_SHOW(hWnd, DlgID)			ShowWindow(GetDlgItem(hWnd, DlgID), SW_SHOW)
#define CTRL_SET_ENABLE(hWnd, DlgID, Enable)	EnableWindow(GetDlgItem(hWnd, DlgID), Enable)
#define CTRL_IS_ENABLED(hWnd, DlgID, Enable)	IsWindowEnabled(GetDlgItem(hWnd, DlgID))
#define SETCHECKBOX(hWnd, DlgID, Check)	SendDlgItemMessage(hWnd, DlgID, BM_SETCHECK, Check, 0)
#define CREATE_CHILD(DlgID, DlgProc)	\
								CreateDialog(hPluginInst, (LPCTSTR)DlgID, hWndMain, DlgProc)
#define COMBO_ADDSTR(hWnd, DlgID, String)	\
								SendDlgItemMessage(hWnd, DlgID, CB_ADDSTRING, 0, (LPARAM)String)
#define COMBO_GETPOS(hWnd, DlgID)		SendDlgItemMessage(hWnd, DlgID, CB_GETCURSEL, 0, 0)
#define COMBO_SETPOS(hWnd, DlgID, Pos)	SendDlgItemMessage(hWnd, DlgID, CB_SETCURSEL, Pos, 0)
#define CHECK2BOOL(hWnd, DlgID)			(IsDlgButtonChecked(hWnd, DlgID) == BST_CHECKED)


// Function Prototypes from in_vgm.c
void UpdatePlayback(void);


// Function Prototypes from dlg_fileinfo.c
void QueueInfoReload(void);


// Function Prototypes
void InitConfigDialog(HWND hWndMain);
INLINE void AddTab(HWND tabCtrlWnd, int ImgIndex, char* TabTitle);
static void EnableWinXPVisualStyles(HWND hWndMain);
static void Slider_Setup(HWND hWndDlg, int DlgID, int Min, int Max, int BigStep, int TickFreq);
static BOOL SetDlgItemFloat(HWND hDlg, int nIDDlgItem, double Value, int Precision);
static int LoadConfigDialogInfo(HWND hWndDlg);

BOOL CALLBACK ConfigDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CfgDlgPlaybackProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CfgDlgTagsProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CfgDlgMutingProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CfgDlgOptPanProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CfgDlgChildProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam);

static bool IsChipAvailable(UINT8 ChipID, UINT8 ChipSet);
static void ShowMutingCheckBoxes(UINT8 ChipID, UINT8 ChipSet);
static void SetMutingData(UINT32 CheckBox, bool MuteOn);
static void ShowOptPanBoxes(UINT8 ChipID, UINT8 ChipSet);
static void SetPanningData(UINT32 Slider, UINT16 Value, bool NoRefresh);
void Dialogue_TrackChange(void);


extern UINT32 VGMMaxLoop;
extern UINT32 VGMPbRate;
extern UINT32 FadeTime;

extern float VolumeLevel;
extern bool SurroundSound;
extern bool FadeRAWLog;

extern UINT8 ResampleMode;
extern UINT8 CHIP_SAMPLING_MODE;

extern CHIPS_OPTION ChipOpts[0x02];

extern VGM_HEADER VGMHead;
extern UINT8 PlayingMode;


extern HANDLE hPluginInst;

static HWND hWndCfgTab[NUM_CFG_TABS];
static bool LoopTimeMode;
static UINT8 MuteChipID = 0x00;
static UINT8 MuteChipSet = 0x00;
static CHIP_OPTS* MuteCOpts;

void InitConfigDialog(HWND hWndMain)
{
	HWND TabCtrlWnd;
	RECT TabDispRect;
	RECT TabRect;
	HIMAGELIST hImgList;
	unsigned int CurTab;
	
	TabCtrlWnd = GetDlgItem(hWndMain, TabCollection);
	InitCommonControls();
	
	// Load images for tabs
	hImgList = ImageList_LoadImage(hPluginInst, (LPCSTR)TabIcons, TAB_ICON_WIDTH, 0,
									TAB_ICON_TRANSP, IMAGE_BITMAP, LR_CREATEDIBSECTION);
	TabCtrl_SetImageList(TabCtrlWnd, hImgList);
	
	// Add tabs
	AddTab(TabCtrlWnd, 0, "Playback");
	AddTab(TabCtrlWnd, 1, "Tags");
#ifndef HIDE_VGM7Z
	AddTab(TabCtrlWnd, 2, "VGM7z");
#endif
	AddTab(TabCtrlWnd, 3, "Muting");
	AddTab(TabCtrlWnd, 5, "Other Opts");
	
	// Get display rect
	GetWindowRect(TabCtrlWnd, &TabDispRect);
	GetWindowRect(hWndMain, &TabRect);
	OffsetRect(&TabDispRect, -TabRect.left - GetSystemMetrics(SM_CXDLGFRAME),
							-TabRect.top - GetSystemMetrics(SM_CYDLGFRAME) -
																GetSystemMetrics(SM_CYCAPTION));
	TabCtrl_AdjustRect(TabCtrlWnd, FALSE, &TabDispRect);
	
	// Create child windows
	CfgPlayback	= CREATE_CHILD(DlgCfgPlayback,	CfgDlgPlaybackProc);
	CfgTags		= CREATE_CHILD(DlgCfgTags,		CfgDlgTagsProc);
	Cfg7z		= CREATE_CHILD(DlgCfgVgm7z,		CfgDlgChildProc);
	CfgMuting	= CREATE_CHILD(DlgCfgMuting,	CfgDlgMutingProc);
	CfgOptPan	= CREATE_CHILD(DlgCfgOptPan,	CfgDlgOptPanProc);
	
	EnableWinXPVisualStyles(hWndMain);
	
	// position tabs
	TabDispRect.right -= TabDispRect.left;	// .right gets Tab Width
	TabDispRect.bottom -= TabDispRect.top;	// .bottom gets Tab Height
	for (CurTab = 0; CurTab < NUM_CFG_TABS; CurTab ++)
	{
		SetWindowPos(hWndCfgTab[CurTab], HWND_TOP, TabDispRect.left, TabDispRect.top,
					TabDispRect.right, TabDispRect.bottom, SWP_NOZORDER);
	}
	
	// show first tab, hide the others
	ShowWindow(hWndCfgTab[0], SW_SHOW);
	for (CurTab = 1; CurTab < NUM_CFG_TABS; CurTab ++)
		ShowWindow(hWndCfgTab[CurTab], SW_HIDE);
	
	return;
}

INLINE void AddTab(HWND tabCtrlWnd, int ImgIndex, char* TabTitle)
{
	TC_ITEM newTab;
	int tabIndex;
	
	tabIndex = TabCtrl_GetItemCount(tabCtrlWnd);
	newTab.mask = TCIF_TEXT;
	newTab.mask |= (ImgIndex >= 0) ? TCIF_IMAGE : 0;
	newTab.pszText = TabTitle;
	newTab.iImage = ImgIndex;
	TabCtrl_InsertItem(tabCtrlWnd, tabIndex, &newTab);
	
	return;
}

static void EnableWinXPVisualStyles(HWND hWndMain)
{
	HINSTANCE dllinst;
	FARPROC EnThemeDlgTex;
	FARPROC ThemeDlgTexIsEn;
	unsigned int CurWnd;
	
	dllinst = LoadLibrary("uxtheme.dll");
	if (dllinst == NULL)
		return;
	
	EnThemeDlgTex = GetProcAddress(dllinst, "EnableThemeDialogTexture");
	ThemeDlgTexIsEn = GetProcAddress(dllinst, "IsThemeDialogTextureEnabled");
	if (ThemeDlgTexIsEn == NULL || EnThemeDlgTex == NULL)
		goto CancelXPStyles;
	
	if (ThemeDlgTexIsEn(hWndMain))
	{ 
#ifndef ETDT_ENABLETAB
#define ETDT_ENABLETAB	6
#endif
		
		// draw pages with theme texture
		for (CurWnd = 0; CurWnd < NUM_CFG_TABS; CurWnd ++)
			EnThemeDlgTex(hWndCfgTab[CurWnd], ETDT_ENABLETAB);
	}
	
CancelXPStyles:
	FreeLibrary(dllinst);
	
	return;
}


static void Slider_Setup(HWND hWndDlg, int DlgID, int Min, int Max, int BigStep, int TickFreq)
{
	LONG RetVal;
	
	RetVal = SendDlgItemMessage(hWndDlg, DlgID, TBM_SETRANGE, 0, MAKELONG(Min, Max));
	// Note to TBM_SETTICFREQ:
	//  Needs Automatic Ticks enabled, draw a tick mark every x ticks.
	RetVal = SendDlgItemMessage(hWndDlg, DlgID, TBM_SETTICFREQ, TickFreq, 0);
	RetVal = SendDlgItemMessage(hWndDlg, DlgID, TBM_SETLINESIZE, 0, 1);
	RetVal = SendDlgItemMessage(hWndDlg, DlgID, TBM_SETPAGESIZE, 0, BigStep);
	
	return;
}

static BOOL SetDlgItemFloat(HWND hDlg, int nIDDlgItem, double Value, int Precision)
{
	char TempStr[0x10];
	
	sprintf(TempStr, "%.*f", Precision, Value);
	return SetDlgItemText(hDlg, nIDDlgItem, TempStr);
}

static int LoadConfigDialogInfo(HWND hWndDlg)
{
	UINT8 CurChp;
	float dbVol;
	INT32 TempSLng;
	char TempStr[0x18];
	const char* TempPnt;
	
	// --- Main Dialog ---
	CheckDlgButton(hWndDlg, ImmediateUpdCheck,
					Options.ImmediateUpdate ? BST_CHECKED : BST_UNCHECKED);
	
	// --- Playback Tab ---
	COMBO_ADDSTR(CfgPlayback, ResmpModeList, "HQ resampling");
	COMBO_ADDSTR(CfgPlayback, ResmpModeList, "HQ up, LQ down");
	COMBO_ADDSTR(CfgPlayback, ResmpModeList, "LQ resampling");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "native");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "highest (nat./cust.)");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "custom");
	COMBO_ADDSTR(CfgPlayback, ChipSmpModeList, "highest, FM native");
	
	LoopTimeMode = false;
	SetDlgItemInt(CfgPlayback, LoopText, VGMMaxLoop, FALSE);
	SetDlgItemInt(CfgPlayback, FadeText, FadeTime, FALSE );
	SetDlgItemInt(CfgPlayback, PauseNlText, Options.PauseNL, FALSE);
	SetDlgItemInt(CfgPlayback, PauseLpText, Options.PauseLp, FALSE);
	
	// Playback rate
	switch(VGMPbRate)
	{
	case 0:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, RateRecRadio);
		break;
	case 60:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, Rate60HzRadio);
		break;
	case 50:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, Rate50HzRadio);
		break;
	default:
		CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, RateOtherRadio);
		break;
	}
	CTRL_SET_ENABLE(CfgPlayback, RateText, CHECK2BOOL(CfgPlayback, RateOtherRadio));
	CTRL_SET_ENABLE(CfgPlayback, RateLabel, CHECK2BOOL(CfgPlayback, RateOtherRadio));
	SetDlgItemInt(CfgPlayback, RateText, VGMPbRate, FALSE);
	
	// Slider from -12.0 db to +12.0 db, 0.1 db Steps (large Steps 2.0 db)
	Slider_Setup(CfgPlayback, VolumeSlider, 0, 240, 20, 10);
	dbVol = (float)(6.0 * log(VolumeLevel) / log(2.0));
	TempSLng = (INT32)(floor(dbVol * 10 + 0.5)) + 120;
	if (TempSLng < 0)
		TempSLng = 0;
	else if (TempSLng > 240)
		TempSLng = 240;
	SLIDER_SETPOS(CfgPlayback, VolumeSlider, TempSLng);
	
	sprintf(TempStr, "%.3f (%+.1f db)", VolumeLevel, dbVol);
	SetDlgItemText(CfgPlayback, VolumeText, TempStr);
	
	SetDlgItemInt(CfgPlayback, SmplPbRateText, Options.SampleRate, FALSE);
	COMBO_SETPOS(CfgPlayback, ResmpModeList, ResampleMode);
	SetDlgItemInt(CfgPlayback, SmplChipRateText, Options.ChipRate, FALSE);
	COMBO_SETPOS(CfgPlayback, ChipSmpModeList, CHIP_SAMPLING_MODE);
	
	CheckDlgButton(CfgPlayback, SurroundCheck, SurroundSound ? BST_CHECKED : BST_UNCHECKED);
	
	/*// Filter
	switch(filter_type)
	{
	case FILTER_NONE:
		CheckRadioButton(CfgPlayback, rbFilterNone, rbFilterWeighted, rbFilterNone);
		break;
	case FILTER_LOWPASS:
		CheckRadioButton(CfgPlayback, rbFilterNone, rbFilterWeighted, rbFilterLowPass);
		break;
	case FILTER_WEIGHTED:
		CheckRadioButton(CfgPlayback, rbFilterNone, rbFilterWeighted, rbFilterWeighted);
		break;
	}*/
	
	// --- Tags Tab ---
	SetDlgItemText(CfgTags, TitleFormatText, Options.TitleFormat);
	
	CheckDlgButton(CfgTags, PreferJapCheck, Options.JapTags);
	CheckDlgButton(CfgTags, FM2413Check, Options.AppendFM2413);
	CheckDlgButton(CfgTags, TrimWhitespcCheck, Options.TrimWhitespc);
	CheckDlgButton(CfgTags, StdSeparatorsCheck, Options.StdSeparators);
	CheckDlgButton(CfgTags, TagFallbackCheck, Options.TagFallback);
	CheckDlgButton(CfgTags, NoInfoCacheCheck, Options.NoInfoCache);
	//CheckDlgButton(CfgTags, cbTagsStandardiseDates, TagsStandardiseDates);
	
	SetDlgItemInt(CfgTags, MLTypeText, Options.MLFileType, FALSE);
	
	// --- Muting Tab and Options & Pan Tab ---
	for (CurChp = 0x00; CurChp < CHIP_COUNT; CurChp ++)
	{
		TempPnt = GetAccurateChipName(CurChp, 0xFF);
		COMBO_ADDSTR(CfgMuting, MutingChipList, TempPnt);
		COMBO_ADDSTR(CfgOptPan, EmuOptChipList, TempPnt);
	}
	COMBO_ADDSTR(CfgMuting, MutingChipNumList, "Chip #1");
	COMBO_ADDSTR(CfgOptPan, EmuOptChipNumList, "Chip #1");
	COMBO_ADDSTR(CfgMuting, MutingChipNumList, "Chip #2");
	COMBO_ADDSTR(CfgOptPan, EmuOptChipNumList, "Chip #2");
	
	COMBO_SETPOS(CfgMuting, MutingChipList, MuteChipID);
	COMBO_SETPOS(CfgOptPan, EmuOptChipList, MuteChipID);
	COMBO_SETPOS(CfgMuting, MutingChipNumList, MuteChipSet);
	COMBO_SETPOS(CfgOptPan, EmuOptChipNumList, MuteChipSet);
	
	CheckDlgButton(CfgMuting, ResetMuteCheck, Options.ResetMuting);
	
	for (TempSLng = PanChn1Slider; TempSLng <= PanChn15Slider; TempSLng ++)
		Slider_Setup(CfgOptPan, TempSLng, 0x00, 0x40, 0x08, 0x08);
	
	return 0;
}
 

// Dialogue box callback function
BOOL CALLBACK ConfigDialogProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		// do nothing if this is a child window (tab page) callback, pass to the parent
		if (GetWindowLong(hWndDlg, GWL_STYLE) & WS_CHILD)
			assert(false);
			//return FALSE;
		
		InitConfigDialog(hWndDlg);
		
		SetWindowText(hWndDlg, INVGM_TITLE " configuration");
		
		LoadConfigDialogInfo(hWndDlg);
		
		SendMessage(hWndDlg, WM_HSCROLL, 0, 0); // trigger slider value change handlers
		SendMessage(CfgPlayback, WM_COMMAND, MAKEWPARAM(ResmpModeList, CBN_SELCHANGE), 0);
		SendMessage(CfgPlayback, WM_COMMAND, MAKEWPARAM(ChipSmpModeList, CBN_SELCHANGE), 0);
		SendMessage(CfgMuting, WM_COMMAND, MAKEWPARAM(MutingChipList, CBN_SELCHANGE), 0);
		SendMessage(CfgOptPan, WM_COMMAND, MAKEWPARAM(EmuOptChipList, CBN_SELCHANGE), 0);
		
		SetFocus(hWndDlg);
		
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			{
				UINT8 CurTab;
				
				for (CurTab = 0; CurTab < NUM_CFG_TABS; CurTab ++)
					SendMessage(hWndCfgTab[CurTab], WM_COMMAND, IDOK, lParam);
			}
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:	// [X] button, Alt+F4, etc
			{
				UINT8 CurTab;
				
				for (CurTab = 0; CurTab < NUM_CFG_TABS; CurTab ++)
					SendMessage(hWndCfgTab[CurTab], WM_COMMAND, IDCANCEL, lParam);
			}
			
			EndDialog(hWndDlg, 1);
			return TRUE;

/*		// Stuff not in tabs --------------------------------------------------------
		case btnReadMe: {
				char FileName[MAX_PATH];
				char *PChar;
				GetModuleFileName(hPluginInst,FileName,MAX_PATH);  // get *dll* path
				GetFullPathName(FileName,MAX_PATH,FileName,&PChar);  // make it fully qualified plus find the filename bit
				strcpy(PChar,"in_vgm.html");
				if ((int)ShellExecute(mod.hMainWindow,NULL,FileName,NULL,NULL,SW_SHOWNORMAL)<=32)
					MessageBox(hWndDlg,"Error opening in_vgm.html from plugin folder",mod.description,MB_ICONERROR+MB_OK);
			}
			break;*/
		case ImmediateUpdCheck:
			if (HIWORD(wParam) == BN_CLICKED)
				Options.ImmediateUpdate = CHECK2BOOL(hWndDlg, ImmediateUpdCheck);
			break;
		}
		break; // end WM_COMMAND handling

	case WM_NOTIFY:
		switch(LOWORD(wParam))
		{
		case TabCollection:
		{
			int CurTab;
			
			CurTab = TabCtrl_GetCurSel(GetDlgItem(hWndDlg, TabCollection));
			if (CurTab == -1)
				break;
#ifdef HIDE_VGM7Z
			if (CurTab >= 2)
				CurTab ++;	// skip hidden VGM7Z tab
#endif
			
			switch(((LPNMHDR)lParam)->code)
			{
			case TCN_SELCHANGING:	// hide current window
				ShowWindow(hWndCfgTab[CurTab], SW_HIDE);
				EnableWindow(hWndCfgTab[CurTab], FALSE);
				break;
			case TCN_SELCHANGE:		// show current window
				if (hWndCfgTab[CurTab] == CfgMuting)
				{
					COMBO_SETPOS(CfgMuting, MutingChipList, MuteChipID);
					COMBO_SETPOS(CfgMuting, MutingChipNumList, MuteChipSet);
					ShowMutingCheckBoxes(MuteChipID, MuteChipSet);
				}
				else if (hWndCfgTab[CurTab] == CfgOptPan)
				{
					COMBO_SETPOS(CfgOptPan, EmuOptChipList, MuteChipID);
					COMBO_SETPOS(CfgOptPan, EmuOptChipNumList, MuteChipSet);
					ShowOptPanBoxes(MuteChipID, MuteChipSet);
				}
				EnableWindow(hWndCfgTab[CurTab], TRUE);
				ShowWindow(hWndCfgTab[CurTab], SW_SHOW);
				break;
			}
			break;
		}
		}
		return TRUE;
	}
	
	return FALSE;	// message not processed
}

BOOL CALLBACK CfgDlgPlaybackProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			VGMMaxLoop = GetDlgItemInt(CfgPlayback, LoopText, NULL, FALSE);
			FadeTime = GetDlgItemInt(CfgPlayback, FadeText, NULL, FALSE);
			Options.PauseNL = GetDlgItemInt(CfgPlayback, PauseNlText, NULL, FALSE);
			Options.PauseLp = GetDlgItemInt(CfgPlayback, PauseLpText, NULL, FALSE);
			
			if (IsDlgButtonChecked(CfgPlayback, RateRecRadio))
				VGMPbRate = 0;
			else if (IsDlgButtonChecked(CfgPlayback, Rate60HzRadio))
				VGMPbRate = 60;
			else if (IsDlgButtonChecked(CfgPlayback, Rate50HzRadio))
				VGMPbRate = 50;
			else if (IsDlgButtonChecked(CfgPlayback, RateOtherRadio))
				VGMPbRate = GetDlgItemInt(CfgPlayback, RateText, NULL, FALSE);
			
			Options.SampleRate = GetDlgItemInt(CfgPlayback, SmplPbRateText, NULL, FALSE);
			ResampleMode = (UINT8)COMBO_GETPOS(CfgPlayback, ResmpModeList);
			Options.ChipRate = GetDlgItemInt(CfgPlayback, SmplChipRateText, NULL, FALSE);
			CHIP_SAMPLING_MODE = (UINT8)COMBO_GETPOS(CfgPlayback, ChipSmpModeList);
			
			QueueInfoReload();	// if the Playback Rate was changed, a refresh is needed
			
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case RateRecRadio:
		case Rate60HzRadio:
		case Rate50HzRadio:
		case RateOtherRadio:
			// Windows already does the CheckRadioButton by itself
			//CheckRadioButton(CfgPlayback, RateRecRadio, RateOtherRadio, LOWORD(wParam));
			if (LOWORD(wParam) == RateOtherRadio)
			{
				CTRL_ENABLE(CfgPlayback, RateText);
				CTRL_ENABLE(CfgPlayback, RateLabel);
				//SetFocus(GetDlgItem(CfgPlayback, RateText));
			}
			else
			{
				CTRL_DISABLE(CfgPlayback, RateText);
				CTRL_DISABLE(CfgPlayback, RateLabel);
			}
			break;
		case ResmpModeList:
			break;
		case ChipSmpModeList:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				BOOL CtrlEnable;
				
				CtrlEnable = (COMBO_GETPOS(CfgPlayback, ChipSmpModeList) > 0);
				CTRL_SET_ENABLE(CfgPlayback, SmplChipRateLabel, CtrlEnable);
				CTRL_SET_ENABLE(CfgPlayback, SmplChipRateText, CtrlEnable);
				CTRL_SET_ENABLE(CfgPlayback, SmplChipRateHzLabel, CtrlEnable);
			}
			break;
		case LoopText:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				// for correct grammar: "Play loop x time(s)"
				BOOL Translated;
				bool NewLTMode;
				
				NewLTMode = (GetDlgItemInt(CfgPlayback, LoopText, &Translated, FALSE) == 1);
				if (Translated && NewLTMode != LoopTimeMode)
				{
					char TimeStr[0x08];
					
					LoopTimeMode = NewLTMode;
					strcpy(TimeStr, "times");
					if (LoopTimeMode)
						TimeStr[0x04] = '\0';
					SetDlgItemText(CfgPlayback, LoopTimesLabel, TimeStr);
				}
			}
			break;
		case SurroundCheck:
			SurroundSound = CHECK2BOOL(CfgPlayback, SurroundCheck);
			
			UpdatePlayback();
			break;
		}
		break;
	case WM_HSCROLL:
		if (GetDlgCtrlID((HWND)lParam) == VolumeSlider)
		{
			float dbVol;
			UINT16 TempSht;
			char TempStr[0x18];
			
			if (LOWORD(wParam) == TB_THUMBPOSITION || LOWORD(wParam) == TB_THUMBTRACK)
				TempSht = HIWORD(wParam);
			else
				TempSht = SLIDER_GETPOS(CfgPlayback, VolumeSlider);
			dbVol = (TempSht - 120) / 10.0f;
			VolumeLevel = (float)pow(2.0, dbVol / 6.0);
			
			sprintf(TempStr, "%.3f (%+.1f db)", VolumeLevel, dbVol);
			SetDlgItemText(CfgPlayback, VolumeText, TempStr);
			
			RefreshPlaybackOptions();
			if (LOWORD(wParam) != TB_THUMBTRACK)	// prevent too many skips
				UpdatePlayback();
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

BOOL CALLBACK CfgDlgTagsProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			GetDlgItemText(CfgTags, TitleFormatText, Options.TitleFormat, 0x7F);
			
			//TagsStandardiseDates = CHECK2BOOL(CfgTags, cbTagsStandardiseDates, );
			
			SetDlgItemInt(CfgTags, MLTypeText, Options.MLFileType, FALSE);
			
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case PreferJapCheck:
			Options.JapTags = CHECK2BOOL(CfgTags, PreferJapCheck);
			break;
		case FM2413Check:
			Options.AppendFM2413 = CHECK2BOOL(CfgTags, FM2413Check);
			break;
		case TrimWhitespcCheck:
			Options.TrimWhitespc = CHECK2BOOL(CfgTags, TrimWhitespcCheck);
			QueueInfoReload();
			break;
		case StdSeparatorsCheck:
			Options.StdSeparators = CHECK2BOOL(CfgTags, StdSeparatorsCheck);
			QueueInfoReload();
			break;
		case TagFallbackCheck:
			Options.TagFallback = CHECK2BOOL(CfgTags, TagFallbackCheck);
			break;
		case NoInfoCacheCheck:
			Options.NoInfoCache = CHECK2BOOL(CfgTags, NoInfoCacheCheck);
			break;
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

BOOL CALLBACK CfgDlgMutingProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case MutingChipList:
		case MutingChipNumList:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				ShowMutingCheckBoxes((UINT8)COMBO_GETPOS(CfgMuting, MutingChipList),
									(UINT8)COMBO_GETPOS(CfgMuting, MutingChipNumList));
			}
			break;
		case ResetMuteCheck:
			if (HIWORD(wParam) == BN_CLICKED)
				Options.ResetMuting = CHECK2BOOL(CfgMuting, ResetMuteCheck);
			break;
		case MuteChipCheck:
			MuteCOpts->Disabled = CHECK2BOOL(CfgMuting, MuteChipCheck);
			
			UpdatePlayback();
			break;
		}
		// I'm NOT going to have 24 case-lines!
		if (LOWORD(wParam) >= MuteChn1Check && LOWORD(wParam) <= MuteChn24Check)
		{
			if (HIWORD(wParam) == BN_CLICKED)
			{
				SetMutingData(LOWORD(wParam) - MuteChn1Check,
								(bool)CHECK2BOOL(CfgMuting, LOWORD(wParam)));
			}
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

BOOL CALLBACK CfgDlgOptPanProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		case EmuCoreRadio1:
		case EmuCoreRadio2:
		{
			CHIP_OPTS* TempCOpts;
			
			// EmuCore is only available for Chip #1
			TempCOpts = (CHIP_OPTS*)&ChipOpts[0x00] + MuteChipID;
			if (LOWORD(wParam) == EmuCoreRadio1)
			{
				TempCOpts->EmuCore = 0x00;
			}
			else //if (LOWORD(wParam) == EmuCoreRadio2)
			{
				TempCOpts->EmuCore = 0x01;
			}
			RefreshPlaybackOptions();
			ShowOptPanBoxes(MuteChipID, MuteChipSet);
			break;
		}
		case EmuOptChipList:
		case EmuOptChipNumList:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				ShowOptPanBoxes((UINT8)COMBO_GETPOS(CfgOptPan, EmuOptChipList),
								(UINT8)COMBO_GETPOS(CfgOptPan, EmuOptChipNumList));
			}
			break;
		}
		break;
	case WM_HSCROLL:
	{
		int SliderID;
		
		SliderID = GetDlgCtrlID((HWND)lParam);
		if (SliderID >= PanChn1Slider && SliderID <= PanChn15Slider)
		{
			UINT16 TempSht;
			bool Dragging;
			
			Dragging = (LOWORD(wParam) == TB_THUMBTRACK);
			if (Dragging || LOWORD(wParam) == TB_THUMBPOSITION)
				TempSht = HIWORD(wParam);
			else
				TempSht = SLIDER_GETPOS(CfgOptPan, SliderID);
			
			SetPanningData(SliderID - PanChn1Slider, TempSht, Dragging);
		}
		break;
	}
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}

BOOL CALLBACK CfgDlgChildProc(HWND hWndDlg, UINT wMessage, WPARAM wParam, LPARAM lParam)
{
	// Generic Procedure for unused/unfinished tabs
	switch(wMessage)
	{
	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWndDlg, 1);
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		break;
	}
	
	return FALSE;
}


static bool IsChipAvailable(UINT8 ChipID, UINT8 ChipSet)
{
	UINT32 Clock;
	
	if (PlayingMode == 0xFF)
		return true;
	
	Clock = GetChipClock(&VGMHead, (ChipSet << 7) | ChipID, NULL);
	
	if (! Clock)
		return false;
	else
		return true;
}

static void ShowMutingCheckBoxes(UINT8 ChipID, UINT8 ChipSet)
{
	UINT8 ChnCount;
	UINT8 ChnCntS[0x04];
	const char* SpcName[0x40];	// Special Channel Names
	UINT8 CurChn;
	UINT8 SpcModes;
	bool EnableChk;
	bool Checked;
	UINT8 CurMode;
	UINT8 ChnBase;
	UINT8 FnlChn;
	char TempName[0x18];
	
	for (CurChn = 0x00; CurChn < 0x40; CurChn ++)
		SpcName[CurChn] = NULL;
	
	EnableChk = IsChipAvailable(ChipID, ChipSet);
	SpcModes = 0;
	switch(ChipID)
	{
	case 0x00:	// SN76496
		ChnCount = 4;
		SpcName[3] = "&Noise";
		break;
	case 0x01:	// YM2413
	case 0x09:	// YM3812
	case 0x0A:	// YM3526
	case 0x0B:	// Y8950
		ChnCount = 14;	// 9 + 5
		SpcName[ 9] = "&Bass Drum";
		SpcName[10] = "S&nare Drum";
		SpcName[11] = "&Tom Tom";
		SpcName[12] = "C&ymbal";
		SpcName[13] = "&Hi-Hat";
		if (ChipID == 0x0B)
		{
			ChnCount = 15;
			SpcName[14] = "&Delta-T";
		}
		break;
	case 0x02:	// YM2612
		ChnCount = 7;	// 6 + DAC
		SpcName[6] = "&DAC";
		break;
	case 0x03:	// YM2151
		ChnCount = 8;
		break;
	case 0x04:	// Sega PCM
		ChnCount = 16;
		EnableChk &= ! ChipSet;
		break;
	case 0x05:	// RF5C68
	case 0x10:	// RF5C164
		ChnCount = 8;
		EnableChk &= ! ChipSet;
		break;
	case 0x06:	// YM2203
		ChnCount = 6;	// 3 FM + 3 AY8910
		SpcModes = 3;
		ChnCntS[0] = 3;
		SpcName[0] = "FM Chn";
		ChnCntS[1] = 0;
		ChnCntS[2] = 3;
		SpcName[2] = "PSG Chn";
		break;
	case 0x07:	// YM2608
	case 0x08:	// YM2610
		ChnCount = 16;	// 6 FM + 6 ADPCM + 1 DeltaT + 3 AY8910
		SpcModes = 3;
		ChnCntS[0] = 6;
		SpcName[0] = "FM Chn";
		ChnCntS[1] = 7;
		SpcName[1] = "PCM Chn";
		SpcName[14] = "&Delta-T";
		ChnCntS[2] = 3;
		SpcName[2] = "PSG Chn";
		break;
	case 0x0C:	// YMF262
		ChnCount = 23;	// 18 + 5
		SpcName[18] = "&Bass Drum";
		SpcName[19] = "S&nare Drum";
		SpcName[20] = "&Tom Tom";
		SpcName[21] = "C&ymbal";
		SpcName[22] = "&Hi-Hat";
		break;
	case 0x0D:	// YMF278B
		ChnCount = 24;
		break;
	case 0x0E:	// YMF271
		ChnCount = 12;
		break;
	case 0x0F:	// YMZ280B
		ChnCount = 8;
		break;
	case 0x11:	// PWM
		ChnCount = 0;
		EnableChk &= ! ChipSet;
		break;
	case 0x12:	// AY8910
		ChnCount = 3;
		break;
	case 0x13:	// GB DMG
		SpcName[0] = "Square &1";
		SpcName[1] = "Square &2";
		SpcName[2] = "Progr. &Wave";
		SpcName[3] = "&Noise";
		ChnCount = 4;
		break;
	case 0x14:	// NES APU
		SpcName[0] = "Square &1";
		SpcName[1] = "Square &2";
		SpcName[2] = "&Triangle";
		SpcName[3] = "&Noise";
		SpcName[4] = "&DPCM";
		SpcName[5] = "&FDS";
		ChnCount = 6;
		break;
	case 0x15:	// Multi PCM
		ChnCount = 28;
		break;
	case 0x16:	// UPD7759
		ChnCount = 0;
		break;
	case 0x17:	// OKIM6258
		ChnCount = 0;
		break;
	case 0x18:	// OKIM6295
		ChnCount = 4;
		break;
	case 0x19:	// K051649
		ChnCount = 5;
		break;
	case 0x1A:	// K054539
		ChnCount = 8;
		break;
	case 0x1B:	// HuC6280
		ChnCount = 6;
		break;
	case 0x1C:	// C140
		ChnCount = 24;
		break;
	case 0x1D:	// K053260
		ChnCount = 4;
		break;
	case 0x1E:	// Pokey
		ChnCount = 4;
		break;
	case 0x1F:	// Q-Sound
		ChnCount = 16;
		EnableChk &= ! ChipSet;
		break;
	case 0x20:	// SCSP
		ChnCount = 32;
		break;
	case 0x21:	// WonderSwan
		ChnCount = 4;
		break;
	case 0x22:	// VSU
		ChnCount = 6;
		break;
	case 0x23:	// SAA1099
		ChnCount = 6;
		break;
	case 0x24:	// ES5503
		ChnCount = 32;
		break;
	case 0x25:	// ES5506
		ChnCount = 32;
		break;
	case 0x26:	// X1-010
		ChnCount = 16;
		break;
	case 0x27:	// C352
		ChnCount = 32;
		break;
	case 0x28:	// GA20
		ChnCount = 4;
		break;
	default:
		ChnCount = 0;
		EnableChk &= ! ChipSet;
		break;
	}
	MuteCOpts = (CHIP_OPTS*)&ChipOpts[ChipSet] + ChipID;
	if (ChnCount > 24)
		ChnCount = 24;	// currently only 24 checkboxes are supported
	
	SETCHECKBOX(CfgMuting, MuteChipCheck, MuteCOpts->Disabled);
	CTRL_SET_ENABLE(CfgMuting, MuteChipCheck, EnableChk);
	if (! SpcModes)
	{
		for (CurChn = 0; CurChn < ChnCount; CurChn ++)
		{
			if (SpcName[CurChn] == NULL)
			{
				if (1 + CurChn < 10)
					sprintf(TempName, "Channel &%u", 1 + CurChn);
				else
					sprintf(TempName, "Channel %u (&%c)", 1 + CurChn, 'A' + (CurChn - 9));
				SetDlgItemText(CfgMuting, MuteChn1Check + CurChn, TempName);
			}
			else
			{
				SetDlgItemText(CfgMuting, MuteChn1Check + CurChn, SpcName[CurChn]);
			}
			
			if (ChipID == 0x0D)
				Checked = (MuteCOpts->ChnMute2 >> CurChn) & 0x01;
			else
				Checked = (MuteCOpts->ChnMute1 >> CurChn) & 0x01;
			SETCHECKBOX(CfgMuting, MuteChn1Check + CurChn, Checked);
			CTRL_SET_ENABLE(CfgMuting, MuteChn1Check + CurChn, EnableChk);
			CTRL_SHOW(CfgMuting, MuteChn1Check + CurChn);
		}
	}
	else
	{
		for (CurMode = 0; CurMode < SpcModes; CurMode ++)
		{
			ChnBase = CurMode * 8;
			if (SpcName[CurMode] == NULL)
				SpcName[CurMode] = "Channel";
			
			for (CurChn = 0, FnlChn = ChnBase; CurChn < ChnCntS[CurMode]; CurChn ++, FnlChn ++)
			{
				if (FnlChn < SpcModes || SpcName[FnlChn] == NULL)
				{
					if (1 + CurChn < 10)
						sprintf(TempName, "%s &%u", SpcName[CurMode], 1 + CurChn);
					else
						sprintf(TempName, "%s %u (&%c)", SpcName[CurMode], 1 + CurChn,
								'A' + (CurChn - 9));
					SetDlgItemText(CfgMuting, MuteChn1Check + FnlChn, TempName);
				}
				else
				{
					SetDlgItemText(CfgMuting, MuteChn1Check + FnlChn, SpcName[FnlChn]);
				}
				
				switch(CurMode)
				{
				case 0:
					Checked = (MuteCOpts->ChnMute1 >> CurChn) & 0x01;
					break;
				case 1:
					Checked = (MuteCOpts->ChnMute2 >> CurChn) & 0x01;
					break;
				case 2:
					Checked = (MuteCOpts->ChnMute3 >> CurChn) & 0x01;
					break;
				}
				SETCHECKBOX(CfgMuting, MuteChn1Check + FnlChn, Checked);
				CTRL_SET_ENABLE(CfgMuting, MuteChn1Check + FnlChn, EnableChk);
				CTRL_SHOW(CfgMuting, MuteChn1Check + FnlChn);
			}
			for (; CurChn < 8; CurChn ++, FnlChn ++)
			{
				CTRL_HIDE(CfgMuting, MuteChn1Check + FnlChn);
				CTRL_DISABLE(CfgMuting, MuteChn1Check + FnlChn);
				SetDlgItemText(CfgMuting, MuteChn1Check + FnlChn, "");
			}
		}
		CurChn = FnlChn;
	}
	for (; CurChn < 24; CurChn ++)
	{
		// I thought that disabling a window should prevent it from catching other
		// windows' keyboard shortcuts.
		// But NO, it seems that you have to make its text EMPTY to make it work!
		CTRL_HIDE(CfgMuting, MuteChn1Check + CurChn);
		CTRL_DISABLE(CfgMuting, MuteChn1Check + CurChn);
		SetDlgItemText(CfgMuting, MuteChn1Check + CurChn, "");
	}
	
	MuteChipID = ChipID;
	MuteChipSet = ChipSet;
	
	return;
}

static void SetMutingData(UINT32 CheckBox, bool MuteOn)
{
	//UINT8 ChnCount;
	//UINT8 ChnCntS[0x04];
	UINT8 CurChn;
	UINT8 SpcModes;
	UINT8 CurMode;
	
	SpcModes = 0;
	switch(MuteChipID)
	{
	case 0x06:	// YM2203
		//ChnCount = 6;	// 3 FM + 3 AY8910
		SpcModes = 2;
		//ChnCntS[0] = 3;
		//ChnCntS[1] = 3;
		break;
	case 0x07:	// YM2608
	case 0x08:	// YM2610
		//ChnCount = 16;	// 6 FM + 6 ADPCM + 1 DeltaT + 3 AY8910
		SpcModes = 3;
		//ChnCntS[0] = 6;
		//ChnCntS[1] = 7;
		//ChnCntS[2] = 3;
		break;
	}
	
	if (! SpcModes)
	{
		CurMode = 0;
		if (MuteChipID == 0x0D)
			CurMode = 1;
		CurChn = CheckBox;
	}
	else
	{
		CurMode = CheckBox / 8;
		CurChn = CheckBox % 8;
	}
	switch(CurMode)
	{
	case 0:
		MuteCOpts->ChnMute1 &= ~(1 << CurChn);
		MuteCOpts->ChnMute1 |= (MuteOn << CurChn);
		break;
	case 1:
		MuteCOpts->ChnMute2 &= ~(1 << CurChn);
		MuteCOpts->ChnMute2 |= (MuteOn << CurChn);
		break;
	case 2:
		MuteCOpts->ChnMute3 &= ~(1 << CurChn);
		MuteCOpts->ChnMute3 |= (MuteOn << CurChn);
		break;
	}
	
	RefreshMuting();
	UpdatePlayback();
	
	return;
}

static void ShowOptPanBoxes(UINT8 ChipID, UINT8 ChipSet)
{
	UINT8 ChnCount;
	UINT8 CurChn;
	const char* SpcName[0x20];	// Special Channel Names
	const char* CoreName[0x02];	// Names for the Emulation Cores
	bool EnableChk;
	bool MultiCore;
	bool CanPan;
	INT16 PanPos;
	char TempName[0x20];
	const char* TempStr;
	CHIP_OPTS* TempCOpts;	// for getting EmuCore only
	
	for (CurChn = 0x00; CurChn < 0x20; CurChn ++)
		SpcName[CurChn] = NULL;
	
	CoreName[0x00] = "MAME";
	CoreName[0x01] = NULL;
	
	// ChipOpts[0x01] contains the EmuCore that's currently used
	TempCOpts = (CHIP_OPTS*)&ChipOpts[0x01] + ChipID;
	EnableChk = IsChipAvailable(ChipID, ChipSet);
	MultiCore = false;
	CanPan = false;
	ChnCount = 0;
	switch(ChipID)
	{
	case 0x00:	// SN76496
		MultiCore = true;
		CoreName[0x01] = "Maxim";
		CanPan = (TempCOpts->EmuCore == 0x01);
		
		ChnCount = 4;
		SpcName[3] = "&N";
		break;
	case 0x01:	// YM2413
		MultiCore = true;
		CoreName[0x00] = "EMU2413";
		CoreName[0x01] = "MAME";
		CanPan = (TempCOpts->EmuCore == 0x00);
		
		ChnCount = 14;	// 9 + 5
		SpcName[ 9] = "&BD";
		SpcName[10] = "S&D";
		SpcName[11] = "&TT";
		SpcName[12] = "C&Y";
		SpcName[13] = "&HH";
		break;
	case 0x02:	// YM2612
		MultiCore = true;
		CoreName[0x01] = "Nuked OPN2";
		//CoreName[0x02] = "Gens";
		break;
	case 0x06:	// YM2203
	case 0x07:	// YM2608
	case 0x08:	// YM2610
		MultiCore = true;
		CoreName[0x00] = "EMU2149";
		CoreName[0x01] = "MAME";
		break;
	/*case 0x06:	// YM2203
		ChnCount = 6;	// 3 FM + 3 AY8910
		SpcModes = 2;
		ChnCntS[0] = 3;
		SpcName[0] = "FM Chn";
		ChnCntS[1] = 3;
		SpcName[1] = "PSG Chn";
		break;*/
	case 0x09:	// YM3812
	case 0x0C:	// YMF262
		MultiCore = true;
		CoreName[0x00] = "AdLibEmu";
		CoreName[0x01] = "MAME";
		break;
	case 0x0D:	// YMF272B
		CoreName[0x00] = "openMSX";
		break;
	case 0x10:	// RF5C164
	case 0x11:	// PWM
		CoreName[0x00] = "Gens";
		break;
	case 0x12:	// AY8910
		MultiCore = true;
		CoreName[0x00] = "EMU2149";
		CoreName[0x01] = "MAME";
		break;
	case 0x14:	// NES APU
		MultiCore = true;
		CoreName[0x00] = "NSFPlay";
		CoreName[0x01] = "MAME";
		break;
	case 0x1B:	// HuC6280
		MultiCore = true;
		CoreName[0x00] = "Ootake";
		CoreName[0x01] = "MAME";
		break;
	case 0x1F:	// QSound
		MultiCore = true;
		CoreName[0x00] = "superctr";
		CoreName[0x01] = "MAME";
		break;
	case 0x27:	// C352
		CoreName[0x00] = "superctr";
		break;
	default:
		ChnCount = 0;
		EnableChk = false;
		break;
	}
	TempCOpts = (CHIP_OPTS*)&ChipOpts[0x00] + ChipID;
	MuteCOpts = (CHIP_OPTS*)&ChipOpts[ChipSet] + ChipID;
	if (! MuteCOpts->ChnCnt)
		CanPan = false;
	
	if (ChnCount > 15)
		ChnCount = 15;	// there are only 15 sliders
	
	for (CurChn = 0x00; CurChn < 0x02; CurChn ++)
	{
		CTRL_SET_ENABLE(CfgOptPan, EmuCoreRadio1 + CurChn, MultiCore && EnableChk);
		
		TempStr = (! CurChn) ? "Default" : "Alternative";
		if (CoreName[CurChn] == NULL)
			sprintf(TempName, "%s Core", TempStr);
		else
			sprintf(TempName, "%s (%s)", TempStr, CoreName[CurChn]);
		PanPos = strlen(TempName);
		SetDlgItemText(CfgOptPan, EmuCoreRadio1 + CurChn, TempName);
	}
	
	CheckRadioButton(CfgOptPan, EmuCoreRadio1, EmuCoreRadio2,
					EmuCoreRadio1 + TempCOpts->EmuCore);
	
	for (CurChn = 0; CurChn < ChnCount; CurChn ++)
	{
		if (SpcName[CurChn] == NULL)
		{
			if (1 + CurChn < 10)
				sprintf(TempName, "&%u", 1 + CurChn);
			else
				sprintf(TempName, "&%c", 'A' + (CurChn - 9));
			SetDlgItemText(CfgOptPan, PanChn1Label + CurChn, TempName);
		}
		else
		{
			SetDlgItemText(CfgOptPan, PanChn1Label + CurChn, SpcName[CurChn]);
		}
		CTRL_SET_ENABLE(CfgOptPan, PanChn1Label + CurChn, EnableChk && CanPan);
		
		PanPos = MuteCOpts->Panning[CurChn] / 0x08 + 0x20;
		SLIDER_SETPOS(CfgOptPan, PanChn1Slider + CurChn, PanPos);
		CTRL_SET_ENABLE(CfgOptPan, PanChn1Slider + CurChn, EnableChk && CanPan);
	}
	PanPos = 0x20;
	for (; CurChn < 15; CurChn ++)
	{
		if (1 + CurChn < 10)
			sprintf(TempName, "&%u", 1 + CurChn);
		else
			sprintf(TempName, "&%c", 'A' + (CurChn - 9));
		SetDlgItemText(CfgOptPan, PanChn1Label + CurChn, TempName);
		
		CTRL_DISABLE(CfgOptPan, PanChn1Label + CurChn);
		CTRL_DISABLE(CfgOptPan, PanChn1Slider + CurChn);
		SLIDER_SETPOS(CfgOptPan, PanChn1Slider + CurChn, PanPos);
	}
	
	MuteChipID = ChipID;
	MuteChipSet = ChipSet;
	
	return;
}

static void SetPanningData(UINT32 Slider, UINT16 Value, bool NoRefresh)
{
	if (Slider >= MuteCOpts->ChnCnt)
		return;
	
	MuteCOpts->Panning[Slider] = (Value - 0x20) * 0x08;
	
	RefreshPanning();
	if (! NoRefresh)
		UpdatePlayback();
	
	return;
}

void Dialogue_TrackChange(void)
{
	// redraw Muting Boxes and Panning Sliders
	ShowMutingCheckBoxes(MuteChipID, MuteChipSet);
	ShowOptPanBoxes(MuteChipID, MuteChipSet);
	
	return;
}
