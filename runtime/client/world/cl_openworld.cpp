/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client open-world bridge: per-chunk nav tiles + billboard scatter residency.
===========================================================================
*/

extern "C" {
#include "client.h"
#include "cl_engine_sprites.h"
#include "../../world/world_open.h"
#include "../../world/world_proc.h"
#include "../../world/world_residency.h"
#include "../../navigation/nav_recast.h"
#include "engine_sprite_map.h"
#include "cm_stream.h"
#include "cm_public.h"
#include "cluster_graph.h"
}

#include <cstring>

#define CL_OPENWORLD_SPRITE_SECTORS 64
#define CL_OPENWORLD_SPRITES_PER_SECTOR MAX_ENGINE_MAP_SPRITES

typedef struct {
	qboolean             active;
	int                  cellX;
	int                  cellY;
	engineSpriteDesc_t   sprites[CL_OPENWORLD_SPRITES_PER_SECTOR];
	int                  spriteCount;
} clOpenWorldSpriteSector_t;

static clOpenWorldSpriteSector_t spriteSectors[CL_OPENWORLD_SPRITE_SECTORS];
static navMeshHandle_t openWorldNavMesh = -1;
static cvar_t *cl_openWorldSync;
static char cl_openWorldLastSync[256];

#define CL_OPENWORLD_SYNC_MAX 64
#define CL_OPENWORLD_BSP_PATCHES 64

typedef struct {
	int cellX;
	int cellY;
} clOpenWorldCell_t;

static int CL_OpenWorld_ParseSectorList( const char *sectorList, clOpenWorldCell_t *cells, int maxCells ) {
	char buf[256];
	char *p;
	char *tok;
	int count = 0;

	if ( !sectorList || !sectorList[0] || !cells || maxCells < 1 ) {
		return 0;
	}
	Q_strncpyz( buf, sectorList, sizeof( buf ) );
	for ( p = buf; *p; p++ ) {
		if ( *p == ',' ) {
			*p = ' ';
		}
	}
	p = buf;
	while ( *p && count < maxCells ) {
		int cellX = 0, cellY = 0;

		while ( *p == ' ' ) {
			p++;
		}
		if ( !*p ) {
			break;
		}
		tok = p;
		while ( *p && *p != ' ' ) {
			p++;
		}
		if ( *p ) {
			*p++ = '\0';
		}
		if ( sscanf( tok, "%d_%d", &cellX, &cellY ) == 2 ) {
			cells[count].cellX = cellX;
			cells[count].cellY = cellY;
			count++;
		}
	}
	return count;
}

