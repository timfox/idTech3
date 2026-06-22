/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client demo record/playback and demo AVI capture.
===========================================================================
*/
#pragma once
#include "../../qcommon/qcommon.h"
#ifdef __cplusplus
extern "C" {
#endif
void CL_Demo_Init( void );
void CL_Demo_Shutdown( void );
void CL_Demo_InitCommands( void );
void CL_Demo_ShutdownCommands( void );
void CL_Demo_Frame( int *msec, int *realMsec );
void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes );
#ifdef __cplusplus
}
#endif
