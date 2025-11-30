/*
DBus/MPRIS for VGMPlay.
By Tasos Sahanidis <vgmsrc@tasossah.com>

required packages:
libdbus-1-dev

compiling:
CFLAGS=$(pkg-config --cflags dbus-1)
LDFLAGS=$(pkg-config --libs dbus-1)

They weren't lying when they said that using libdbus directly signs you up for some pain...
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dbus/dbus.h>
#include "chips/mamedef.h"          // for UINT8
#include "mmkeys.h"
#include <inttypes.h>
#include "dbus.h"
#include "stdbool.h"
#include "VGMPlay.h"                // For VGMFile.h and CHIP_COUNT
#include <errno.h>
#include <locale.h>
#include <wchar.h>
#include <limits.h>
#include <glob.h>
#include "VGMPlay_Intf.h"

// DBus MPRIS Constants
#define DBUS_MPRIS_PATH             "/org/mpris/MediaPlayer2"
#define DBUS_MPRIS_MEDIAPLAYER2     "org.mpris.MediaPlayer2"
#define DBUS_MPRIS_PLAYER           "org.mpris.MediaPlayer2.Player"
#define DBUS_MPRIS_VGMPLAY          "org.mpris.MediaPlayer2.vgmplay"
#define DBUS_PROPERTIES             "org.freedesktop.DBus.Properties"

//#define DBUS_DEBUG

#ifdef DBUS_DEBUG
#define ART_EXISTS_PRINTF(x)    fprintf(stderr, "\nTrying %s\n", (x));
#else
#define ART_EXISTS_PRINTF(x)
#endif

#define ART_EXISTS(path)        ART_EXISTS_PRINTF(path) \
                                if(FileExists((path))) { \
                                    free(basepath); \
                                    return; \
                                }

static mmkey_cbfunc evtCallback = NULL;

extern INT32 VGMSmplPlayed;
extern UINT32 SampleRate;
extern GD3_TAG VGMTag;
extern wchar_t* GetTagStrEJ(const wchar_t* EngTag, const wchar_t* JapTag);
extern INT32 SampleVGM2Playback(INT32 SampleVal);
extern VGM_HEADER VGMHead;

// Playlist status
extern UINT32 PLFileCount;
extern UINT32 CurPLFile;
extern UINT8 PLMode;

// Playlist Vars
extern char PLFileName[];

// Playback Status
extern UINT8 PlayingMode;
extern bool PausePlay;

DBusConnection* dbus_connection = NULL;

// Seek Function
extern void SeekVGM(bool Relative, INT32 PlayBkSamples);

// Filename
extern char VgmFileName[];

extern char* GetLastDirSeparator(const char* FilePath);

// Needed for loop detection
static UINT32 OldLoopCount;
extern UINT32 VGMCurLoop;

// MPRIS Metadata Struct
typedef struct DBusMetadata_
{
    const void* title;
    const char* dbusType;
    const void* content;
    int contentType;
    size_t childLen;
} DBusMetadata;

// Cached art path
static char* cached_artpath = NULL;
// Effectively the allocation size of cached_artpath, including the terminator
static size_t pathmax = 0;

// Misc Helper Functions

static inline void invalidateArtCache()
{
    *cached_artpath = '\0';
}

static char* wcharToUTF8(wchar_t* GD3)
{
    size_t len = wcstombs(NULL, GD3, 0) + 1;
    char* out = malloc(len);
    wcstombs(out, GD3, len);
    return out;
}

static char* urlencode(const char* str)
{
    size_t len = strlen(str);
    // Don't try to encode blank strings
    if(!len)
        return calloc(1, sizeof(char));

    // strlen("file://") + max str size + \0
    char* newstring = malloc(7 + len * 3 + 1);
    char* loop = newstring;
    loop += sprintf(loop, "%s", "file://");
    for(size_t i = 0; i < len; i++)
    {
        // http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
        unsigned char c = (unsigned char)str[i];
        switch(c)
        {
            case 32:
            case 36:
            case 38:
            case 43:
            case 44:
            case 58:
            case 59:
            case 61:
            case 63:
            case 64:
                loop += sprintf(loop, "%%%02X", c);
                break;
            default:
                if(c > 127)
                    loop += sprintf(loop, "%%%02X", c);
                else
                   loop += sprintf(loop, "%c", str[i]);
        }
    }
    return newstring;
}

// Return current position in μs
static inline int64_t ReturnPosMsec(UINT32 SamplePos, UINT32 SmplRate)
{
    return (int64_t)((SamplePos / (double)SmplRate)*1000000.0);
}

// Return current position in samples
static inline INT32 ReturnSamplePos(int64_t UsecPos, UINT32 SmplRate)
{
    return (UsecPos / 1000000.0)*(double)SmplRate;
}

static inline int FileExists(char* file)
{
    return access(file, F_OK) + 1;
}

// DBus Helper Functions
static void DBusEmptyMethodResponse(DBusConnection* connection, DBusMessage* request)
{
    DBusMessage* reply = dbus_message_new_method_return(request);
    dbus_message_append_args(reply, DBUS_TYPE_INVALID);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
}

static void DBusReplyToIntrospect(DBusConnection* connection, DBusMessage* request)
{
    const char* introspection_data =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node>\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.DBus.Peer\">\n"
"    <method name=\"Ping\"/>\n"
"    <method name=\"GetMachineId\">\n"
"      <arg type=\"s\" name=\"machine_uuid\" direction=\"out\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.DBus.Properties\">\n"
"    <method name=\"Get\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"v\"/>\n"
"    </method>\n"
"    <method name=\"Set\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"v\"/>\n"
"    </method>\n"
"    <method name=\"GetAll\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"a{sv}\"/>\n"
"    </method>\n"
"    <signal name=\"PropertiesChanged\">\n"
"      <arg type=\"s\"/>\n"
"      <arg type=\"a{sv}\"/>\n"
"      <arg type=\"as\"/>\n"
"    </signal>\n"
"  </interface>\n"
"  <interface name=\"org.mpris.MediaPlayer2\">\n"
"    <property name=\"Identity\" type=\"s\" access=\"read\" />\n"
"    <property name=\"DesktopEntry\" type=\"s\" access=\"read\" />\n"
"    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\" />\n"
"    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\" />\n"
"    <property name=\"HasTrackList\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanQuit\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanRaise\" type=\"b\" access=\"read\" />\n"
"    <method name=\"Quit\" />\n"
"    <method name=\"Raise\" />\n"
"  </interface>\n"
"  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
"    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\" />\n"
"    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\" />\n"
"    <property name=\"Volume\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"Position\" type=\"x\" access=\"read\" />\n"
"    <property name=\"Rate\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"MinimumRate\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"MaximumRate\" type=\"d\" access=\"readwrite\" />\n"
"    <property name=\"CanControl\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanGoNext\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanPlay\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanPause\" type=\"b\" access=\"read\" />\n"
"    <property name=\"CanSeek\" type=\"b\" access=\"read\" />\n"
"    <method name=\"Previous\" />\n"
"    <method name=\"Next\" />\n"
"    <method name=\"Stop\" />\n"
"    <method name=\"Play\" />\n"
"    <method name=\"Pause\" />\n"
"    <method name=\"PlayPause\" />\n"
"    <method name=\"Seek\">\n"
"      <arg type=\"x\" direction=\"in\" />\n"
"    </method>"
"    <method name=\"OpenUri\">\n"
"      <arg type=\"s\" direction=\"in\" />\n"
"    </method>\n"
"    <method name=\"SetPosition\">\n"
"      <arg type=\"o\" direction=\"in\" />\n"
"      <arg type=\"x\" direction=\"in\" />\n"
"    </method>\n"
"    <signal name=\"Seeked\">\n"
"      <arg type=\"x\" name=\"Position\"/>\n"
"    </signal>\n"
"  </interface>\n"
"</node>\n"
;

    DBusMessage* reply = dbus_message_new_method_return(request);
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspection_data, DBUS_TYPE_INVALID);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
}

static void DBusReplyWithVariant(DBusMessageIter* args, int type, char* type_as_string, const void* response)
{
    DBusMessageIter subargs;
    dbus_message_iter_open_container(args, DBUS_TYPE_VARIANT, type_as_string, &subargs);
        dbus_message_iter_append_basic(&subargs, type, response);
    dbus_message_iter_close_container(args, &subargs);
}

void DBusAppendCanGoNext(DBusMessageIter* args)
{
    dbus_bool_t response = FALSE;
    if(PLMode == 0x01)
        if(CurPLFile < PLFileCount - 1)
            response = TRUE;
    DBusReplyWithVariant(args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
}

void DBusAppendCanGoPrevious(DBusMessageIter* args)
{
    dbus_bool_t response = FALSE;
    if(PLMode == 0x01)
        if(CurPLFile > 0x00)
            response = TRUE;
    DBusReplyWithVariant(args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
}

static void DBusSendMetadataArray(DBusMessageIter* dict_root, DBusMetadata meta[], size_t dbus_meta_array_len)
{
    DBusMessageIter root_variant, dict, dict_entry, variant;

    dbus_message_iter_open_container(dict_root, DBUS_TYPE_VARIANT, "a{sv}", &root_variant);
        // Open Root Container
        dbus_message_iter_open_container(&root_variant, DBUS_TYPE_ARRAY, "{sv}", &dict);
            for(size_t i = 0; i < dbus_meta_array_len; i++)
            {
                // Ignore empty strings
                if(meta[i].contentType == DBUS_TYPE_STRING)
                    if(!strlen(*(char**)meta[i].content))
                        continue;
                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &meta[i].title);
                    // Field Value
                    dbus_message_iter_open_container(&dict_entry, DBUS_TYPE_VARIANT, meta[i].dbusType, &variant);

                        if(meta[i].contentType == DBUS_TYPE_ARRAY)
                        {
                            DBusMessageIter array_root;

                            dbus_message_iter_open_container(&variant, meta[i].contentType, meta[i].dbusType, &array_root);

                                for(size_t len = 0; len < meta[i].childLen; len++)
                                {
                                    const DBusMetadata* content = meta[i].content;
                                    dbus_message_iter_append_basic(&array_root, content[len].contentType, content[len].content);
                                }

                            dbus_message_iter_close_container(&variant, &array_root);
                        }
                        else
                            dbus_message_iter_append_basic(&variant, meta[i].contentType, meta[i].content);

                    dbus_message_iter_close_container(&dict_entry, &variant);

                dbus_message_iter_close_container(&dict, &dict_entry);
            }
        // Close Root Container
        dbus_message_iter_close_container(&root_variant, &dict);
    dbus_message_iter_close_container(dict_root, &root_variant);
}

static inline char* getAbsBasePath()
{
    // Pick the appropriate pointer
    const char* fileptr = VgmFileName;
    if(PLMode == 0x01)
        fileptr = PLFileName;

    char* basepath = realpath(fileptr, NULL);
    if(basepath) {
        char* sep = GetLastDirSeparator(basepath);
        // It's okay to + 1 because sep points to a character before the terminator
        if(sep)
            sep[1] = '\0';
#ifdef DBUS_DEBUG
        fprintf(stderr, "\nBase Path %s\n", basepath);
#endif
    }
    return basepath;
}

static inline void getArtPath(const char* utf8album, char* artpath, const size_t artpath_size)
{
    char* basepath = getAbsBasePath();
    if(basepath == NULL || basepath[0] == '\0') {
        free(basepath);
        return;
    }

    // Now that we have the base path, we start looking for art
    // If we are reading a playlist, append everything after the separator to the path and replace its m3u extension with png
    if(PLMode == 0x01)
    {
        // Using the GNU version of basename which doesn't modify the argument
        char* plname = basename(PLFileName);
        char* plname_ext = strrchr(plname, '.');

        // Find out how many characters we need to print until the file extension
        // We use int because that's what the precision specifier accepts
        // The filename is really not going to be more than INT_MAX characters
        int plname_len = (plname_ext == NULL ? (int)strlen(plname) : (int)(plname_ext - plname));

        snprintf(artpath, artpath_size, "%s%.*s.png", basepath, plname_len, plname);

        ART_EXISTS(artpath);
    }

    // If we get here, we're probably in single track mode, or the playlist is named differently
    // check base path + album + .png
    snprintf(artpath, artpath_size, "%s%s.png", basepath, utf8album);
    ART_EXISTS(artpath);

    // As a last resort, pick the first image glob can find in the base path
    // Append the case insensitive extension to the base path
    snprintf(artpath, artpath_size, "%s*.[pP][nN][gG]", basepath);

#ifdef DBUS_DEBUG
    fprintf(stderr, "Using glob %s\n", artpath);
#endif
    glob_t result;
    if(glob(artpath, GLOB_NOSORT, NULL, &result) == 0)
    {
        if(result.gl_pathc > 0)
        {
            snprintf(artpath, artpath_size, "%s", result.gl_pathv[0]);
            globfree(&result);
            ART_EXISTS(artpath);
        }
    }
    globfree(&result);

    // There's nothing else we can do. Return an empty string
    *artpath = '\0';
    free(basepath);
}

static void DBusSendMetadata(DBusMessageIter* dict_root)
{
    // Send an empty array in a variant if nothing is playing
    if(PlayingMode == 0xFF)
    {
        DBusSendMetadataArray(dict_root, NULL, 0);
        return;
    }

    // Prepare metadata

    // Album
    wchar_t* album = GetTagStrEJ(VGMTag.strGameNameE, VGMTag.strGameNameJ);
    char* utf8album = wcharToUTF8(album);

    // Title
    wchar_t* title =  GetTagStrEJ(VGMTag.strTrackNameE, VGMTag.strTrackNameJ);
    char* utf8title = wcharToUTF8(title);

    // Length
    int64_t len64 = 0;
    INT32 VGMPbSmplCount = SampleVGM2Playback(VGMHead.lngTotalSamples);
    len64 = ReturnPosMsec(VGMPbSmplCount, SampleRate);

    // Artist
    wchar_t* artist = GetTagStrEJ(VGMTag.strAuthorNameE, VGMTag.strAuthorNameJ);
    char* utf8artist = wcharToUTF8(artist);

    // Track Number in playlist
    int32_t tracknum = 0;
    if(PLMode == 0x01)
        tracknum = (int32_t)(CurPLFile + 0x01);

    // Try to get the cover art url
    if(cached_artpath[0] == '\0')
        getArtPath(utf8album, cached_artpath, pathmax);

#ifdef DBUS_DEBUG
    fprintf(stderr, "\nFinal art path %s\n", artpath);
#endif

    // URL encode the path to the png
    char* arturlescaped = urlencode(cached_artpath);

    // Game release date
    wchar_t* release = VGMTag.strReleaseDate;
    char* utf8release = wcharToUTF8(release);

    // VGM File Creator
    wchar_t* creator = VGMTag.strCreator;
    char* utf8creator = wcharToUTF8(creator);

    // Notes
    wchar_t* notes = VGMTag.strNotes;
    char* utf8notes = wcharToUTF8(notes);

    // System
    wchar_t* system = GetTagStrEJ(VGMTag.strSystemNameE, VGMTag.strSystemNameJ);
    char* utf8system = wcharToUTF8(system);

    // VGM File version
    uint32_t version = VGMHead.lngVersion;

    // Loop point
    int64_t loop = ReturnPosMsec(VGMHead.lngLoopSamples, SampleRate);

    if(!strlen(utf8artist))
    {
        utf8artist = realloc(utf8artist, strlen(utf8album) + 1);
        strcpy(utf8artist, utf8album);
    }

    // Encapsulate some data in DBusMetadata Arrays
    // Artist Array
    DBusMetadata dbusartist[] =
    {
        { "", DBUS_TYPE_STRING_AS_STRING, &utf8artist, DBUS_TYPE_STRING, 0 },
    };

    // Genre Array
    const char* genre = "Video Game Music";
    DBusMetadata dbusgenre[] =
    {
        { "", DBUS_TYPE_STRING_AS_STRING, &genre, DBUS_TYPE_STRING, 0 },
    };

    DBusMetadata chips[CHIP_COUNT] = { 0 };
    size_t chipslen = 0;
    // Generate chips array
    for(UINT8 CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
    {
        UINT8 ChpType;
        UINT32 ChpClk = GetChipClock(&VGMHead, CurChip, &ChpType);

        if(ChpClk && GetChipClock(&VGMHead, 0x80 | CurChip, NULL))
            ChpClk |= 0x40000000;

        if(ChpClk)
        {
            if(CurChip == 0x00 && (ChpClk & 0x80000000))
                ChpClk &= ~0x40000000;
            if(ChpClk & 0x80000000)
            {
                ChpClk &= ~0x80000000;
                CurChip |= 0x80;
            }
            const char* chip = GetAccurateChipName(CurChip, ChpType);
            char** charptr = malloc(sizeof(char*));
            chips[chipslen].content = charptr;
            *charptr = strdup(chip);
            // Set the type
            chips[chipslen].dbusType = DBUS_TYPE_STRING_AS_STRING;
            chips[chipslen].contentType = DBUS_TYPE_STRING;

            // Duplicate the chip if necessasry
            if(ChpClk & 0x40000000)
            {
                chipslen++;
                chips[chipslen].content = chips[chipslen - 1].content;
                chips[chipslen].dbusType = DBUS_TYPE_STRING_AS_STRING;
                chips[chipslen].contentType = DBUS_TYPE_STRING;
            }
            chipslen++;
        }
    }

    // URL encoded Filename
    char* abspath = realpath(VgmFileName, NULL);
    if(abspath == NULL)
        abspath = calloc(1, sizeof(char));

    char* url = urlencode(abspath);
    free(abspath);

    // Stubs
    const char* trackid = "/org/mpris/MediaPlayer2/CurrentTrack";
    const char* lastused = "2018-01-04T12:21:32Z";
    int32_t discnum = 1;
    int32_t usecnt = 0;
    double userrating = 0;

    DBusMetadata meta[] =
    {
        { "mpris:trackid", DBUS_TYPE_STRING_AS_STRING, &trackid, DBUS_TYPE_STRING, 0 },
        { "xesam:url", DBUS_TYPE_STRING_AS_STRING, &url, DBUS_TYPE_STRING, 0 },
        { "mpris:artUrl", DBUS_TYPE_STRING_AS_STRING, &arturlescaped, DBUS_TYPE_STRING, 0 },
        { "xesam:lastused", DBUS_TYPE_STRING_AS_STRING, &lastused, DBUS_TYPE_STRING, 0 },
        { "xesam:genre", "as", &dbusgenre, DBUS_TYPE_ARRAY, 1 },
        { "xesam:album", DBUS_TYPE_STRING_AS_STRING, &utf8album, DBUS_TYPE_STRING, 0 },
        { "xesam:title", DBUS_TYPE_STRING_AS_STRING, &utf8title, DBUS_TYPE_STRING, 0 },
        { "mpris:length", DBUS_TYPE_INT64_AS_STRING, &len64, DBUS_TYPE_INT64, 0 },
        { "xesam:artist", "as", &dbusartist, DBUS_TYPE_ARRAY, 1 },
        { "xesam:composer", "as", &dbusartist, DBUS_TYPE_ARRAY, 1 },
        { "xesam:trackNumber", DBUS_TYPE_INT32_AS_STRING, &tracknum, DBUS_TYPE_INT32, 1 },
        { "xesam:discNumber", DBUS_TYPE_INT32_AS_STRING, &discnum, DBUS_TYPE_INT32, 1 },
        { "xesam:useCount", DBUS_TYPE_INT32_AS_STRING, &usecnt, DBUS_TYPE_INT32, 1 },
        { "xesam:userRating", DBUS_TYPE_DOUBLE_AS_STRING, &userrating, DBUS_TYPE_DOUBLE, 1 },
        // Extra non-xesam/mpris entries
        { "vgm:release", DBUS_TYPE_STRING_AS_STRING, &utf8release, DBUS_TYPE_STRING, 0 },
        { "vgm:creator", DBUS_TYPE_STRING_AS_STRING, &utf8creator, DBUS_TYPE_STRING, 0 },
        { "vgm:notes", DBUS_TYPE_STRING_AS_STRING, &utf8notes, DBUS_TYPE_STRING, 0 },
        { "vgm:system", DBUS_TYPE_STRING_AS_STRING, &utf8system, DBUS_TYPE_STRING, 0 },
        { "vgm:version", DBUS_TYPE_UINT32_AS_STRING, &version, DBUS_TYPE_UINT32, 0 },
        { "vgm:loop", DBUS_TYPE_INT64_AS_STRING, &loop, DBUS_TYPE_INT64, 0 },
        { "vgm:chips", "as", &chips, DBUS_TYPE_ARRAY, chipslen },
    };
    DBusSendMetadataArray(dict_root, meta, sizeof(meta)/sizeof(*meta));

    // Free everything
    free(arturlescaped);
    free(url);
    free(utf8title);
    free(utf8album);
    free(utf8artist);
    free(utf8release);
    free(utf8creator);
    free(utf8notes);
    free(utf8system);
    for(size_t i = 0; i < chipslen; i++)
    {
        // If the next pointer is the same as the current one, don't free it.
        // We also discard const here since we know we manually allocated these above.
        char** ptr = (char**)chips[i].content;
        if(chips[i].content == chips[i+1].content)
            continue;
        char* ch = *ptr;
        free(ptr);
        free(ch);
    }
}

static void DBusSendPlaybackStatus(DBusMessageIter* args)
{
    const char* response;

    if(PlayingMode == 0xFF)
        response = "Stopped";
    else if(PausePlay)
        response = "Paused";
    else
        response = "Playing";

    DBusReplyWithVariant(args, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &response);
}

void DBus_EmitSignal_Internal(DBusConnection* connection, UINT8 type)
{
#ifdef DBUS_DEBUG
    fprintf(stderr, "Emitting signal type 0x%x\n", type);
#endif
    if(connection == NULL)
        return;

    // Make sure we're connected to DBus
    // Otherwise discard the event
    if(!dbus_connection_get_is_connected(connection))
        return;

    DBusMessage* msg;
    DBusMessageIter args;

    if(type & SIGNAL_SEEK)
    {
        msg = dbus_message_new_signal(DBUS_MPRIS_PATH, DBUS_MPRIS_PLAYER, "Seeked");

        dbus_message_iter_init_append(msg, &args);
        int64_t response = ReturnPosMsec(VGMSmplPlayed, SampleRate);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT64, &response);

        dbus_connection_send(connection, msg, NULL);

        dbus_message_unref(msg);

        // Despite Seeked() being a different signal
        // we need to send the changed position property too
        // so we shouldn't return just yet.
    }

    msg = dbus_message_new_signal(DBUS_MPRIS_PATH, DBUS_PROPERTIES, "PropertiesChanged");

    dbus_message_iter_init_append(msg, &args);
    // The interface in which the properties were changed must be sent first
    // Thankfully the only properties changing are in the same interface
    const char* player = DBUS_MPRIS_PLAYER;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &player);
    // Wrap everything inside an a{sv}
    DBusMessageIter dict, dict_entry;

    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
        if(type & SIGNAL_METADATA)
        {
            // It is possible for the art to change if the playlist contains tracks from multiple games
            // since art can be found by the "Game Name".png field
            // Invalidate it on track change, as it will be populated later on demand
            invalidateArtCache();
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                const char* title = "Metadata";
                dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusSendMetadata(&dict_entry);
            dbus_message_iter_close_container(&dict, &dict_entry);
        }
        if(type & SIGNAL_CONTROLS)
        {
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                const char* title = "CanGoPrevious";
                dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                DBusAppendCanGoPrevious(&dict_entry);
            dbus_message_iter_close_container(&dict, &dict_entry);
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                title = "CanGoNext";
                dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                DBusAppendCanGoNext(&dict_entry);
            dbus_message_iter_close_container(&dict, &dict_entry);
        }
        if(type & SIGNAL_PLAYSTATUS)
        {
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                const char* playing = "PlaybackStatus";
                dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &playing);
                DBusSendPlaybackStatus(&dict_entry);

            dbus_message_iter_close_container(&dict, &dict_entry);
        }
        if((type & SIGNAL_SEEK) || (type & SIGNAL_PLAYSTATUS))
        {
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                const char* playing = "Position";
                dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &playing);
                int64_t response = ReturnPosMsec(VGMSmplPlayed, SampleRate);
                DBusReplyWithVariant(&dict_entry, DBUS_TYPE_INT64, DBUS_TYPE_INT64_AS_STRING, &response);
            dbus_message_iter_close_container(&dict, &dict_entry);
        }
        if((type & SIGNAL_VOLUME))
        {
            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                const char* playing = "Volume";
                dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &playing);
                const double response = 1.0;
                DBusReplyWithVariant(&dict_entry, DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &response);
            dbus_message_iter_close_container(&dict, &dict_entry);
        }
    dbus_message_iter_close_container(&args, &dict);

    // Send a blank array _with signature "s"_.
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &dict);
    dbus_message_iter_close_container(&args, &dict);

    dbus_connection_send(connection, msg, NULL);
    dbus_message_unref(msg);
}


void DBus_EmitSignal(UINT8 type)
{
    DBus_EmitSignal_Internal(dbus_connection, type);
}

static void DBusSendMimeTypes(DBusMessageIter* args)
{
    DBusMessageIter variant, subargs;
    dbus_message_iter_open_container(args, DBUS_TYPE_VARIANT, "as", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &subargs);
            char* response[] = {"audio/x-vgm", "audio/x-vgz"};
            size_t i_len = sizeof(response) / sizeof(*response);
            for(size_t i = 0; i < i_len; ++i)
            {
                dbus_message_iter_append_basic(&subargs, DBUS_TYPE_STRING, &response[i]);
            }
        dbus_message_iter_close_container(&variant, &subargs);
    dbus_message_iter_close_container(args, &variant);
}

static void DBusSendUriSchemes(DBusMessageIter* args)
{
    DBusMessageIter variant, subargs;
    dbus_message_iter_open_container(args, DBUS_TYPE_VARIANT, "as", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &subargs);
            char* response[] = {"file"};
            size_t i_len = sizeof(response) / sizeof(*response);
            for(size_t i = 0; i < i_len; ++i)
            {
                dbus_message_iter_append_basic(&subargs, DBUS_TYPE_STRING, &response[i]);
            }
        dbus_message_iter_close_container(&variant, &subargs);
    dbus_message_iter_close_container(args, &variant);
}

static void DBusSendEmptyMethodResponse(DBusConnection* connection, DBusMessage* message)
{
    DBusMessage* reply;
    DBusMessageIter args;
    reply = dbus_message_new_method_return(message);
    dbus_message_iter_init_append(reply, &args);
    dbus_connection_send(connection, reply, NULL);
}

static DBusHandlerResult DBusHandler(DBusConnection* connection, DBusMessage* message, void* user_data)
{
#ifdef DBUS_DEBUG
    const char* interface_name = dbus_message_get_interface(message);
    const char* member_name = dbus_message_get_member(message);
    const char* path_name = dbus_message_get_path(message);
    const char* sender = dbus_message_get_sender(message);

    fprintf(stderr, "Interface: %s; Member: %s; Path: %s; Sender: %s;\n", interface_name, member_name, path_name, sender);
#endif
    if(!dbus_message_has_path(message, DBUS_MPRIS_PATH))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    // Respond to Introspect
    if(dbus_message_is_method_call(message, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
    {
        DBusReplyToIntrospect(connection, message);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_MEDIAPLAYER2, "Raise"))
    {
        printf("\a");
        fflush(stdout);
        DBusSendEmptyMethodResponse(connection, message);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    // Respond to Get
    else if(dbus_message_is_method_call(message, DBUS_INTERFACE_PROPERTIES, "Get"))
    {
        char* method_interface_arg = NULL;
        char* method_property_arg  = NULL;
        DBusMessage* reply;

        if(!dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &method_interface_arg, DBUS_TYPE_STRING, &method_property_arg, DBUS_TYPE_INVALID))
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        //printf("Interface name: %s\n", method_interface_arg);
        //init reply
        reply = dbus_message_new_method_return(message);

        //printf("Property name: %s\n", method_property_arg);

        // Global Iterator
        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);

        if(!strcmp(method_interface_arg, DBUS_MPRIS_MEDIAPLAYER2))
        {
            if(!strcmp(method_property_arg, "SupportedMimeTypes"))
            {
                DBusSendMimeTypes(&args);
            }
            else if(!strcmp(method_property_arg, "SupportedUriSchemes"))
            {
                DBusSendUriSchemes(&args);
            }
            else if(!strcmp(method_property_arg, "CanQuit"))
            {
                const dbus_bool_t response = TRUE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "CanRaise"))
            {
                const dbus_bool_t response = TRUE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "HasTrackList"))
            {
                const dbus_bool_t response = FALSE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "DesktopEntry"))
            {
                const char* response = "vgmplay";
                DBusReplyWithVariant(&args, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "Identity"))
            {
                const char* response = "VGMPlay";
                DBusReplyWithVariant(&args, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &response);
            }
            else
                dbus_message_append_args(reply, DBUS_TYPE_INVALID);
        }
        else if(!strcmp(method_interface_arg, DBUS_MPRIS_PLAYER))
        {
            if(!strcmp(method_property_arg, "CanPlay"))
            {
                const dbus_bool_t response = TRUE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "CanPause"))
            {
                const dbus_bool_t response = TRUE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "CanGoNext"))
            {
                DBusAppendCanGoNext(&args);
            }
            else if(!strcmp(method_property_arg, "CanGoPrevious"))
            {
                DBusAppendCanGoPrevious(&args);
            }
            else if(!strcmp(method_property_arg, "CanSeek"))
            {
                const dbus_bool_t response = TRUE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "PlaybackStatus"))
            {
                DBusSendPlaybackStatus(&args);
            }
            else if(!strcmp(method_property_arg, "Position"))
            {
                int64_t response = ReturnPosMsec(VGMSmplPlayed, SampleRate);
                DBusReplyWithVariant(&args, DBUS_TYPE_INT64, DBUS_TYPE_INT64_AS_STRING, &response);
            }
            //Dummy volume
            else if(!strcmp(method_property_arg, "Volume") || !strcmp(method_property_arg, "MaximumRate") || !strcmp(method_property_arg, "MinimumRate") || !strcmp(method_property_arg, "Rate"))
            {
                const double response = 1.0;
                DBusReplyWithVariant(&args, DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &response);

            }
            else if(!strcmp(method_property_arg, "CanControl"))
            {
                const dbus_bool_t response = TRUE;
                DBusReplyWithVariant(&args, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &response);
            }
            else if(!strcmp(method_property_arg, "Metadata"))
            {
                DBusSendMetadata(&args);
            }
            else
                dbus_message_append_args(reply, DBUS_TYPE_INVALID);
        }
        else
        {
#ifdef DBUS_DEBUG
            fprintf(stderr, "Unimplemented interface %s passed to Get()\n", method_interface_arg);
#endif
            dbus_message_unref(reply);
            reply = dbus_message_new_error(message, "org.freedesktop.DBus.Error.InvalidArgs", "No such interface");
        }

        dbus_connection_send(connection, reply, NULL);
        dbus_message_unref(reply);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    // Respond to GetAll
    else if(dbus_message_is_method_call(message, DBUS_INTERFACE_PROPERTIES, "GetAll"))
    {
        char* method_interface_arg = NULL;
        DBusMessage* reply;

        if(!dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &method_interface_arg, DBUS_TYPE_INVALID))
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        //init reply
        reply = dbus_message_new_method_return(message);

        //printf("Property name: %s\n", property_name);

        // Global Iterator
        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);

        const dbus_bool_t dbustrue = TRUE;
        const dbus_bool_t dbusfalse = FALSE;
        const char* title;
        const char* strresponse;

        if(!strcmp(method_interface_arg, "org.mpris.MediaPlayer2"))
        {
            // a{sv}
            DBusMessageIter dict, dict_entry;
            dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

                // Open Dict
                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "SupportedMimeTypes";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusSendMimeTypes(&dict_entry);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "SupportedUriSchemes";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusSendUriSchemes(&dict_entry);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanQuit";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbustrue);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanRaise";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbustrue);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "HasTrackList";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbusfalse);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "DesktopEntry";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    strresponse = "vgmplay";
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &strresponse);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "Identity";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    strresponse = "VGMPlay";
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &strresponse);
                dbus_message_iter_close_container(&dict, &dict_entry);

            dbus_message_iter_close_container(&args, &dict);
        }
        else if(!strcmp(method_interface_arg, DBUS_MPRIS_PLAYER))
        {
            const double doubleresponse = 1.0;
            // a{sv}
            DBusMessageIter dict, dict_entry;
            dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanControl";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbustrue);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanGoNext";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusAppendCanGoNext(&dict_entry);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanGoPrevious";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusAppendCanGoPrevious(&dict_entry);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanPause";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbustrue);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanPlay";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbustrue);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "CanSeek";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &dbustrue);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "Metadata";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                        DBusSendMetadata(&dict_entry);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "MaximumRate";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &doubleresponse);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "MinimumRate";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &doubleresponse);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "Rate";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &doubleresponse);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "Volume";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &doubleresponse);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "Position";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    int64_t position = ReturnPosMsec(VGMSmplPlayed, SampleRate);
                    DBusReplyWithVariant(&dict_entry, DBUS_TYPE_INT64, DBUS_TYPE_INT64_AS_STRING, &position);
                dbus_message_iter_close_container(&dict, &dict_entry);

                dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry);
                    // Field Title
                    title = "PlaybackStatus";
                    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &title);
                    DBusSendPlaybackStatus(&dict_entry);
                dbus_message_iter_close_container(&dict, &dict_entry);

            dbus_message_iter_close_container(&args, &dict);
        }
        else
        {
#ifdef DBUS_DEBUG
            fprintf(stderr, "Unimplemented interface %s passed to GetAll\n", method_interface_arg);
#endif
            dbus_message_unref(reply);
            reply = dbus_message_new_error(message, "org.freedesktop.DBus.Error.InvalidArgs", "No such interface");
        }

        dbus_connection_send(connection, reply, NULL);
        dbus_message_unref(reply);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    //Respond to Seek
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "Seek"))
    {
        int64_t offset = 0;

        if(!dbus_message_get_args(message, NULL, DBUS_TYPE_INT64, &offset, DBUS_TYPE_INVALID))
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

#ifdef DBUS_DEBUG
        fprintf(stderr, "Seek called with %"PRId64"\n", offset);
#endif
        INT32 TargetSeekPos = ReturnSamplePos(offset, SampleRate);
        SeekVGM(true, TargetSeekPos);

        DBusEmptyMethodResponse(connection, message);

        // Emit seeked signal
        DBus_EmitSignal_Internal(connection, SIGNAL_SEEK);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    //Respond to PlayPause
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "PlayPause"))
    {
        DBusEmptyMethodResponse(connection, message);
        evtCallback(MMKEY_PLAY);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    //Respond to Play
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "Play"))
    {
        DBusEmptyMethodResponse(connection, message);
        if(PausePlay)
            evtCallback(MMKEY_PLAY);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    //Respond to Pause
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "Pause"))
    {
        DBusEmptyMethodResponse(connection, message);
        if(!PausePlay)
            evtCallback(MMKEY_PLAY);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    // Stop is currently a stub
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "Stop"))
    {
        DBusEmptyMethodResponse(connection, message);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    //Respond to Previous
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "Previous"))
    {
        DBusEmptyMethodResponse(connection, message);
        evtCallback(MMKEY_PREV);

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    //Respond to Next
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "Next"))
    {
        DBusEmptyMethodResponse(connection, message);
        evtCallback(MMKEY_NEXT);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    else if(dbus_message_is_method_call(message, DBUS_MPRIS_PLAYER, "SetPosition"))
    {
        int64_t pos;
        const char* path;
        if(!dbus_message_get_args(message, NULL, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INT64, &pos, DBUS_TYPE_INVALID))
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        INT32 seek_pos = ReturnSamplePos(pos, SampleRate);
        SeekVGM(false, seek_pos);

        DBusEmptyMethodResponse(connection, message);
        DBus_EmitSignal_Internal(connection, SIGNAL_SEEK);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    else if(dbus_message_is_method_call(message, DBUS_INTERFACE_PROPERTIES, "Set"))
    {
        // Dummy Set to send a signal to revert Volume change attempts
        DBus_EmitSignal_Internal(connection, SIGNAL_VOLUME);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    else
    {
#ifdef DBUS_DEBUG
        fprintf(stderr, "Method %s for interface %s not implemented", member_name, interface_name);
#endif
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
}

UINT8 MultimediaKeyHook_Init(void)
{
    // Allocate memory for the art path cache
    long pathmax_s = pathconf("/", _PC_PATH_MAX);
    if (pathmax_s <= 0)
        pathmax = 4096;
    else
        pathmax = pathmax_s + 1;

    cached_artpath = malloc(pathmax);
    *cached_artpath = '\0';

    dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
    if(!dbus_connection)
        return 0x00;

    // If we're not the owners, don't bother with anything else
    if(dbus_bus_request_name(dbus_connection, DBUS_MPRIS_VGMPLAY, DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
        dbus_connection_unref(dbus_connection);
        dbus_connection = NULL;
        return 0x00;
    }

    DBusObjectPathVTable vtable =
    {
        .message_function = DBusHandler,
        .unregister_function = NULL,
    };

    dbus_connection_try_register_object_path(dbus_connection, DBUS_MPRIS_PATH, &vtable, NULL, NULL);

    return 0x00;
}

void MultimediaKeyHook_Deinit(void)
{
    if(dbus_connection != NULL)
        dbus_connection_unref(dbus_connection);
    free(cached_artpath);
    cached_artpath = NULL;
}

void MultimediaKeyHook_SetCallback(mmkey_cbfunc callbackFunc)
{
    evtCallback = callbackFunc;
}

void DBus_ReadWriteDispatch(void)
{
    if(dbus_connection == NULL)
        return;

    // Detect loops and send the seeked signal when appropriate
    if(OldLoopCount != VGMCurLoop)
    {
        OldLoopCount = VGMCurLoop;
        DBus_EmitSignal_Internal(dbus_connection, SIGNAL_SEEK);
    }

    // Wait at most for 1ms
    dbus_connection_read_write_dispatch(dbus_connection, 1);
}
