/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Drain touch-overlay input (from JNI) into Sys_QueEvent / Cbuf on game thread.
===========================================================================
*/

#ifndef ANDROID_TOUCH_OVERLAY_H
#define ANDROID_TOUCH_OVERLAY_H

#ifdef __ANDROID__

void Android_TouchOverlay_PumpEvents( void );

#else

static inline void Android_TouchOverlay_PumpEvents( void ) {}

#endif

#endif