static qboolean CL_OpenWorld_CellInList( int cellX, int cellY, const clOpenWorldCell_t *cells, int count ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		if ( cells[i].cellX == cellX && cells[i].cellY == cellY ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void CL_OpenWorld_SyncUnloadRemoved( const clOpenWorldCell_t *newCells, int newCount ) {
	clOpenWorldCell_t oldCells[CL_OPENWORLD_SYNC_MAX];
	int oldCount;
	int i, n;
	const worldOpenSector_t *sec;
	worldOpenLayerMask_t syncMask = 0;

	if ( Cvar_VariableIntegerValue( "cm_openWorldCollision" ) ) {
		syncMask |= WO_LAYER_MASK_COLLISION;
	}
	if ( Cvar_VariableIntegerValue( "r_openWorldNav" ) ) {
		syncMask |= WO_LAYER_MASK_NAV;
	}

	oldCount = CL_OpenWorld_ParseSectorList( cl_openWorldLastSync, oldCells, CL_OPENWORLD_SYNC_MAX );
	for ( i = 0; i < oldCount; i++ ) {
		if ( !CL_OpenWorld_CellInList( oldCells[i].cellX, oldCells[i].cellY, newCells, newCount ) ) {
			WorldOpen_UnloadSectorLayers( oldCells[i].cellX, oldCells[i].cellY, syncMask );
		}
	}

	n = WorldOpen_GetSectorCount();
	for ( i = 0; i < n; i++ ) {
		sec = WorldOpen_GetSector( i );
		if ( !sec || !sec->active ) {
			continue;
		}
		if ( !CL_OpenWorld_CellInList( sec->cellX, sec->cellY, newCells, newCount ) &&
			CL_OpenWorld_CellInList( sec->cellX, sec->cellY, oldCells, oldCount ) ) {
			WorldOpen_UnloadSectorLayers( sec->cellX, sec->cellY, syncMask );
		}
	}
}
static int cl_bspMerged[CL_OPENWORLD_BSP_PATCHES][2];
static int cl_bspMergedCount;

static void CL_OpenWorld_MergeVisual( int cellX, int cellY ) {
	float sectorSize;

	if ( !Cvar_VariableIntegerValue( "r_bspStream" ) || !re.BspStreamMergeSector ) {
		return;
	}
	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}
	re.BspStreamMergeSector( cellX, cellY, sectorSize );
}

static void CL_OpenWorld_UnmergeVisual( int cellX, int cellY ) {
	if ( re.BspStreamUnmergeSector ) {
		re.BspStreamUnmergeSector( cellX, cellY );
	}
}

static qboolean CL_OpenWorld_IsBspMerged( int cellX, int cellY ) {
	int i;

	for ( i = 0; i < cl_bspMergedCount; i++ ) {
		if ( cl_bspMerged[i][0] == cellX && cl_bspMerged[i][1] == cellY ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void CL_OpenWorld_SetBspMerged( int cellX, int cellY, qboolean merged ) {
	int i;

	if ( merged ) {
		if ( CL_OpenWorld_IsBspMerged( cellX, cellY ) ) {
			return;
		}
		if ( cl_bspMergedCount < CL_OPENWORLD_BSP_PATCHES ) {
			cl_bspMerged[cl_bspMergedCount][0] = cellX;
			cl_bspMerged[cl_bspMergedCount][1] = cellY;
			cl_bspMergedCount++;
		}
		CL_OpenWorld_MergeVisual( cellX, cellY );
		return;
	}

	for ( i = 0; i < cl_bspMergedCount; i++ ) {
		if ( cl_bspMerged[i][0] == cellX && cl_bspMerged[i][1] == cellY ) {
			cl_bspMerged[i][0] = cl_bspMerged[cl_bspMergedCount - 1][0];
			cl_bspMerged[i][1] = cl_bspMerged[cl_bspMergedCount - 1][1];
			cl_bspMergedCount--;
			break;
		}
	}
	CL_OpenWorld_UnmergeVisual( cellX, cellY );
}

static void CL_OpenWorld_UpdateBspStream( void ) {
	int i, n;
	int current[CL_OPENWORLD_BSP_PATCHES][2];
	int currentCount = 0;
	const worldOpenSector_t *sec;

	if ( !Cvar_VariableIntegerValue( "r_bspStream" ) ) {
		return;
	}

	n = WorldOpen_GetSectorCount();
	for ( i = 0; i < n; i++ ) {
		sec = WorldOpen_GetSector( i );
		if ( !sec || !sec->active || !sec->collision ) {
			continue;
		}
		if ( currentCount < CL_OPENWORLD_BSP_PATCHES ) {
			current[currentCount][0] = sec->cellX;
			current[currentCount][1] = sec->cellY;
			currentCount++;
		}
	}

	for ( i = 0; i < cl_bspMergedCount; i++ ) {
		int j, found = 0;
		for ( j = 0; j < currentCount; j++ ) {
			if ( cl_bspMerged[i][0] == current[j][0] && cl_bspMerged[i][1] == current[j][1] ) {
				found = 1;
				break;
			}
		}
		if ( !found ) {
			CL_OpenWorld_SetBspMerged( cl_bspMerged[i][0], cl_bspMerged[i][1], qfalse );
			i--;
		}
	}
	for ( i = 0; i < currentCount; i++ ) {
		if ( !CL_OpenWorld_IsBspMerged( current[i][0], current[i][1] ) ) {
			CL_OpenWorld_SetBspMerged( current[i][0], current[i][1], qtrue );
		}
	}
}

static void CL_OpenWorld_LoadSectorCells( const char *sectorList, worldOpenLayerMask_t layerMask ) {
	clOpenWorldCell_t cells[CL_OPENWORLD_SYNC_MAX];
	int count;
	int i;

	count = CL_OpenWorld_ParseSectorList( sectorList, cells, CL_OPENWORLD_SYNC_MAX );
	for ( i = 0; i < count; i++ ) {
		WorldOpen_LoadSector( cells[i].cellX, cells[i].cellY, layerMask );
	}
}

static int CL_OpenWorld_FindSpriteSector( int cellX, int cellY ) {
	int i;

	for ( i = 0; i < CL_OPENWORLD_SPRITE_SECTORS; i++ ) {
		if ( spriteSectors[i].active && spriteSectors[i].cellX == cellX &&
			spriteSectors[i].cellY == cellY ) {
			return i;
		}
	}
	return -1;
}

static int CL_OpenWorld_AllocSpriteSector( int cellX, int cellY ) {
	int i;

	i = CL_OpenWorld_FindSpriteSector( cellX, cellY );
	if ( i >= 0 ) {
		return i;
	}
	for ( i = 0; i < CL_OPENWORLD_SPRITE_SECTORS; i++ ) {
		if ( !spriteSectors[i].active ) {
			spriteSectors[i].active = qtrue;
			spriteSectors[i].cellX = cellX;
			spriteSectors[i].cellY = cellY;
			spriteSectors[i].spriteCount = 0;
			return i;
		}
	}
	return -1;
}

static void CL_OpenWorld_MapDefToDesc( const engineSpriteMapDef_t *def, engineSpriteDesc_t *desc ) {
	if ( !def || !desc || !re.RegisterShader ) {
		return;
	}
	Com_Memset( desc, 0, sizeof( *desc ) );
	desc->type = def->type;
	VectorCopy( def->origin, desc->origin );
	desc->radius = def->radius;
	desc->rotation = def->rotation;
	desc->cols = def->cols;
	desc->rows = def->rows;
	desc->fps = def->fps;
	desc->swayAmount = def->swayAmount;
	desc->swaySpeed = def->swaySpeed;
	if ( def->shader[0] ) {
		desc->shader = re.RegisterShader( def->shader );
	}
}

static qboolean CL_OpenWorld_ReadScatterFile( const char *path, void **buf, int *len ) {
	*buf = NULL;
	*len = FS_ReadFile( path, buf );
	return *len > 0 && *buf;
}

static qboolean CL_OpenWorld_LoadSprites( int cellX, int cellY ) {
	char path[MAX_QPATH];
	char altPath[MAX_QPATH];
	void *buf;
	int len;
	engineSpriteMapList_t list;
	int idx;
	int i;
	float sectorSize;

	idx = CL_OpenWorld_AllocSpriteSector( cellX, cellY );
	if ( idx < 0 ) {
		return qfalse;
	}

	WorldProc_FormatScatterSectorPath( cellX, cellY, path, sizeof( path ) );
	if ( !CL_OpenWorld_ReadScatterFile( path, &buf, &len ) &&
		Cvar_VariableIntegerValue( "r_proc" ) &&
		Cvar_VariableIntegerValue( "r_procScatterRegion" ) ) {
		sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
		if ( sectorSize < 256.0f ) {
			sectorSize = Cvar_VariableValue( "r_procScale" );
		}
		WorldProc_FormatScatterRegionPath( cellX, cellY, sectorSize, altPath, sizeof( altPath ) );
		if ( CL_OpenWorld_ReadScatterFile( altPath, &buf, &len ) ) {
			Q_strncpyz( path, altPath, sizeof( path ) );
			Com_DPrintf( "[world_proc] scatter fallback %s\n", path );
		} else {
			WorldProc_FormatScatterPalettePath( cellX, cellY, sectorSize, altPath, sizeof( altPath ) );
			if ( CL_OpenWorld_ReadScatterFile( altPath, &buf, &len ) ) {
				Q_strncpyz( path, altPath, sizeof( path ) );
				Com_DPrintf( "[world_proc] scatter fallback %s\n", path );
			}
		}
	}
	if ( len <= 0 || !buf ) {
		Com_DPrintf( "[world_open] no scatter %s\n", path );
		spriteSectors[idx].spriteCount = 0;
		return qfalse;
	}

	EngineSpriteMap_Parse( (const char *)buf, &list );
	FS_FreeFile( buf );

	spriteSectors[idx].spriteCount = 0;
	for ( i = 0; i < list.count && i < CL_OPENWORLD_SPRITES_PER_SECTOR; i++ ) {
		CL_OpenWorld_MapDefToDesc( &list.defs[i], &spriteSectors[idx].sprites[i] );
		if ( spriteSectors[idx].sprites[i].shader ) {
			spriteSectors[idx].spriteCount++;
		}
	}

	Com_Printf( "[world_open] scatter %d,%d: %d billboard(s) from %s\n",
		cellX, cellY, spriteSectors[idx].spriteCount, path );
	return spriteSectors[idx].spriteCount > 0;
}

static void CL_OpenWorld_UnloadSprites( int cellX, int cellY ) {
	int idx = CL_OpenWorld_FindSpriteSector( cellX, cellY );

	if ( idx < 0 ) {
		return;
	}
	spriteSectors[idx].active = qfalse;
	spriteSectors[idx].spriteCount = 0;
}

static qboolean CL_OpenWorld_SectorLoad( int cellX, int cellY, worldOpenLayer_t layer ) {
	switch ( layer ) {
		case WO_LAYER_NAV:
			if ( openWorldNavMesh < 0 ) {
				openWorldNavMesh = Nav_CreateOpenWorldMesh();
			}
			if ( openWorldNavMesh < 0 ) {
				return qfalse;
			}
			return Nav_LoadSectorTile( openWorldNavMesh, cellX, cellY );
		case WO_LAYER_SPRITES:
			return CL_OpenWorld_LoadSprites( cellX, cellY );
		default:
			return qtrue;
	}
}

static void CL_OpenWorld_SectorUnload( int cellX, int cellY, worldOpenLayer_t layer ) {
	switch ( layer ) {
		case WO_LAYER_NAV:
			if ( openWorldNavMesh >= 0 ) {
				Nav_UnloadSectorTile( openWorldNavMesh, cellX, cellY );
			}
			break;
		case WO_LAYER_SPRITES:
			CL_OpenWorld_UnloadSprites( cellX, cellY );
			break;
		default:
			break;
	}
}

static void CL_OpenWorld_DrawSprites( void ) {
	int i, s;

	if ( !re.AddEngineSpriteToSceneAtTime && !re.AddEngineSpriteToScene ) {
		return;
	}
	for ( i = 0; i < CL_OPENWORLD_SPRITE_SECTORS; i++ ) {
		if ( !spriteSectors[i].active || spriteSectors[i].spriteCount <= 0 ) {
			continue;
		}
		for ( s = 0; s < spriteSectors[i].spriteCount; s++ ) {
			CL_EngineSprite_AddLocalAtTime( &spriteSectors[i].sprites[s], cls.realtime );
		}
	}
}

static void CL_OpenWorld_Start_f( void ) {
	Cvar_Set( "r_openWorld", "1" );
	Cvar_Set( "cm_stream", "1" );
	Cvar_Set( "cm_streamMerge", "1" );
	WorldOpen_Enable( qtrue );
	openWorldNavMesh = Nav_CreateOpenWorldMesh();
	Com_Printf( "[world_open] started (nav mesh %d)\n", openWorldNavMesh );
}

static void CL_OpenWorld_Stop_f( void ) {
	WorldOpen_Enable( qfalse );
	Cvar_Set( "r_openWorld", "0" );
	cl_openWorldLastSync[0] = '\0';
}

static void CL_OpenWorld_List_f( void ) {
	WorldOpen_List();
}

static void CL_OpenWorld_Status_f( void ) {
	WorldOpen_Status();
	Com_Printf( "  client nav mesh: %d\n", openWorldNavMesh );
}

static void CL_OpenWorld_BakeNavSector_f( void ) {
	int cellX, cellY;
	float sectorSize;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: nav_bake_sector <cellX> <cellY>\n" );
		return;
	}
	cellX = atoi( Cmd_Argv( 1 ) );
	cellY = atoi( Cmd_Argv( 2 ) );
	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( Nav_BakeSectorTile( cellX, cellY, sectorSize, NULL ) ) {
		if ( openWorldNavMesh < 0 ) {
			openWorldNavMesh = Nav_CreateOpenWorldMesh();
		}
		if ( openWorldNavMesh >= 0 ) {
			Nav_LoadSectorTile( openWorldNavMesh, cellX, cellY );
		}
	}
}

static void CL_OpenWorld_BakeNavView_f( void ) {
	float sectorSize;
	int cellX, cellY;

	if ( !cl.snap.valid ) {
		Com_Printf( "nav_bake_view: no active snapshot\n" );
		return;
	}
	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}
	cellX = (int)floor( cl.snap.ps.origin[0] / sectorSize );
	cellY = (int)floor( cl.snap.ps.origin[1] / sectorSize );
	if ( Nav_BakeSectorTile( cellX, cellY, sectorSize, NULL ) ) {
		if ( openWorldNavMesh < 0 ) {
			openWorldNavMesh = Nav_CreateOpenWorldMesh();
		}
		if ( openWorldNavMesh >= 0 ) {
			Nav_LoadSectorTile( openWorldNavMesh, cellX, cellY );
		}
	}
	Com_Printf( "[world_open] nav_bake_view -> sector %d,%d\n", cellX, cellY );
}

static void CL_OpenWorld_LoadSector_f( void ) {
	int cellX, cellY;
	worldOpenLayerMask_t mask = WO_LAYER_MASK_NAV | WO_LAYER_MASK_SPRITES;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: openworld_sector <cellX> <cellY>\n" );
		return;
	}
	if ( !WorldOpen_IsEnabled() ) {
		WorldOpen_Enable( qtrue );
	}
	cellX = atoi( Cmd_Argv( 1 ) );
	cellY = atoi( Cmd_Argv( 2 ) );
	if ( Cvar_VariableIntegerValue( "cm_openWorldCollision" ) ) {
		mask |= WO_LAYER_MASK_COLLISION;
	}
	WorldOpen_LoadSector( cellX, cellY, mask );
}

extern "C" void CL_OpenWorld_OnConfigstring( const char *sectorList ) {
	clOpenWorldCell_t newCells[CL_OPENWORLD_SYNC_MAX];
	worldResidencyCell_t allowCells[CL_OPENWORLD_SYNC_MAX];
	int newCount;
	worldOpenLayerMask_t layerMask = 0;
	int i;

	if ( !cl_openWorldSync || !cl_openWorldSync->integer ) {
		return;
	}
	if ( !sectorList ) {
		sectorList = "";
	}
	if ( !sectorList[0] ) {
		if ( cl_openWorldLastSync[0] ) {
			clOpenWorldCell_t keep[1];
			CL_OpenWorld_SyncUnloadRemoved( keep, 0 );
			cl_openWorldLastSync[0] = '\0';
		}
		return;
	}
	if ( !Cvar_VariableIntegerValue( "r_openWorld" ) ) {
		Com_DPrintf( "[world_open] ignoring sector sync (r_openWorld 0): %s\n", sectorList );
		return;
	}
	if ( !strcmp( sectorList, cl_openWorldLastSync ) ) {
		return;
	}
	if ( !WorldOpen_IsEnabled() ) {
		WorldOpen_Enable( qtrue );
	}

	newCount = CL_OpenWorld_ParseSectorList( sectorList, newCells, CL_OPENWORLD_SYNC_MAX );
	for ( i = 0; i < newCount; i++ ) {
		allowCells[i].cellX = newCells[i].cellX;
		allowCells[i].cellY = newCells[i].cellY;
	}
	WorldResidency_SetServerCollisionAllowList( allowCells, newCount );

	CL_OpenWorld_SyncUnloadRemoved( newCells, newCount );

	if ( Cvar_VariableIntegerValue( "cm_openWorldCollision" ) ) {
		layerMask |= WO_LAYER_MASK_COLLISION;
	}
	if ( Cvar_VariableIntegerValue( "r_openWorldNav" ) ) {
		qboolean residencyNavLocal = Cvar_VariableIntegerValue( "cl_openWorldResidencyNavLocal" ) != 0;
		if ( !WorldResidency_IsEnabled() || residencyNavLocal ) {
			layerMask |= WO_LAYER_MASK_NAV;
		}
	}
	CL_OpenWorld_LoadSectorCells( sectorList, layerMask );

	Q_strncpyz( cl_openWorldLastSync, sectorList, sizeof( cl_openWorldLastSync ) );
	Com_DPrintf( "[world_open] MP sector sync: %s (residency allow=%d)\n", sectorList, newCount );
}

extern "C" void CL_OpenWorld_Init( void ) {
	cl_openWorldSync = Cvar_Get( "cl_openWorldSync", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_openWorldSync,
		"Apply CS_ENGINE_OPENWORLD_SECTORS from server (collision + nav when enabled)." );

	WorldOpen_Init();
	WorldOpen_SetSectorLoad( CL_OpenWorld_SectorLoad );
	WorldOpen_SetSectorUnload( CL_OpenWorld_SectorUnload );

	Cmd_AddCommand( "openworld_start", CL_OpenWorld_Start_f );
	Cmd_AddCommand( "openworld_stop", CL_OpenWorld_Stop_f );
	Cmd_AddCommand( "openworld_list", CL_OpenWorld_List_f );
	Cmd_AddCommand( "openworld_status", CL_OpenWorld_Status_f );
	Cmd_AddCommand( "openworld_sector", CL_OpenWorld_LoadSector_f );
	Cmd_AddCommand( "nav_bake_sector", CL_OpenWorld_BakeNavSector_f );
	Cmd_AddCommand( "nav_bake_view", CL_OpenWorld_BakeNavView_f );

	Com_Printf( "Open world: openworld_start, nav_bake_sector, openworld_sector (r_openWorld)\n" );
}

extern "C" void CL_OpenWorld_Frame( void ) {
	if ( !WorldOpen_IsEnabled() || !cl.snap.valid ) {
		return;
	}
	WorldOpen_UpdateView( cl.snap.ps.origin, 0.0f );
	if ( ClusterGraph_ReachEnabled() ) {
		const int leafnum = CM_PointLeafnum( cl.snap.ps.origin );
		const int cluster = CM_LeafCluster( leafnum );
		ClusterGraph_UpdateReachability( cluster, -1 );
	}
	CL_OpenWorld_UpdateBspStream();
	CL_OpenWorld_DrawSprites();
}
