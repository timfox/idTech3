/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void CL_OpenWorld_Init( void );
void CL_OpenWorld_Frame( void );
void CL_OpenWorld_OnConfigstring( const char *sectorList );

#ifdef __cplusplus
}
#endif
