/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "cm_stream.h"
#include "cm_stream_merge.h"
#include "../world/world_config.h"

#define CM_STREAM_GRID 32
#define CM_STREAM_SECTORS (CM_STREAM_GRID * CM_STREAM_GRID)

static qboolean s_sectorLoaded[CM_STREAM_SECTORS];
static qboolean s_classicBaseMap;
static cm_stream_prefetch_f s_prefetchHandler;
static cvar_t *cm_stream;
static cvar_t *sv_sectorURL;
static cvar_t *cl_sectorPrefetch;

static qboolean CM_Stream_MapNameIsSectorChunk( const char *mapBase ) {
	if ( !mapBase || !mapBase[0] ) {
		return qfalse;
	}
	return !Q_stricmpn( mapBase, "sector_", 7 );
}

qboolean CM_Stream_SectorOverlayPermitted( void ) {
	if ( Cvar_VariableIntegerValue( "com_openWorldSmoke" ) ) {
		return qtrue;
	}
	if ( s_classicBaseMap ) {
		if ( Cvar_VariableIntegerValue( "r_openWorld" ) ) {
			return qtrue;
		}
		return qfalse;
	}
	if ( cm_stream && cm_stream->integer ) {
		return qtrue;
	}
	if ( Cvar_VariableIntegerValue( "sv_openWorld" ) ) {
		return qtrue;
	}
	return qfalse;
}

void CM_Stream_Clear( void ) {
	Com_Memset( s_sectorLoaded, 0, sizeof( s_sectorLoaded ) );
	CM_Stream_Merge_ClearAll();
}

void CM_Stream_OnBaseMapLoad( const char *mapPath ) {
	const char *base;
	char mapBase[MAX_QPATH];
	int len;

	s_classicBaseMap = qtrue;
	if ( !mapPath || !mapPath[0] ) {
		return;
	}

	base = mapPath;
	if ( !Q_stricmpn( base, "maps/", 5 ) ) {
		base += 5;
	}
	Q_strncpyz( mapBase, base, sizeof( mapBase ) );
	len = (int)strlen( mapBase );
	if ( len > 4 && !Q_stricmp( mapBase + len - 4, ".bsp" ) ) {
		mapBase[len - 4] = '\0';
		len -= 4;
	}
	if ( CM_Stream_MapNameIsSectorChunk( mapBase ) ) {
		s_classicBaseMap = qfalse;
		return;
	}

	if ( Cvar_VariableIntegerValue( "cm_stream" ) ||
		Cvar_VariableIntegerValue( "cm_streamMerge" ) ||
		Cvar_VariableIntegerValue( "cm_openWorldCollision" ) ) {
		Com_Printf( "[cm_stream] classic map %s — sector overlays require r_openWorld 1\n", mapPath );
	}
}

void CM_Stream_Init( void ) {
	s_classicBaseMap = qtrue;
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
	CM_Stream_Merge_Init();
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

void CM_Stream_WorldToCell( const vec3_t origin, float sectorSize, int *cellX, int *cellY ) {
	float size;

	if ( !origin || !cellX || !cellY ) {
		return;
	}
	size = sectorSize;
	if ( size < 256.0f ) {
		size = 256.0f;
	}
	*cellX = (int)floor( origin[0] / size );
	*cellY = (int)floor( origin[1] / size );
}

void CM_Stream_UpdateView( const vec3_t viewOrigin, float radius, float sectorSize, qboolean loadCollision ) {
	int centerX, centerY;
	int minX, maxX, minY, maxY;
	int x, y;
	int cellRadius;

	if ( !cm_stream || !cm_stream->integer || !viewOrigin || radius <= 0.0f ) {
		return;
	}
	if ( !CM_Stream_SectorOverlayPermitted() ) {
		return;
	}

	CM_Stream_WorldToCell( viewOrigin, sectorSize, &centerX, &centerY );
	cellRadius = (int)ceil( radius / sectorSize );
	if ( cellRadius < 1 ) {
		cellRadius = 1;
	}

	minX = centerX - cellRadius;
	maxX = centerX + cellRadius;
	minY = centerY - cellRadius;
	maxY = centerY + cellRadius;

	for ( y = minY; y <= maxY; y++ ) {
		for ( x = minX; x <= maxX; x++ ) {
			vec3_t center;
			float dist;

			center[0] = ( (float)x + 0.5f ) * sectorSize;
			center[1] = ( (float)y + 0.5f ) * sectorSize;
			center[2] = viewOrigin[2];
			dist = Distance( viewOrigin, center );
			if ( dist <= radius ) {
				if ( loadCollision ) {
					(void)CM_Stream_LoadSector( x, y );
				} else {
					(void)CM_Stream_PrefetchSectorPk3( x, y );
				}
			} else if ( CM_Stream_IsSectorLoaded( x, y ) ) {
				CM_Stream_UnloadSector( x, y );
			}
		}
	}
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
	if ( !CM_Stream_SectorOverlayPermitted() ) {
		return qfalse;
	}

	idx = CM_Stream_Index( cellX, cellY );
	if ( idx < 0 ) {
		return qfalse;
	}
	if ( s_sectorLoaded[idx] ) {
		return qtrue;
	}

	{
		char preferred[MAX_QPATH];
		char fallback[MAX_QPATH];

		Com_sprintf( fallback, sizeof( fallback ), "maps/sector_%d_%d.bsp", cellX, cellY );
		WorldConfig_FormatSectorBsp( cellX, cellY, preferred, sizeof( preferred ) );
		if ( !WorldConfig_ResolveReadable( preferred, fallback, mapName, sizeof( mapName ) ) ) {
			if ( sv_sectorURL && sv_sectorURL->string[0] ) {
				Com_Printf( "[cm_stream] sector %d,%d missing — prefetch via sv_sectorURL (see cl_sectorPrefetch)\n",
					cellX, cellY );
			}
			return qfalse;
		}
	}

	if ( Cvar_VariableIntegerValue( "cm_streamMerge" ) ) {
		if ( !CM_Stream_MergeSector( cellX, cellY ) ) {
			return qfalse;
		}
	} else {
		CM_LoadMap( mapName, qtrue, &checksum );
	}
	s_sectorLoaded[idx] = qtrue;
	Com_Printf( "[cm_stream] loaded sector %d,%d (%s%s)\n", cellX, cellY, mapName,
		Cvar_VariableIntegerValue( "cm_streamMerge" ) ? ", merge overlay" : "" );

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
	if ( Cvar_VariableIntegerValue( "cm_streamMerge" ) ) {
		CM_Stream_UnmergeSector( cellX, cellY );
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

void CM_Stream_BuildLoadedList( char *buf, int bufSize ) {
	int x, y;
	int len = 0;
	char token[32];

	if ( !buf || bufSize < 1 ) {
		return;
	}
	buf[0] = '\0';
	if ( !cm_stream || !cm_stream->integer ) {
		return;
	}
	for ( y = 0; y < CM_STREAM_GRID; y++ ) {
		for ( x = 0; x < CM_STREAM_GRID; x++ ) {
			if ( !CM_Stream_IsSectorLoaded( x, y ) ) {
				continue;
			}
			Com_sprintf( token, sizeof( token ), "%s%d_%d", len ? "," : "", x, y );
			if ( len + (int)strlen( token ) + 1 >= bufSize ) {
				return;
			}
			Q_strcat( buf, bufSize, token );
			len += (int)strlen( token );
		}
	}
}
