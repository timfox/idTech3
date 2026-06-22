/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client connection: connect/disconnect, rcon, OOB packets, resend/timeout.
===========================================================================
*/

#pragma once

#include "../../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_Connect_Init( void );
void CL_Connect_ShutdownCommands( void );
void CL_Connect_Frame( void );
void CL_Connect_CheckForResend( void );
void CL_Connect_Reconnect( void );
void CL_Connect_SetShutdownQuit( qboolean quit );

#ifdef __cplusplus
}
#endif
