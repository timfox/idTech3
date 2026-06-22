/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Dedicated-server open-world collision sector residency (cm_stream merge).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_OPEN_WORLD
void SV_OpenWorld_Init( void );
void SV_OpenWorld_OnMapLoad( const char *mapname );
void SV_OpenWorld_Frame( void );
#else
static inline void SV_OpenWorld_Init( void ) {}
static inline void SV_OpenWorld_OnMapLoad( const char *mapname ) { (void)mapname; }
static inline void SV_OpenWorld_Frame( void ) {}
#endif

#ifdef __cplusplus
}
#endif
