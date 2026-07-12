/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Server browser: local/global scan, ping queue, server status queries.
===========================================================================
*/

#pragma once

#include "qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_ServerBrowser_Init( void );
void CL_ServerBrowser_Shutdown( void );

void CL_ServerBrowser_InfoPacket( const netadr_t *from, msg_t *msg );
void CL_ServerBrowser_StatusResponse( const netadr_t *from, msg_t *msg );
void CL_ServerBrowser_ServersResponse( const netadr_t *from, msg_t *msg, qboolean extended );

#ifdef __cplusplus
}
#endif
