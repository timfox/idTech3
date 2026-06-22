/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client lifecycle: memory flush, map load, hunk users, shutdown.
===========================================================================
*/
#pragma once

#include "../../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_ShutdownVMs( void );
void CL_ShutdownAll( void );
void CL_ClearMemory( void );
void CL_FlushMemory( void );
void CL_MapLoading( void );
void CL_StartHunkUsers( void );
void CL_Shutdown( const char *finalmsg, qboolean quit );

#ifdef __cplusplus
}
#endif
