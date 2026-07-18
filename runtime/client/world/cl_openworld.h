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
void CL_OpenWorld_Init( void );
void CL_OpenWorld_Frame( void );
void CL_OpenWorld_OnConfigstring( const char *sectorList );
void CL_WorldConfig_OnConfigstring( const char *payload );
#else
static inline void CL_OpenWorld_Init( void ) {}
static inline void CL_OpenWorld_Frame( void ) {}
static inline void CL_OpenWorld_OnConfigstring( const char *sectorList ) { (void)sectorList; }
static inline void CL_WorldConfig_OnConfigstring( const char *payload ) { (void)payload; }
#endif

#ifdef __cplusplus
}
#endif
