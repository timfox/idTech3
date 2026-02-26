/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Stub implementations for Android platform.
Provides minimal no-op implementations of functions that the engine
references but which have no equivalent on Android yet.
These will be replaced with real implementations as Android support
matures.
===========================================================================
*/

#ifdef __ANDROID__

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"

/* Platform stubs */
void Sys_UpdateWindowTitle( const char *title ) { (void)title; }
char *Sys_GetClipboardData( void ) { return NULL; }

/* Sound stubs (Android will use OpenSL ES or AAudio in future) */
typedef struct {
	int speed;
	int channels;
	int samplebits;
	int samples;
	int submission_chunk;
	byte *buffer;
} dma_t;

extern dma_t dma;

qboolean SNDDMA_Init( int sampleFrequencyInKHz ) { (void)sampleFrequencyInKHz; return qfalse; }
void SNDDMA_Shutdown( void ) {}
void SNDDMA_BeginPainting( void ) {}
int  SNDDMA_GetDMAPos( void ) { return 0; }
void SNDDMA_Submit( void ) {}

/* Navigation stubs (Recast not available on Android) */
#ifndef USE_RECAST_NAV
void Nav_Init( void ) {}
void Nav_Shutdown( void ) {}
int  Nav_BuildFromBSP( const char *mapName, void *params ) { (void)mapName; (void)params; return -1; }
void Nav_UpdateCrowd( int mesh, float dt ) { (void)mesh; (void)dt; }
void Nav_BSP_ClearGeometry( void ) {}
int  Nav_BSP_AddVertex( float x, float y, float z ) { (void)x; (void)y; (void)z; return 0; }
void Nav_BSP_AddTriangle( int v0, int v1, int v2 ) { (void)v0; (void)v1; (void)v2; }
#endif

#endif /* __ANDROID__ */
