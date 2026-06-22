/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Renderer plugin load, GLimp cvars, video mode list, and vid_restart path.
===========================================================================
*/

#pragma once

#include "../../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_Ref_Init( void );
void CL_Ref_ShutdownCommands( void );
void CL_Ref_Shutdown( refShutdownCode_t code );
void CL_Ref_InitRenderer( void );
void CL_Ref_VidRestart( refShutdownCode_t shutdownCode );

#ifdef __cplusplus
}
#endif
