/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/
#pragma once
#include "qcommon.h"
#ifdef __cplusplus
extern "C" {
#endif
void CL_Cmds_Init( void );
void CL_Cmds_Shutdown( void );
void CL_SocialOverlaySync( void );
qboolean CL_SocialOverlayHandleEscape( int key );
void CL_SocialOverlayReset( const char *reason );
#ifdef __cplusplus
}
#endif
