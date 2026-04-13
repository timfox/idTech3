/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Bridge engine console output to Android logcat (strip Q3 color codes).
===========================================================================
*/

#ifndef ANDROID_LOGCAT_H
#define ANDROID_LOGCAT_H

#ifdef __ANDROID__

void Android_LogcatPrint( const char *msg );

#else

static inline void Android_LogcatPrint( const char *msg ) {
	(void)msg;
}

#endif

#endif /* ANDROID_LOGCAT_H */
