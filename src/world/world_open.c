/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_stream.h"
#include "world_open.h"
#include "world_proc.h"
#include "world_residency.h"

static worldOpenSector_t sectors[WORLD_OPEN_SECTOR_MAX];
static int sectorCount;
static qboolean openWorldEnabled;

static worldOpenSectorLoad_f sectorLoadFn;
static worldOpenSectorUnload_f sectorUnloadFn;

static cvar_t *r_openWorld;
static cvar_t *r_openWorldSectorSize;
static cvar_t *r_openWorldRadius;
static cvar_t *r_openWorldStream;
static cvar_t *r_openWorldNav;
static cvar_t *r_openWorldSprites;
static cvar_t *cm_openWorldCollision;

static int WorldOpen_FindSlot( int cellX, int cellY ) {
	int i;

	for ( i = 0; i < sectorCount; i++ ) {
		if ( sectors[i].active && sectors[i].cellX == cellX && sectors[i].cellY == cellY ) {
			return i;
		}
	}
	return -1;
}

static int WorldOpen_AllocSlot( int cellX, int cellY ) {
	int idx;

	idx = WorldOpen_FindSlot( cellX, cellY );
	if ( idx >= 0 ) {
		return idx;
	}
	if ( sectorCount >= WORLD_OPEN_SECTOR_MAX ) {
		Com_Printf( S_COLOR_YELLOW "[world_open] sector table full (%d,%d)\n", cellX, cellY );
		return -1;
	}
	idx = sectorCount++;
	Com_Memset( &sectors[idx], 0, sizeof( sectors[idx] ) );
	sectors[idx].active = qtrue;
	sectors[idx].cellX = cellX;
	sectors[idx].cellY = cellY;
	return idx;
}

void WorldOpen_Init( void ) {
	r_openWorld = Cvar_Get( "r_openWorld", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorld,
		"Enable infinite open-world residency (BSP sectors + per-chunk nav + billboard scatter)." );
	r_openWorldSectorSize = Cvar_Get( "r_openWorldSectorSize", "4096", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldSectorSize,
		"World units per streamed sector cell (maps/sector_X_Y.bsp, nav/sector_X_Y.nav, sprites/sector_X_Y.ents)." );
	r_openWorldRadius = Cvar_Get( "r_openWorldRadius", "12288", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldRadius,
		"View-driven residency radius for open-world layers." );
	r_openWorldStream = Cvar_Get( "r_openWorldStream", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldStream,
		"When 1, prefetch/load BSP sector pk3s via cm_stream around the view." );
	r_openWorldNav = Cvar_Get( "r_openWorldNav", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldNav,
		"When 1, load Detour nav tiles per sector (nav/sector_X_Y.nav)." );
	r_openWorldSprites = Cvar_Get( "r_openWorldSprites", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldSprites,
		"When 1, scatter misc_billboard props from sprites/sector_X_Y.ents per sector." );
	cm_openWorldCollision = Cvar_Get( "cm_openWorldCollision", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cm_openWorldCollision,
		"When 1 and cm_stream 1, merge sector BSP brushes (cm_streamMerge 1) or replace CM (cm_streamMerge 0)." );

	sectorCount = 0;
	openWorldEnabled = qfalse;
	sectorLoadFn = NULL;
	sectorUnloadFn = NULL;
	Com_Memset( sectors, 0, sizeof( sectors ) );
	WorldResidency_Init();
	Com_Printf( "[world_open] open-world streaming layer initialized\n" );
}

void WorldOpen_Shutdown( void ) {
	int i;

	for ( i = 0; i < sectorCount; i++ ) {
		if ( sectors[i].active ) {
			WorldOpen_UnloadSector( sectors[i].cellX, sectors[i].cellY );
		}
	}
	sectorCount = 0;
	openWorldEnabled = qfalse;
	WorldResidency_Shutdown();
}

void WorldOpen_SetSectorLoad( worldOpenSectorLoad_f fn ) {
	sectorLoadFn = fn;
}

void WorldOpen_SetSectorUnload( worldOpenSectorUnload_f fn ) {
	sectorUnloadFn = fn;
}

void WorldOpen_Enable( qboolean enable ) {
	openWorldEnabled = enable;
	if ( enable ) {
		Com_Printf( "[world_open] enabled (sector %s, radius %.0f)\n",
			r_openWorldSectorSize ? r_openWorldSectorSize->string : "4096",
			r_openWorldRadius ? r_openWorldRadius->value : 12288.0f );
	} else {
		WorldOpen_Shutdown();
		Com_Printf( "[world_open] disabled\n" );
	}
}

