/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Runtime smoke tests for open-world sector streaming (CI / dedicated):
  collision merge, visual BSP lumps, MP sync list, residency stress.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "qfiles.h"
#include "cm_public.h"
#include "cm_stream.h"

static cvar_t *com_openWorldSmoke;

#define OPENWORLD_SMOKE_SECTOR_SIZE 4096.0f
#define OPENWORLD_SMOKE_PLATFORM_Z 128.0f

static qboolean Com_OpenWorld_SectorListContains( const char *list, int cellX, int cellY ) {
	char token[32];

	if ( !list ) {
		return qfalse;
	}
	Com_sprintf( token, sizeof( token ), "%d_%d", cellX, cellY );
	return strstr( list, token ) != NULL ? qtrue : qfalse;
}

static qboolean Com_OpenWorld_TraceSectorPlatform( int cellX, int cellY, float *hitZ ) {
	trace_t trace;
	vec3_t start, end;
	float wx;
	float wy;

	wx = (float)cellX * OPENWORLD_SMOKE_SECTOR_SIZE + 2048.0f;
	wy = (float)cellY * OPENWORLD_SMOKE_SECTOR_SIZE + 2048.0f;

	VectorSet( start, wx, wy, 256.0f );
	VectorSet( end, wx, wy, 0.0f );
	CM_BoxTrace( &trace, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );

	if ( trace.allsolid || trace.startsolid ) {
		return qfalse;
	}
	if ( trace.fraction >= 1.0f ) {
		return qfalse;
	}
	if ( trace.endpos[2] < OPENWORLD_SMOKE_PLATFORM_Z - 8.0f ||
		trace.endpos[2] > OPENWORLD_SMOKE_PLATFORM_Z + 8.0f ) {
		return qfalse;
	}
	if ( hitZ ) {
		*hitZ = trace.endpos[2];
	}
	return qtrue;
}

static qboolean Com_OpenWorld_BspHasVisualLumps( int cellX, int cellY, int *outDrawVerts, int *outSurfaces ) {
	char mapName[MAX_QPATH];
	byte *buf;
	int length;
	dheader_t header;
	int drawVerts;
	int surfaces;
	int lightmaps;

	Com_sprintf( mapName, sizeof( mapName ), "maps/sector_%d_%d.bsp", cellX, cellY );
	length = FS_ReadFile( mapName, (void **)&buf );
	if ( length <= 0 || !buf ) {
		return qfalse;
	}
	if ( (size_t)length < sizeof( header ) ) {
		FS_FreeFile( buf );
		return qfalse;
	}

	header = *(dheader_t *)buf;
	if ( LittleLong( header.ident ) != (int)BSP_IDENT || LittleLong( header.version ) != BSP_VERSION ) {
		FS_FreeFile( buf );
		return qfalse;
	}

	drawVerts = LittleLong( header.lumps[LUMP_DRAWVERTS].filelen );
	surfaces = LittleLong( header.lumps[LUMP_SURFACES].filelen );
	lightmaps = LittleLong( header.lumps[LUMP_LIGHTMAPS].filelen );
	FS_FreeFile( buf );

	if ( drawVerts <= 0 || surfaces <= 0 || lightmaps <= 0 ) {
		return qfalse;
	}
	if ( outDrawVerts ) {
		*outDrawVerts = drawVerts;
	}
	if ( outSurfaces ) {
		*outSurfaces = surfaces;
	}
	return qtrue;
}

static qboolean Com_OpenWorld_RunCollisionCore( void ) {
	vec3_t probe;
	int contents;
	float hitZ;

	if ( !CM_Stream_LoadSector( 0, 0 ) ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL sector load (maps/sector_0_0.bsp + cm_stream)\n" );
		return qfalse;
	}
	if ( !Com_OpenWorld_TraceSectorPlatform( 0, 0, &hitZ ) ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL sector platform trace (0,0)\n" );
		return qfalse;
	}

	VectorSet( probe, 2048.0f, 2048.0f, 64.0f );
	contents = CM_PointContents( probe, 0 );
	if ( !( contents & CONTENTS_SOLID ) ) {
		Com_Printf( "OPENWORLD_SMOKE: FAIL point not solid at platform (contents 0x%x)\n", contents );
		return qfalse;
	}

	Com_Printf( "OPENWORLD_SMOKE: OK sector trace z=%.1f merged=%d\n",
		hitZ, CM_Stream_IsSectorLoaded( 0, 0 ) );
	return qtrue;
}

static void Com_OpenWorld_Smoke_f( void ) {
	if ( !com_openWorldSmoke || !com_openWorldSmoke->integer ) {
		Com_Printf( "OPENWORLD_SMOKE: disabled (com_openWorldSmoke 0)\n" );
		return;
	}

	Cvar_Set( "cm_stream", "1" );
	Cvar_Set( "cm_streamMerge", "1" );

	{
		int checksum;
		CM_LoadMap( "maps/open_void.bsp", qfalse, &checksum );
	}

	(void)Com_OpenWorld_RunCollisionCore();
}

