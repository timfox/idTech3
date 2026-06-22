/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Bullet debug draw line buffer — collected by btIDebugDraw, submitted by
the client via AddPolyToScene during world render.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define PHYS_DEBUG_MAX_LINES 8192

typedef struct phys_debug_line_s {
	vec3_t  from;
	vec3_t  to;
	vec3_t  color;
} phys_debug_line_t;

void                PhysDebug_Clear( void );
void                PhysDebug_AddLine( const vec3_t from, const vec3_t to, const vec3_t color );
int                 PhysDebug_GetLineCount( void );
const phys_debug_line_t *PhysDebug_GetLines( void );

#ifdef __cplusplus
}
#endif