qboolean WorldOpen_IsEnabled( void ) {
	if ( !r_openWorld || !r_openWorld->integer ) {
		if ( openWorldEnabled ) {
			WorldOpen_Shutdown();
			openWorldEnabled = qfalse;
		}
		return qfalse;
	}
	if ( !openWorldEnabled ) {
		openWorldEnabled = qtrue;
	}
	return qtrue;
}

static qboolean WorldOpen_LoadLayer( int cellX, int cellY, worldOpenLayer_t layer ) {
	int idx = WorldOpen_AllocSlot( cellX, cellY );

	if ( idx < 0 ) {
		return qfalse;
	}

	switch ( layer ) {
		case WO_LAYER_COLLISION:
			if ( sectors[idx].collision ) {
				return qtrue;
			}
			if ( cm_openWorldCollision && cm_openWorldCollision->integer ) {
				if ( !CM_Stream_LoadSector( cellX, cellY ) ) {
					return qfalse;
				}
			}
			sectors[idx].collision = qtrue;
			break;
		case WO_LAYER_NAV:
			if ( sectors[idx].nav ) {
				return qtrue;
			}
			if ( sectorLoadFn && !sectorLoadFn( cellX, cellY, WO_LAYER_NAV ) ) {
				return qfalse;
			}
			sectors[idx].nav = qtrue;
			break;
		case WO_LAYER_SPRITES:
			if ( sectors[idx].sprites ) {
				return qtrue;
			}
			if ( sectorLoadFn && !sectorLoadFn( cellX, cellY, WO_LAYER_SPRITES ) ) {
				return qfalse;
			}
			sectors[idx].sprites = qtrue;
			break;
		default:
			return qfalse;
	}
	return qtrue;
}

static void WorldOpen_UnloadLayer( int cellX, int cellY, worldOpenLayer_t layer ) {
	int idx = WorldOpen_FindSlot( cellX, cellY );

	if ( idx < 0 ) {
		return;
	}

	switch ( layer ) {
		case WO_LAYER_COLLISION:
			if ( sectors[idx].collision ) {
				CM_Stream_UnloadSector( cellX, cellY );
				sectors[idx].collision = qfalse;
			}
			break;
		case WO_LAYER_NAV:
			if ( sectors[idx].nav ) {
				if ( sectorUnloadFn ) {
					sectorUnloadFn( cellX, cellY, WO_LAYER_NAV );
				}
				sectors[idx].nav = qfalse;
			}
			break;
		case WO_LAYER_SPRITES:
			if ( sectors[idx].sprites ) {
				if ( sectorUnloadFn ) {
					sectorUnloadFn( cellX, cellY, WO_LAYER_SPRITES );
				}
				sectors[idx].sprites = qfalse;
			}
			break;
		default:
			break;
	}

	if ( !sectors[idx].collision && !sectors[idx].nav && !sectors[idx].sprites ) {
		sectors[idx].active = qfalse;
	}
}

qboolean WorldOpen_LoadSector( int cellX, int cellY, worldOpenLayerMask_t layerMask ) {
	qboolean ok = qtrue;

	if ( !WorldOpen_IsEnabled() ) {
		return qfalse;
	}
	if ( layerMask & WO_LAYER_MASK_COLLISION ) {
		ok = WorldOpen_LoadLayer( cellX, cellY, WO_LAYER_COLLISION ) && ok;
	}
	if ( layerMask & WO_LAYER_MASK_NAV ) {
		ok = WorldOpen_LoadLayer( cellX, cellY, WO_LAYER_NAV ) && ok;
	}
	if ( layerMask & WO_LAYER_MASK_SPRITES ) {
		ok = WorldOpen_LoadLayer( cellX, cellY, WO_LAYER_SPRITES ) && ok;
	}
	if ( ok ) {
		Com_DPrintf( "[world_open] sector %d,%d layers loaded\n", cellX, cellY );
		if ( Cvar_VariableIntegerValue( "r_proc" ) ) {
			float ssize = r_openWorldSectorSize ? r_openWorldSectorSize->value : 4096.0f;
			worldProcSample_t ps = WorldProc_SampleSector( cellX, cellY, ssize );
			Com_DPrintf( "[world_proc] sector %d,%d -> region %d palette %d\n",
				cellX, cellY, ps.regionId, ps.paletteIndex );
		}
	}
	return ok;
}

