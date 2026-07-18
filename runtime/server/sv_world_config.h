/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_OPEN_WORLD
void SV_WorldConfig_Init( void );
void SV_WorldConfig_OnMapLoad( const char *mapname );
#else
static inline void SV_WorldConfig_Init( void ) {}
static inline void SV_WorldConfig_OnMapLoad( const char *mapname ) { (void)mapname; }
#endif

#ifdef __cplusplus
}
#endif
