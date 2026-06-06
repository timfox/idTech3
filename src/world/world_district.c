/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_stream.h"
#include "world_district.h"
#include "world_open.h"

static worldDistrict_t districts[WORLD_DISTRICT_MAX];
static int districtCount;
static char manifestPath[WORLD_DISTRICT_PATH_MAX];
static worldDistrictRegisterModel_f registerModelFn;

static cvar_t *r_district;
static cvar_t *r_districtProxy;
static cvar_t *cm_districtStream;
static cvar_t *r_districtSectorSize;
static cvar_t *r_districtLoadRadius;

static void WorldDistrict_DefaultPaths( worldDistrict_t *d );
static void WorldDistrict_ComputeSectors( worldDistrict_t *d );

void WorldDistrict_Import( int count, const worldDistrict_t *src, const char *path ) {
	int i;

	WorldDistrict_Clear();
	if ( !src || count <= 0 ) {
		return;
	}
	if ( count > WORLD_DISTRICT_MAX ) {
		count = WORLD_DISTRICT_MAX;
	}
	for ( i = 0; i < count; i++ ) {
		districts[i] = src[i];
		districts[i].active = qtrue;
		districts[i].state = WD_STATE_UNLOADED;
		districts[i].proxyModel = 0;
		districts[i].fullModel = 0;
		WorldDistrict_DefaultPaths( &districts[i] );
		WorldDistrict_ComputeSectors( &districts[i] );
	}
	districtCount = count;
	if ( path && path[0] ) {
		Q_strncpyz( manifestPath, path, sizeof( manifestPath ) );
	}
	Com_Printf( "[world_district] imported %d district(s) from '%s'\n", districtCount,
		path ? path : "(memory)" );
}

void WorldDistrict_Init( void ) {
	r_district = Cvar_Get( "r_district", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_district,
		"Enable USD world districts (proxy meshes + optional sector streaming)." );
	r_districtProxy = Cvar_Get( "r_districtProxy", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_districtProxy,
		"When 1, load purpose=proxy USDA meshes before full district payloads." );
	cm_districtStream = Cvar_Get( "cm_districtStream", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cm_districtStream,
		"When 1 and cm_stream 1, load BSP sectors covered by a district on full load." );
	r_districtSectorSize = Cvar_Get( "r_districtSectorSize", "4096", CVAR_ARCHIVE );
	Cvar_SetDescription( r_districtSectorSize,
		"World units per cm_stream sector cell when deriving grid from district bounds." );
	r_districtLoadRadius = Cvar_Get( "r_districtLoadRadius", "8192", CVAR_ARCHIVE );
	Cvar_SetDescription( r_districtLoadRadius,
		"View-driven residency radius for proxy/full district loads." );

	districtCount = 0;
	manifestPath[0] = '\0';
	registerModelFn = NULL;
	Com_Memset( districts, 0, sizeof( districts ) );
	Com_Printf( "[world_district] districts + proxy mesh layer initialized\n" );
}

void WorldDistrict_Shutdown( void ) {
	WorldDistrict_Clear();
}

void WorldDistrict_SetRegisterModel( worldDistrictRegisterModel_f fn ) {
	registerModelFn = fn;
}

void WorldDistrict_Clear( void ) {
	int i;

	for ( i = 0; i < districtCount; i++ ) {
		districts[i].active = qfalse;
		districts[i].state = WD_STATE_UNLOADED;
		districts[i].proxyModel = 0;
		districts[i].fullModel = 0;
	}
	districtCount = 0;
	manifestPath[0] = '\0';
}

static void WorldDistrict_ComputeSectors( worldDistrict_t *d ) {
	float sectorSize;
	int x0, y0, x1, y1;

	if ( !d || !r_districtSectorSize ) {
		return;
	}

	sectorSize = r_districtSectorSize->value;
	if ( sectorSize < 256.0f ) {
		sectorSize = 256.0f;
	}

	x0 = (int)floor( d->boundsMin[0] / sectorSize );
	y0 = (int)floor( d->boundsMin[1] / sectorSize );
	x1 = (int)floor( d->boundsMax[0] / sectorSize );
	y1 = (int)floor( d->boundsMax[1] / sectorSize );
	if ( x0 < 0 ) {
		x0 = 0;
	}
	if ( y0 < 0 ) {
		y0 = 0;
	}
	if ( x1 < x0 ) {
		x1 = x0;
	}
	if ( y1 < y0 ) {
		y1 = y0;
	}
	d->sectorX0 = x0;
	d->sectorY0 = y0;
	d->sectorX1 = x1;
	d->sectorY1 = y1;
}

