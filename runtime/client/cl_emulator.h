/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side idTech3 Emulator bridge: QEMU guest process + Vulkan screen texture.
===========================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_IDTECH3_EMULATOR

void CL_Emulator_Init( void );
void CL_Emulator_Shutdown( void );
void CL_Emulator_Frame( void );
qboolean CL_Emulator_KeyEvent( int key, qboolean down );
qboolean CL_Emulator_CharEvent( int key );

#else

static inline void CL_Emulator_Init( void ) {}
static inline void CL_Emulator_Shutdown( void ) {}
static inline void CL_Emulator_Frame( void ) {}
static inline qboolean CL_Emulator_KeyEvent( int key, qboolean down ) { (void)key; (void)down; return qfalse; }
static inline qboolean CL_Emulator_CharEvent( int key ) { (void)key; return qfalse; }

#endif

#ifdef __cplusplus
}
#endif
