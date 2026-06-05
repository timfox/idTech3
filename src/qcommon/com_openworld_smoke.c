/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Runtime smoke test for open-world sector collision merge (CI / dedicated).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "cm_stream.h"

static cvar_t *com_openWorldSmoke;

static void Com_OpenWorld_Smoke_f( void ) {
	trace_t trace;
	vec3_t start, end, mins, maxs;
	int contents;
	qboolean sectorOk;

	if ( !com_openWorldSmoke || !com_openWorldSmoke->integer ) {
		Com_Printf( "OPENWORLD_SMOKE: disabled (com_openWorldSmoke 0)\n" );
		return;
	}

	Cvar_Set( "cm_stream", "1" );
	Cvar_Set( "cm_streamMerge", "1" );

	sectorOk = CM_Stream_LoadSector( 0, 0 );
	if ( !sectorOk ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL sector load (maps/sector_0_0.bsp + cm_stream)\n" );
		return;
	}

	VectorSet( start, 2048.0f, 2048.0f, 256.0f );
	VectorSet( end, 2048.0f, 2048.0f, 0.0f );
	CM_BoxTrace( &trace, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );

	if ( trace.allsolid || trace.startsolid ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL trace starts in solid\n" );
		return;
	}
	if ( trace.fraction >= 1.0f ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL sector platform trace missed (fraction 1)\n" );
		return;
	}
	if ( trace.endpos[2] < 120.0f || trace.endpos[2] > 136.0f ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL bad hit z=%.2f (expected ~128)\n", trace.endpos[2] );
		return;
	}

	VectorSet( mins, 2048.0f, 2048.0f, 64.0f );
	VectorSet( maxs, 2048.0f, 2048.0f, 64.0f );
	contents = CM_PointContents( mins, 0 );
	if ( !( contents & CONTENTS_SOLID ) ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL point not solid at platform (contents 0x%x)\n", contents );
		return;
	}

	Com_Printf( "OPENWORLD_SMOKE: OK sector trace z=%.1f merged=%d\n",
		trace.endpos[2], CM_Stream_IsSectorLoaded( 0, 0 ) );
}

void Com_OpenWorld_Smoke_Init( void ) {
	com_openWorldSmoke = Cvar_Get( "com_openWorldSmoke", "0", CVAR_TEMP );
	Cvar_SetDescription( com_openWorldSmoke,
		"Enable openworld_smoke command (CI runtime validation)." );
	Cmd_AddCommand( "openworld_smoke", Com_OpenWorld_Smoke_f );
}
