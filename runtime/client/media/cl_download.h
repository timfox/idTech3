/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client pak/map download path (UDP server list + optional cURL).
===========================================================================
*/

#pragma once

#include "qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_Download_Init( void );
void CL_Download_Shutdown( void );
qboolean CL_Download_Frame( int msec, int realMsec );

#ifdef __cplusplus
}
#endif
