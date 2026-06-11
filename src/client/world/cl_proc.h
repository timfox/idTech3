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
void CL_Proc_Init( void );
#else
static inline void CL_Proc_Init( void ) {}
#endif

#ifdef __cplusplus
}
#endif
