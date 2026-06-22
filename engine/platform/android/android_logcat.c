/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Strip Q3 color codes and print to Android logcat (tag idTech3).
===========================================================================
*/

#ifdef __ANDROID__

#include <android/log.h>
#include <string.h>

#define LOGCAT_TAG "idTech3"
#define LOGCAT_CHUNK 900

void Android_LogcatPrint( const char *msg ) {
	char chunk[LOGCAT_CHUNK + 8];
	int o;
	const char *s;

	if ( !msg || !msg[0] ) {
		return;
	}

	o = 0;
	for ( s = msg; *s; s++ ) {
		if ( *s == '^' && s[1] >= '0' && s[1] <= '9' ) {
			s++;
			continue;
		}
		if ( *s == '\r' ) {
			continue;
		}
		chunk[o++] = *s;
		if ( o >= LOGCAT_CHUNK ) {
			chunk[o] = '\0';
			__android_log_print( ANDROID_LOG_INFO, LOGCAT_TAG, "%s", chunk );
			o = 0;
		}
	}
	if ( o > 0 ) {
		chunk[o] = '\0';
		__android_log_print( ANDROID_LOG_INFO, LOGCAT_TAG, "%s", chunk );
	}
}

#endif