static void Com_OpenWorld_SmokeFidelity_f( void ) {
	char list[256];
	vec3_t viewOrigin;
	float hitZ;
	int drawVerts;
	int surfaces;
	int cycle;

	if ( !com_openWorldSmoke || !com_openWorldSmoke->integer ) {
		Com_Printf( "OPENWORLD_FIDELITY: disabled (com_openWorldSmoke 0)\n" );
		return;
	}

	Cvar_Set( "cm_stream", "1" );
	Cvar_Set( "cm_streamMerge", "1" );
	Cvar_Set( "cm_streamSectorSize", "4096" );

	{
		int checksum;
		CM_LoadMap( "maps/open_void.bsp", qfalse, &checksum );
	}

	/* Collision: multi-sector merge + world-space traces */
	if ( !CM_Stream_LoadSector( 0, 0 ) || !CM_Stream_LoadSector( 1, 0 ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL multi-sector load (0,0 + 1,0)\n" );
		return;
	}
	if ( !Com_OpenWorld_TraceSectorPlatform( 0, 0, &hitZ ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL trace sector (0,0)\n" );
		return;
	}
	if ( !Com_OpenWorld_TraceSectorPlatform( 1, 0, &hitZ ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL trace sector (1,0) world offset\n" );
		return;
	}

	CM_Stream_BuildLoadedList( list, sizeof( list ) );
	if ( !Com_OpenWorld_SectorListContains( list, 0, 0 ) ||
		!Com_OpenWorld_SectorListContains( list, 1, 0 ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL sync list after load got '%s'\n", list );
		return;
	}

	/* Visual BSP: authored drawVerts/surfaces/lightmaps for r_bspStream */
	if ( !Com_OpenWorld_BspHasVisualLumps( 0, 0, &drawVerts, &surfaces ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL visual lumps missing on sector (0,0)\n" );
		return;
	}
	if ( !Com_OpenWorld_BspHasVisualLumps( 1, 0, NULL, NULL ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL visual lumps missing on sector (1,0)\n" );
		return;
	}

	/* MP sync simulation: unload dropped sector, list must shrink */
	CM_Stream_UnloadSector( 1, 0 );
	CM_Stream_BuildLoadedList( list, sizeof( list ) );
	if ( !Com_OpenWorld_SectorListContains( list, 0, 0 ) ||
		Com_OpenWorld_SectorListContains( list, 1, 0 ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL sync list after unload got '%s'\n", list );
		return;
	}
	if ( !CM_Stream_LoadSector( 1, 0 ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL reload sector (1,0)\n" );
		return;
	}

	/* View residency: load around origin, verify neighbor sector present */
	VectorSet( viewOrigin, 2048.0f, 2048.0f, 256.0f );
	CM_Stream_UpdateView( viewOrigin, 12288.0f, OPENWORLD_SMOKE_SECTOR_SIZE, qtrue );
	CM_Stream_BuildLoadedList( list, sizeof( list ) );
	if ( !Com_OpenWorld_SectorListContains( list, 0, 0 ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL view residency missing home sector list='%s'\n", list );
		return;
	}

	/* Stress: churn sectors under load, collision must remain valid */
	for ( cycle = 0; cycle < 32; cycle++ ) {
		CM_Stream_UnloadSector( 0, 0 );
		CM_Stream_UnloadSector( 1, 0 );
		if ( !CM_Stream_LoadSector( 0, 0 ) || !CM_Stream_LoadSector( 1, 0 ) ) {
			Com_Printf( "OPENWORLD_FIDELITY: FAIL stress reload cycle %d\n", cycle );
			return;
		}
	}
	if ( !Com_OpenWorld_TraceSectorPlatform( 0, 0, &hitZ ) ||
		!Com_OpenWorld_TraceSectorPlatform( 1, 0, &hitZ ) ) {
		Com_Printf( "OPENWORLD_FIDELITY: FAIL trace after stress churn\n" );
		return;
	}

	CM_Stream_BuildLoadedList( list, sizeof( list ) );
	Com_Printf( "OPENWORLD_FIDELITY: OK collision+visual+sync stress list='%s' drawVerts=%d surfaces=%d\n",
		list, drawVerts, surfaces );
}

void Com_OpenWorld_Smoke_Init( void ) {
	com_openWorldSmoke = Cvar_Get( "com_openWorldSmoke", "0", CVAR_TEMP );
	Cvar_SetDescription( com_openWorldSmoke,
		"Enable openworld_smoke / openworld_smoke_fidelity commands (CI runtime validation)." );
	Cmd_AddCommand( "openworld_smoke", Com_OpenWorld_Smoke_f );
	Cmd_AddCommand( "openworld_smoke_fidelity", Com_OpenWorld_SmokeFidelity_f );
}