void WorldOpen_UnloadSector( int cellX, int cellY ) {
	WorldOpen_UnloadSectorLayers( cellX, cellY, WO_LAYER_MASK_ALL );
}

void WorldOpen_UnloadSectorLayers( int cellX, int cellY, worldOpenLayerMask_t layerMask ) {
	if ( layerMask & WO_LAYER_MASK_SPRITES ) {
		WorldOpen_UnloadLayer( cellX, cellY, WO_LAYER_SPRITES );
	}
	if ( layerMask & WO_LAYER_MASK_NAV ) {
		WorldOpen_UnloadLayer( cellX, cellY, WO_LAYER_NAV );
	}
	if ( layerMask & WO_LAYER_MASK_COLLISION ) {
		WorldOpen_UnloadLayer( cellX, cellY, WO_LAYER_COLLISION );
	}
}

void WorldOpen_UpdateView( const vec3_t viewOrigin, float radius ) {
	int centerX, centerY;
	int minX, maxX, minY, maxY;
	int x, y;
	float sectorSize;
	int cellRadius;
	worldOpenLayerMask_t layerMask = 0;

	if ( !WorldOpen_IsEnabled() || !viewOrigin ) {
		return;
	}

	sectorSize = r_openWorldSectorSize ? r_openWorldSectorSize->value : 4096.0f;
	if ( sectorSize < 256.0f ) {
		sectorSize = 256.0f;
	}
	if ( radius <= 0.0f ) {
		radius = r_openWorldRadius ? r_openWorldRadius->value : 12288.0f;
	}

	if ( cm_openWorldCollision && cm_openWorldCollision->integer ) {
		layerMask |= WO_LAYER_MASK_COLLISION;
	}
	if ( r_openWorldNav && r_openWorldNav->integer ) {
		layerMask |= WO_LAYER_MASK_NAV;
	}
	if ( r_openWorldSprites && r_openWorldSprites->integer ) {
		layerMask |= WO_LAYER_MASK_SPRITES;
	}

	if ( r_openWorldStream && r_openWorldStream->integer ) {
		if ( !WorldResidency_IsEnabled() ) {
			CM_Stream_UpdateView( viewOrigin, radius, sectorSize,
				cm_openWorldCollision && cm_openWorldCollision->integer );
		}
	}

	if ( WorldResidency_IsEnabled() ) {
		WorldResidency_UpdateView( viewOrigin, radius, layerMask );
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
				(void)WorldOpen_LoadSector( x, y, layerMask );
			} else {
				WorldOpen_UnloadSector( x, y );
			}
		}
	}
}

int WorldOpen_GetSectorCount( void ) {
	return sectorCount;
}

const worldOpenSector_t *WorldOpen_GetSector( int index ) {
	if ( index < 0 || index >= sectorCount || !sectors[index].active ) {
		return NULL;
	}
	return &sectors[index];
}

int WorldOpen_FindSector( int cellX, int cellY ) {
	return WorldOpen_FindSlot( cellX, cellY );
}

void WorldOpen_List( void ) {
	int i;

	Com_Printf( "Open-world sectors (%d) enabled=%s sectorSize=%s radius=%.0f\n",
		sectorCount,
		WorldOpen_IsEnabled() ? "yes" : "no",
		r_openWorldSectorSize ? r_openWorldSectorSize->string : "?",
		r_openWorldRadius ? r_openWorldRadius->value : 0.0f );
	for ( i = 0; i < sectorCount; i++ ) {
		if ( !sectors[i].active ) {
			continue;
		}
		Com_Printf( "  [%d] %d,%d collision=%d nav=%d sprites=%d\n",
			i, sectors[i].cellX, sectors[i].cellY,
			sectors[i].collision, sectors[i].nav, sectors[i].sprites );
	}
}

void WorldOpen_Status( void ) {
	Com_Printf( "r_openWorld=%s stream=%s nav=%s sprites=%s cm_collision=%s cm_stream=%s\n",
		r_openWorld && r_openWorld->integer ? "1" : "0",
		r_openWorldStream && r_openWorldStream->integer ? "1" : "0",
		r_openWorldNav && r_openWorldNav->integer ? "1" : "0",
		r_openWorldSprites && r_openWorldSprites->integer ? "1" : "0",
		cm_openWorldCollision && cm_openWorldCollision->integer ? "1" : "0",
		Cvar_VariableIntegerValue( "cm_stream" ) ? "1" : "0" );
	WorldOpen_List();
}
