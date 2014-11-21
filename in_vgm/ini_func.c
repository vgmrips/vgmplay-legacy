#include <stdio.h>
#include <windows.h>

#include "stdbool.h"
#include "chips/mamedef.h"	// for (U)INTxx types

extern char IniFilePath[MAX_PATH];

void ReadIni_Integer(const char* Section, const char* Key, UINT32* Value)
{
	*Value = (INT32)GetPrivateProfileInt(Section, Key, *Value, IniFilePath);
	
	return;
}

void ReadIni_SIntSht(const char* Section, const char* Key, INT16* Value)
{
	*Value = (INT16)GetPrivateProfileInt(Section, Key, *Value, IniFilePath);
	
	return;
}

void ReadIni_IntByte(const char* Section, const char* Key, UINT8* Value)
{
	*Value = (UINT8)GetPrivateProfileInt(Section, Key, *Value, IniFilePath);
	
	return;
}

void ReadIni_Boolean(const char* Section, const char* Key, bool* Value)
{
	char TempStr[0x10];
	
	GetPrivateProfileString(Section, Key, "", TempStr, 0x10, IniFilePath);
	if (! strcmp(TempStr, ""))
		return;
	
	if (! stricmp(TempStr, "True"))
		*Value = true;
	else if (! stricmp(TempStr, "False"))
		*Value = false;
	else
		*Value = strtol(TempStr, NULL, 0) ? true : false;
	
	return;
}

void ReadIni_String(const char* Section, const char* Key, char* String, UINT32 StrSize)
{
	GetPrivateProfileString(Section, Key, String, String, StrSize, IniFilePath);
	
	return;
}

void ReadIni_Float(const char* Section, const char* Key, float* Value)
{
	char TempStr[0x10];
	
	GetPrivateProfileString(Section, Key, "", TempStr, 0x10, IniFilePath);
	if (! strcmp(TempStr, ""))
		return;
	
	*Value = (float)strtod(TempStr, NULL);
	
	return;
}



void WriteIni_Integer(const char* Section, const char* Key, UINT32 Value)
{
	char TempStr[0x10];
	
	sprintf(TempStr, "%u", Value);
	WritePrivateProfileString(Section, Key, TempStr, IniFilePath);
	
	return;
}

void WriteIni_SInteger(const char* Section, const char* Key, INT32 Value)
{
	char TempStr[0x10];
	
	sprintf(TempStr, "%d", Value);
	WritePrivateProfileString(Section, Key, TempStr, IniFilePath);
	
	return;
}

void WriteIni_XInteger(const char* Section, const char* Key, UINT32 Value)
{
	char TempStr[0x10];
	
	sprintf(TempStr, "0x%02X", Value);
	WritePrivateProfileString(Section, Key, TempStr, IniFilePath);
	
	return;
}

void WriteIni_Boolean(const char* Section, const char* Key, bool Value)
{
	char TempStr[0x10];
	
	if (Value)
		strcpy(TempStr, "True");
	else
		strcpy(TempStr, "False");
	WritePrivateProfileString(Section, Key, TempStr, IniFilePath);
	
	return;
}

void WriteIni_String(const char* Section, const char* Key, char* String)
{
	WritePrivateProfileString(Section, Key, String, IniFilePath);
	
	return;
}

void WriteIni_Float(const char* Section, const char* Key, float Value)
{
	char TempStr[0x10];
	
	sprintf(TempStr, "%f", Value);
	WritePrivateProfileString(Section, Key, TempStr, IniFilePath);
	
	return;
}
