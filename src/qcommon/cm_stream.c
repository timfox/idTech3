/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "cm_stream.h"

#define CM_STREAM_GRID 32
#define CM_STREAM_SECTORS (CM_STREAM_GRID * CM_STREAM_GRID)

static qboolean s_sectorLoaded[CM_STREAM_SECTORS];
static cm_stream_prefetch_f s_prefetchHandler;
static cvar_t *cm_stream;
static cvar_t *sv_sectorURL;
static cvar_t *cl_sectorPrefetch;

void CM_Stream_Init( void ) {
	cm_stream = Cvar_Get( "cm_stream", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cm_stream,
		"Enable sector BSP streaming (maps/sector_X_Y.bsp). Default off." );
	sv_sectorURL = Cvar_Get( "sv_sectorURL", "", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_sectorURL,
		"Base URL for sector pk3 autodownload (see docs/CURL_NETWORKING.md)." );
	cl_sectorPrefetch = Cvar_Get( "cl_sectorPrefetch", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_sectorPrefetch,
		"Prefetch adjacent sector pk3 URLs when crossing boundaries (requires sv_sectorURL)." );
	Com_Memset( s_sectorLoaded, 0, sizeof( s_sectorLoaded ) );
	s_prefetchHandler = NULL;
	if ( cm_stream->integer ) {
		Com_Printf( "[cm_stream] cm_stream=1 (sector BSP streaming v1)\n" );
	}
}

void CM_Stream_SetPrefetchHandler( cm_stream_prefetch_f handler ) {
	s_prefetchHandler = handler;
}

static int CM_Stream_Index( int cellX, int cellY ) {
	if ( cellX < 0 || cellY < 0 || cellX >= CM_STREAM_GRID || cellY >= CM_STREAM_GRID ) {
		return -1;
	}
	return cellY * CM_STREAM_GRID + cellX;
}

qboolean CM_Stream_IsSectorLoaded( int cellX, int cellY ) {
	int idx = CM_Stream_Index( cellX, cellY );
	if ( idx < 0 ) {
		return qfalse;
	}
	return s_sectorLoaded[idx];
}

qboolean CM_Stream_LoadSector( int cellX, int cellY ) {
	char mapName[MAX_QPATH];
	int idx;
	int checksum;

	if ( !cm_stream || !cm_stream->integer ) {
		return qfalse;
	}

	idx = CM_Stream_Index( cellX, cellY );
	if ( idx < 0 || s_sectorLoaded[idx] ) {
		return s_sectorLoaded[idx >= 0 ? idx : 0];
	}

	Com_sprintf( mapName, sizeof( mapName ), "maps/sector_%d_%d", cellX, cellY );
	if ( !FS_FileExists( mapName ) ) {
		if ( sv_sectorURL && sv_sectorURL->string[0] ) {
			Com_Printf( "[cm_stream] sector %d,%d missing — prefetch via sv_sectorURL (see cl_sectorPrefetch)\n",
				cellX, cellY );
		}
		return qfalse;
	}

	CM_LoadMap( mapName, qtrue, &checksum );
	s_sectorLoaded[idx] = qtrue;
	Com_Printf( "[cm_stream] loaded sector %d,%d (%s)\n", cellX, cellY, mapName );

	if ( cl_sectorPrefetch && cl_sectorPrefetch->integer ) {
		static const int neighbors[8][2] = {
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
			{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
		};
		int n;

		for ( n = 0; n < 8; ++n ) {
			int nx = cellX + neighbors[n][0];
			int ny = cellY + neighbors[n][1];
			if ( !CM_Stream_IsSectorLoaded( nx, ny ) ) {
				CM_Stream_PrefetchSectorPk3( nx, ny );
			}
		}
	}
	return qtrue;
}

void CM_Stream_UnloadSector( int cellX, int cellY ) {
	int idx = CM_Stream_Index( cellX, cellY );
	if ( idx < 0 ) {
		return;
	}
	s_sectorLoaded[idx] = qfalse;
}

void CM_Stream_PrefetchSectorPk3( int cellX, int cellY ) {
	char url[MAX_OSPATH];
	char localName[MAX_QPATH];

	if ( !sv_sectorURL || !sv_sectorURL->string[0] ) {
		return;
	}
	Com_sprintf( localName, sizeof( localName ), "sector_%d_%d.pk3", cellX, cellY );
	Com_sprintf( url, sizeof( url ), "%s/%s", sv_sectorURL->string, localName );
	if ( FS_FileExists( localName ) ) {
		return;
	}
	if ( s_prefetchHandler ) {
		s_prefetchHandler( localName, url );
	} else {
		Com_Printf( "[cm_stream] prefetch: %s (no client handler; set sv_sectorURL + client CURL)\n", url );
	}
}
