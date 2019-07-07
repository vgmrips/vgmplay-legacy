#ifndef __DBUS_H__
#define __DBUS_H__

// Defines for the DBUS Signal handler
#define SIGNAL_METADATA    0x01 // Metadata changed
#define SIGNAL_PLAYSTATUS  0x02 // Playback Status Changed
#define SIGNAL_SEEKSTATUS  0x04 // Seek Status Changed
#define SIGNAL_SEEK        0x08 // Seeked
#define SIGNAL_CONTROLS    0x10 // Playback controls need to be updated (CanGoNext/Previous)
#define SIGNAL_VOLUME      0x20 // Volume needs to be updated
#define SIGNAL_ALL         0xFF // All Signals

#include "chips/mamedef.h"

void DBus_ReadWriteDispatch(void);
void DBus_EmitSignal(UINT8 type);

#endif	// __DBUS_H__