static void WorldDistrict_DefaultPaths( worldDistrict_t *d ) {
	char slug[WORLD_DISTRICT_NAME_MAX];
	const char *base;
	int i, j;

	if ( !d || !d->name[0] ) {
		return;
	}

	base = d->name;
	if ( !Q_strncmp( base, "District_", 9 ) ) {
		base += 9;
	}

	j = 0;
	for ( i = 0; base[i] && j < (int)sizeof( slug ) - 1; i++ ) {
		char c = base[i];
		if ( c >= 'A' && c <= 'Z' ) {
			c = (char)( c - 'A' + 'a' );
		}
		slug[j++] = c;
	}
	slug[j] = '\0';

	if ( !d->proxyMeshPath[0] ) {
		Com_sprintf( d->proxyMeshPath, sizeof( d->proxyMeshPath ),
			"world/proxies/%s_proxy.usda", slug );
	}
	if ( !d->fullMeshPath[0] ) {
		Com_sprintf( d->fullMeshPath, sizeof( d->fullMeshPath ),
			"world/districts/%s.usda", slug );
	}
}

static qboolean WorldDistrict_PointInBounds( const worldDistrict_t *d, const vec3_t p ) {
	if ( !d ) {
		return qfalse;
	}
	return ( p[0] >= d->boundsMin[0] && p[0] <= d->boundsMax[0] &&
		p[1] >= d->boundsMin[1] && p[1] <= d->boundsMax[1] &&
		p[2] >= d->boundsMin[2] && p[2] <= d->boundsMax[2] ) ? qtrue : qfalse;
}

qboolean WorldDistrict_LoadManifest( const char *usdaPath ) {
	(void)usdaPath;
	Com_Printf( S_COLOR_YELLOW "[world_district] use client command district_load <path.usda> (FreeUSD)\n" );
	return qfalse;
}

int WorldDistrict_GetCount( void ) {
	return districtCount;
}

const worldDistrict_t *WorldDistrict_Get( int index ) {
	if ( index < 0 || index >= districtCount || !districts[index].active ) {
		return NULL;
	}
	return &districts[index];
}

