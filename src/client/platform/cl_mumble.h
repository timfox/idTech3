/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mumble positional audio link.
Sends player position/orientation to Mumble for 3D voice positioning.
Uses Mumble's shared-memory Link protocol.
===========================================================================
*/

#ifndef CL_MUMBLE_H
#define CL_MUMBLE_H

#include "../qcommon/q_shared.h"

qboolean CL_Mumble_Init( void );
void     CL_Mumble_Shutdown( void );
void     CL_Mumble_Update( const vec3_t position, const vec3_t forward, const vec3_t up );

#endif /* CL_MUMBLE_H */
