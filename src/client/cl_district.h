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
void CL_District_Init( void );
void CL_District_Frame( void );
void CL_District_AddRefEntitiesToScene( void );
#else
static inline void CL_District_Init( void ) {}
static inline void CL_District_Frame( void ) {}
static inline void CL_District_AddRefEntitiesToScene( void ) {}
#endif

#ifdef __cplusplus
}
#endif
