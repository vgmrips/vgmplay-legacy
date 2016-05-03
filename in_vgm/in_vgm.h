//#include "../VGMPlay.h"

#if defined(APLHA)

#define VER_EXTRA	" alpha"
#define VER_DATE	" ("__DATE__" "__TIME__")"

#elif defined(BETA)

#define VER_EXTRA	" beta"
#define VER_DATE	" ("__DATE__" "__TIME__")"

#else

#define VER_EXTRA	""
#define VER_DATE	""

#endif

#define INVGM_VERSION		VGMPLAY_VER_STR VER_EXTRA
#define INVGM_TITLE			"VGM Input Plugin v" INVGM_VERSION
#define INVGM_TITLE_FULL	"VGM Input Plugin v" INVGM_VERSION VER_DATE

typedef struct plugin_options
{
	bool ImmediateUpdate;
	bool NoInfoCache;
	
	UINT32 SampleRate;
	UINT32 ChipRate;
	UINT32 PauseNL;
	UINT32 PauseLp;
	
	char TitleFormat[0x80];
	bool JapTags;
	bool AppendFM2413;
	bool TrimWhitespc;
	bool StdSeparators;
	bool TagFallback;
	UINT32 MLFileType;
	
	bool Enable7z;
	
	bool ResetMuting;
} PLGIN_OPTS;

extern PLGIN_OPTS Options;

#include "resource.h"
