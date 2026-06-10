/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client per-frame loop (split from cl_main.c).
===========================================================================
*/
#pragma once

#include "../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_Frame( int msec, int realMsec );
qboolean CL_CheckPaused( void );
qboolean CL_NoDelay( void );

#ifdef __cplusplus
}
#endif
