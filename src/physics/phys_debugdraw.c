/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "phys_debugdraw.h"

static phys_debug_line_t lines[PHYS_DEBUG_MAX_LINES];
static int lineCount;

void PhysDebug_Clear( void ) {
	lineCount = 0;
}

void PhysDebug_AddLine( const vec3_t from, const vec3_t to, const vec3_t color ) {
	if ( lineCount >= PHYS_DEBUG_MAX_LINES ) {
		return;
	}
	VectorCopy( from, lines[lineCount].from );
	VectorCopy( to, lines[lineCount].to );
	VectorCopy( color, lines[lineCount].color );
	lineCount++;
}

int PhysDebug_GetLineCount( void ) {
	return lineCount;
}

const phys_debug_line_t *PhysDebug_GetLines( void ) {
	return lines;
}