int WorldDistrict_FindByName( const char *name ) {
	int i;

	if ( !name || !name[0] ) {
		return -1;
	}
	for ( i = 0; i < districtCount; i++ ) {
		if ( districts[i].active && !Q_stricmp( districts[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}

int WorldDistrict_FindAtPoint( const vec3_t point ) {
	int i, best = -1;
	float bestDist = 1e9f;

	for ( i = 0; i < districtCount; i++ ) {
		vec3_t center;
		float dist;

		if ( !districts[i].active ) {
			continue;
		}
		if ( !WorldDistrict_PointInBounds( &districts[i], point ) ) {
			continue;
		}
		VectorAdd( districts[i].boundsMin, districts[i].boundsMax, center );
		VectorScale( center, 0.5f, center );
		dist = Distance( point, center );
		if ( dist < bestDist ) {
			bestDist = dist;
			best = i;
		}
	}
	return best;
}

static void WorldDistrict_StreamSectors( const worldDistrict_t *d ) {
	int x, y;
	worldOpenLayerMask_t layerMask = 0;

	if ( !d || !cm_districtStream || !cm_districtStream->integer ) {
		return;
	}

	if ( WorldOpen_IsEnabled() ) {
		if ( Cvar_VariableIntegerValue( "cm_openWorldCollision" ) ) {
			layerMask |= WO_LAYER_MASK_COLLISION;
		}
		if ( Cvar_VariableIntegerValue( "r_openWorldNav" ) ) {
			layerMask |= WO_LAYER_MASK_NAV;
		}
		if ( Cvar_VariableIntegerValue( "r_openWorldSprites" ) ) {
			layerMask |= WO_LAYER_MASK_SPRITES;
		}
		for ( y = d->sectorY0; y <= d->sectorY1; y++ ) {
			for ( x = d->sectorX0; x <= d->sectorX1; x++ ) {
				(void)WorldOpen_LoadSector( x, y, layerMask );
			}
		}
		Com_DPrintf( "[world_district] %s streamed via WorldOpen (%d,%d..%d,%d)\n",
			d->name, d->sectorX0, d->sectorY0, d->sectorX1, d->sectorY1 );
		return;
	}

	for ( y = d->sectorY0; y <= d->sectorY1; y++ ) {
		for ( x = d->sectorX0; x <= d->sectorX1; x++ ) {
			(void)CM_Stream_LoadSector( x, y );
		}
	}
}

qboolean WorldDistrict_LoadProxy( int index ) {
	worldDistrict_t *d;

	if ( index < 0 || index >= districtCount || !districts[index].active ) {
		return qfalse;
	}
	d = &districts[index];

	if ( !r_districtProxy || !r_districtProxy->integer ) {
		Com_Printf( "[world_district] proxy disabled (r_districtProxy 0)\n" );
		return qfalse;
	}
	if ( !registerModelFn ) {
		Com_Printf( S_COLOR_YELLOW "[world_district] no RegisterModel hook (client only)\n" );
		return qfalse;
	}
	if ( !d->proxyMeshPath[0] ) {
		Com_Printf( S_COLOR_YELLOW "[world_district] %s has no proxy mesh path\n", d->name );
		return qfalse;
	}

	d->proxyModel = registerModelFn( d->proxyMeshPath );
	if ( !d->proxyModel ) {
		Com_Printf( S_COLOR_YELLOW "[world_district] proxy register failed: %s\n", d->proxyMeshPath );
		return qfalse;
	}

	d->state = WD_STATE_PROXY;
	Com_Printf( "[world_district] %s proxy loaded (%s handle %d)\n",
		d->name, d->proxyMeshPath, d->proxyModel );
	return qtrue;
}

qboolean WorldDistrict_LoadFull( int index ) {
	worldDistrict_t *d;

	if ( index < 0 || index >= districtCount || !districts[index].active ) {
		return qfalse;
	}
	d = &districts[index];

	d->state = WD_STATE_STREAMING;

	if ( cm_districtStream && cm_districtStream->integer ) {
		WorldDistrict_StreamSectors( d );
	}

	if ( registerModelFn && d->fullMeshPath[0] ) {
		d->fullModel = registerModelFn( d->fullMeshPath );
	}

	d->state = WD_STATE_LOADED;
	Com_Printf( "[world_district] %s full load (sectors %d,%d..%d,%d model %d)\n",
		d->name, d->sectorX0, d->sectorY0, d->sectorX1, d->sectorY1, d->fullModel );
	return qtrue;
}

void WorldDistrict_Unload( int index ) {
	if ( index < 0 || index >= districtCount || !districts[index].active ) {
		return;
	}
	districts[index].state = WD_STATE_UNLOADED;
	districts[index].proxyModel = 0;
	districts[index].fullModel = 0;
	Com_Printf( "[world_district] unloaded %s\n", districts[index].name );
}

void WorldDistrict_UpdateView( const vec3_t viewOrigin, float loadRadius ) {
	int i;

	if ( !r_district || !r_district->integer || districtCount <= 0 ) {
		return;
	}

	for ( i = 0; i < districtCount; i++ ) {
		vec3_t center;
		float dist;

		if ( !districts[i].active ) {
			continue;
		}

		VectorAdd( districts[i].boundsMin, districts[i].boundsMax, center );
		VectorScale( center, 0.5f, center );
		dist = Distance( viewOrigin, center );

		if ( dist > loadRadius ) {
			if ( districts[i].state != WD_STATE_UNLOADED ) {
				WorldDistrict_Unload( i );
			}
			continue;
		}

		if ( districts[i].state == WD_STATE_UNLOADED ) {
			if ( !WorldDistrict_LoadProxy( i ) ) {
				(void)WorldDistrict_LoadFull( i );
			}
		} else if ( districts[i].state == WD_STATE_PROXY && dist < loadRadius * 0.5f ) {
			(void)WorldDistrict_LoadFull( i );
		}
	}
}

void WorldDistrict_List( void ) {
	int i;
	const char *stateLabel;

	Com_Printf( "World districts (%d) manifest: %s\n", districtCount,
		manifestPath[0] ? manifestPath : "(none)" );
	for ( i = 0; i < districtCount; i++ ) {
		switch ( districts[i].state ) {
			case WD_STATE_PROXY:     stateLabel = "proxy"; break;
			case WD_STATE_STREAMING: stateLabel = "streaming"; break;
			case WD_STATE_LOADED:    stateLabel = "loaded"; break;
			default:                 stateLabel = "unloaded"; break;
		}
		Com_Printf( "  [%d] %s state=%s proxy=%s full=%s sectors=%d,%d..%d,%d\n",
			i, districts[i].name, stateLabel,
			districts[i].proxyMeshPath, districts[i].fullMeshPath,
			districts[i].sectorX0, districts[i].sectorY0,
			districts[i].sectorX1, districts[i].sectorY1 );
	}
}

void WorldDistrict_Status( int index ) {
	const worldDistrict_t *d = WorldDistrict_Get( index );

	if ( !d ) {
		Com_Printf( "Invalid district index %d\n", index );
		return;
	}
	Com_Printf( "District %s\n", d->name );
	Com_Printf( "  state:     %d\n", (int)d->state );
	Com_Printf( "  bounds:    (%.0f,%.0f,%.0f)..(%.0f,%.0f,%.0f)\n",
		d->boundsMin[0], d->boundsMin[1], d->boundsMin[2],
		d->boundsMax[0], d->boundsMax[1], d->boundsMax[2] );
	Com_Printf( "  proxy:     %s (handle %d)\n", d->proxyMeshPath, d->proxyModel );
	Com_Printf( "  full:      %s (handle %d)\n", d->fullMeshPath, d->fullModel );
	Com_Printf( "  sectors:   %d,%d .. %d,%d\n",
		d->sectorX0, d->sectorY0, d->sectorX1, d->sectorY1 );
}
