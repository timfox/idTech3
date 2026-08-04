/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../../renderers/common/tr_types.h"

#ifdef USE_OPEN_WORLD
void CL_District_Init( void );
void CL_District_Frame( void );
void CL_District_AddRefEntitiesToScene( void );
void CL_District_ApplyView( refdef_t *fd );
void CL_District_RenderStandalone( void );
#else
static inline void CL_District_Init( void ) {}
static inline void CL_District_Frame( void ) {}
static inline void CL_District_AddRefEntitiesToScene( void ) {}
static inline void CL_District_ApplyView( refdef_t *fd ) { (void)fd; }
static inline void CL_District_RenderStandalone( void ) {}
#endif

#ifdef __cplusplus
}
#endif
