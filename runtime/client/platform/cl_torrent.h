/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional peer-assisted package delivery backend.
===========================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_Torrent_Init( void );
void CL_Torrent_Shutdown( void );
qboolean CL_Torrent_Available( void );
qboolean CL_Torrent_IsPackageURL( const char *url );
qboolean CL_Torrent_BeginPackageDownload( const char *localName, const char *packageURL );

#ifdef __cplusplus
}
#endif
