# Microsoft Developer Studio Project File - Name="in_vgm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=in_vgm - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "in_vgm.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "in_vgm.mak" CFG="in_vgm - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "in_vgm - Win32 Release" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "in_vgm - Win32 Debug" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "in_vgm - Win32 Unicode Release" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "in_vgm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_VGM_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../VGMPlay" /I "../VGMPlay/zlib" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_VGM_EXPORTS" /D "VGM_LITTLE_ENDIAN" /D "ENABLE_ALL_CORES" /D "DISABLE_HW_SUPPORT" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 zlib.lib kernel32.lib user32.lib comctl32.lib advapi32.lib /nologo /dll /machine:I386 /libpath:"../VGMPlay/zlib"

!ELSEIF  "$(CFG)" == "in_vgm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_VGM_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "../VGMPlay" /I "../VGMPlay/zlib" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_VGM_EXPORTS" /D "VGM_LITTLE_ENDIAN" /D "ENABLE_ALL_CORES" /D "DISABLE_HW_SUPPORT" /D "UNICODE_INPUT_PLUGIN" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 zlib.lib kernel32.lib user32.lib comctl32.lib advapi32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept /libpath:"../VGMPlay/zlib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy  Debug\in_vgm.dll  D:\Programme\Winamp5_03a\Plugins\ 	copy  Debug\in_vgm.dll  xmplay36\ 
# End Special Build Tool

!ELSEIF  "$(CFG)" == "in_vgm - Win32 Unicode Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "in_vgm___Win32_Unicode_Release"
# PROP BASE Intermediate_Dir "in_vgm___Win32_Unicode_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseW"
# PROP Intermediate_Dir "ReleaseW"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /I "../VGMPlay" /I "../VGMPlay/zlib" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_VGM_EXPORTS" /D "VGM_LITTLE_ENDIAN" /D "ENABLE_ALL_CORES" /D "DISABLE_HW_SUPPORT" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../VGMPlay" /I "../VGMPlay/zlib" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_VGM_EXPORTS" /D "VGM_LITTLE_ENDIAN" /D "ENABLE_ALL_CORES" /D "DISABLE_HW_SUPPORT" /D "UNICODE_INPUT_PLUGIN" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 zlib.lib kernel32.lib user32.lib comctl32.lib advapi32.lib /nologo /dll /machine:I386 /libpath:"../VGMPlay/zlib"
# ADD LINK32 zlib.lib kernel32.lib user32.lib comctl32.lib advapi32.lib /nologo /dll /machine:I386 /out:"ReleaseW/in_vgm_w.dll" /libpath:"../VGMPlay/zlib"

!ENDIF 

# Begin Target

# Name "in_vgm - Win32 Release"
# Name "in_vgm - Win32 Debug"
# Name "in_vgm - Win32 Unicode Release"
# Begin Group "Source Files"

# PROP Default_Filter "c;def"
# Begin Source File

SOURCE=.\dlg_cfg.c
# End Source File
# Begin Source File

SOURCE=.\dlg_fileinfo.c
# End Source File
# Begin Source File

SOURCE=.\in_vgm.c
# End Source File
# Begin Source File

SOURCE=.\ini_func.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\pt_ioctl.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\VGMPlay.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\in_vgm.h
# End Source File
# Begin Source File

SOURCE=.\ini_func.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\PortTalk_IOCTL.h
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\stdbool.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\VGMFile.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\VGMPlay.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\VGMPlay_Intf.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\icon.ico
# End Source File
# Begin Source File

SOURCE=.\in_vgm.rc
# End Source File
# Begin Source File

SOURCE=.\images\logo.bmp
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\images\tabicons.bmp
# End Source File
# End Group
# Begin Group "Sound Core"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\VGMPlay\chips\2151intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2151intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2203intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2203intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2413intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2413intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2608intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2608intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2610intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2610intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2612intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\2612intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\262intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\262intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\3526intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\3526intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\3812intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\3812intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\8950intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\8950intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\adlibemu.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\adlibemu_opl2.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\adlibemu_opl3.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ay8910.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ay8910.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ay_intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ay_intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c140.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c140.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c352.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c352.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c6280.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c6280.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c6280intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\c6280intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ChipIncl.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\ChipMapper.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\ChipMapper.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\dac_control.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\dac_control.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\emu2149.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\emu2149.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\emu2413.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\emu2413.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\emutypes.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\es5503.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\es5503.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\es5506.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\es5506.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\fm.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\fm.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\fm2612.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\fmopl.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\fmopl.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\gb.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\gb.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\iremga20.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\iremga20.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\k051649.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\k051649.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\k053260.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\k053260.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\k054539.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\k054539.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\mamedef.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\multipcm.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\multipcm.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\nes_apu.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\nes_apu.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\nes_defs.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\nes_intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\nes_intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\np_nes_apu.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\np_nes_apu.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\np_nes_dmc.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\np_nes_dmc.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\np_nes_fds.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\np_nes_fds.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\okim6258.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\okim6258.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\okim6295.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\okim6295.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\Ootake_PSG.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\Ootake_PSG.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\opl.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\opl.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\opll.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\opll.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\opm.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\opm.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\panning.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\panning.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\pokey.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\pokey.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\pwm.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\pwm.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\qsound_ctr.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\qsound_ctr.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\qsound_intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\qsound_intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\qsound_mame.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\qsound_mame.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\rf5c68.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\rf5c68.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\saa1099.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\saa1099.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scd_pcm.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scd_pcm.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scsp.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scsp.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scspdsp.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scspdsp.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\scsplfo.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\segapcm.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\segapcm.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\sn76489.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\sn76489.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\sn76496.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\sn76496.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\sn764intf.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\sn764intf.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\upd7759.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\upd7759.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\vrc7tone.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\vsu.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\vsu.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ws_audio.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ws_audio.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ws_initialIo.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\x1_010.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\x1_010.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym2151.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym2151.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym2413.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym2413.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym2612.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym2612.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym3438.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ym3438.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymdeltat.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymdeltat.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymf262.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymf262.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymf271.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymf271.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymf278b.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymf278b.h
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymz280b.c
# End Source File
# Begin Source File

SOURCE=..\VGMPlay\chips\ymz280b.h
# End Source File
# End Group
# End Target
# End Project
