/*	required packages:
libdbus-1-dev libdbus-1-dev
	compiling:
CFLAGS=$(pkg-config --cflags dbus-1 dbus-glib-1)
LDFLAGS=$(pkg-config --libs dbus-1 dbus-glib-1)

Based on https://stackoverflow.com/a/5751632
*/

#include <glib.h>
#include <dbus/dbus-glib.h>

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "chips/mamedef.h"	// for UINT8
#include "mmkeys.h"


#ifndef __g_cclosure_user_marshal_MARSHAL_H__
#define __g_cclosure_user_marshal_MARSHAL_H__

#include    <glib-object.h>

#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)

// VOID:STRING,STRING (hotkeys_GDBus_marshallers.list:1)
static void g_cclosure_user_marshal_VOID__STRING_STRING (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);
static void
g_cclosure_user_marshal_VOID__STRING_STRING (GClosure     *closure,
                                             GValue       *return_value G_GNUC_UNUSED,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint G_GNUC_UNUSED,
                                             gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_STRING) (gpointer     data1,
                                                    gpointer     arg_1,
                                                    gpointer     arg_2,
                                                    gpointer     data2);
  GMarshalFunc_VOID__STRING_STRING callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING_STRING) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_string (param_values + 1),
            g_marshal_value_peek_string (param_values + 2),
            data2);
}

#endif // __g_cclosure_user_marshal_MARSHAL_H__


typedef struct
{
	const char* name;
	UINT8 code;
} KEY_TABLE;


static GMainLoop* evtloop_gloop = NULL;
static pthread_t evtloop_thread = 0;
static mmkey_cbfunc evtCallback = NULL;
static const KEY_TABLE keyTable[] =
{
	{"Play", MMKEY_PLAY},
	{"Previous", MMKEY_PREV},
	{"Next", MMKEY_NEXT},
	{NULL, 0x00}
};


static void media_key_pressed(DBusGProxy* proxy, const char* value1, const char* value2, gpointer user_data)
{
	//printf("mediakey: %s\n", value2);
	const KEY_TABLE* curKey;
	
	for (curKey = keyTable; curKey->name != NULL; curKey ++)
	{
		if (! strcmp(value2, curKey->name))
		{
			if (evtCallback != NULL)
				evtCallback(curKey->code);
			break;
		}
	}
	
	return;
}

static void* event_loop(void* args)
{
	GMainLoop* loop = (GMainLoop*)args;
	
	g_main_loop_run(loop);
	//printf("Event main loop finished.\n");
	
	return NULL;
}


UINT8 MultimediaKeyHook_Init(void)
{
	DBusGConnection* conn;
	DBusGProxy* proxy;
	GError* error;
	
	evtloop_gloop = NULL;
	evtloop_thread = 0;
	
	g_type_init();
	error = NULL;
	
	conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (conn == NULL)
	{
		g_printerr("Failed to connect to the D-BUS daemon: %s\n", error->message);
		g_error_free(error);
		return 1;
	}
	
	evtloop_gloop = g_main_loop_new(NULL, FALSE);
	if (evtloop_gloop == NULL)
	{
		g_printerr("Could not create mainloop\n");
		return 1;
	}
	
	proxy = dbus_g_proxy_new_for_name(conn,
									"org.gnome.SettingsDaemon",
									"/org/gnome/SettingsDaemon/MediaKeys",
									"org.gnome.SettingsDaemon.MediaKeys");
	if (proxy == NULL)
		g_printerr("Could not create proxy object\n");
	
	error = NULL;
	if (! dbus_g_proxy_call(proxy,
					  "GrabMediaPlayerKeys", &error,
					  G_TYPE_STRING, "WebMediaKeys",
					  G_TYPE_UINT, 0,
					  G_TYPE_INVALID,
					  G_TYPE_INVALID))
		g_printerr("Could not grab media player keys: %s\n", error->message);
	
	dbus_g_object_register_marshaller(
			g_cclosure_user_marshal_VOID__STRING_STRING,
			G_TYPE_NONE,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_INVALID);
	
	dbus_g_proxy_add_signal(proxy,
						  "MediaPlayerKeyPressed",
						  G_TYPE_STRING,
						  G_TYPE_STRING,
						  G_TYPE_INVALID);
	
	dbus_g_proxy_connect_signal(proxy,
							  "MediaPlayerKeyPressed",
							  G_CALLBACK(media_key_pressed),
							  NULL,
							  NULL);
	
	g_print("Starting media key listener ...\n");
	pthread_create(&evtloop_thread, NULL, &event_loop, evtloop_gloop);
	
	return 0x00;
}

void MultimediaKeyHook_Deinit(void)
{
	if (evtloop_gloop == NULL)
		return;
	
	//printf("Quitting main loop ...\n");
	g_main_quit(evtloop_gloop);
	pthread_join(evtloop_thread, NULL);
	
	return;
}

void MultimediaKeyHook_SetCallback(mmkey_cbfunc callbackFunc)
{
	evtCallback = callbackFunc;
	
	return;
}
