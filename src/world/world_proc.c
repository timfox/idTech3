/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include <math.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "world_proc.h"

static worldProcParams_t procParams;
static cvar_t *r_proc;
static cvar_t *r_procPattern;
static cvar_t *r_procSeed;
static cvar_t *r_procScale;
static cvar_t *r_procGridW;
static cvar_t *r_procGridH;
static cvar_t *r_procPalette;
static cvar_t *r_procScatterRegion;

static uint32_t WorldProc_Hash32( uint32_t x ) {
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

static uint32_t WorldProc_Rand( int a, int b, int seed ) {
	return WorldProc_Hash32( (uint32_t)( seed ^ ( a * 374761393 ) ^ ( b * 668265263 ) ) );
}

static float WorldProc_Rand01( int a, int b, int seed ) {
	return (float)( WorldProc_Rand( a, b, seed ) & 0xFFFF ) / 65535.0f;
}

static int WorldProc_WrapPalette( int v ) {
	int n = procParams.palette;
	if ( n < 2 ) {
		n = 2;
	}
	if ( n > 64 ) {
		n = 64;
	}
	v %= n;
	if ( v < 0 ) {
		v += n;
	}
	return v;
}

static float WorldProc_Scale( void ) {
	float s = procParams.scale;
	if ( s < 64.0f ) {
		s = 64.0f;
	}
	return s;
}

worldProcPattern_t WorldProc_ParsePattern( const char *name ) {
	if ( !name || !name[0] ) {
		return WPROC_GRID;
	}
	if ( !Q_stricmp( name, "grid" ) ) {
		return WPROC_GRID;
	}
	if ( !Q_stricmp( name, "checker" ) || !Q_stricmp( name, "check" ) ) {
		return WPROC_CHECKER;
	}
	if ( !Q_stricmp( name, "voronoi" ) || !Q_stricmp( name, "veroni" ) ) {
		return WPROC_VORONOI;
	}
	if ( !Q_stricmp( name, "hex" ) || !Q_stricmp( name, "hexagon" ) ) {
		return WPROC_HEX;
	}
	if ( !Q_stricmp( name, "radial" ) || !Q_stricmp( name, "rings" ) ) {
		return WPROC_RADIAL;
	}
	if ( !Q_stricmp( name, "stripe_h" ) || !Q_stricmp( name, "stripes_h" ) ) {
		return WPROC_STRIPE_H;
	}
	if ( !Q_stricmp( name, "stripe_v" ) || !Q_stricmp( name, "stripes_v" ) ) {
		return WPROC_STRIPE_V;
	}
	if ( !Q_stricmp( name, "noise" ) || !Q_stricmp( name, "hash" ) ) {
		return WPROC_NOISE;
	}
	return WPROC_COUNT;
}

const char *WorldProc_PatternName( worldProcPattern_t pattern ) {
	switch ( pattern ) {
		case WPROC_GRID:      return "grid";
		case WPROC_CHECKER:   return "checker";
		case WPROC_VORONOI:   return "voronoi";
		case WPROC_HEX:       return "hex";
		case WPROC_RADIAL:    return "radial";
		case WPROC_STRIPE_H:  return "stripe_h";
		case WPROC_STRIPE_V:  return "stripe_v";
		case WPROC_NOISE:     return "noise";
		default:              return "unknown";
	}
}

void WorldProc_Init( void ) {
	r_proc = Cvar_Get( "r_proc", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_proc,
		"Enable procedural world pattern sampling (Voronoi, grid, hex, etc.)." );
	r_procPattern = Cvar_Get( "r_procPattern", "voronoi", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procPattern,
		"Active pattern: grid, checker, voronoi, hex, radial, stripe_h, stripe_v, noise." );
	r_procSeed = Cvar_Get( "r_procSeed", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procSeed, "Deterministic seed for procedural patterns." );
	r_procScale = Cvar_Get( "r_procScale", "4096", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procScale, "World-unit feature scale (Voronoi site spacing / stripe width)." );
	r_procGridW = Cvar_Get( "r_procGridW", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procGridW, "Grid pattern width in sector cells." );
	r_procGridH = Cvar_Get( "r_procGridH", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procGridH, "Grid pattern height in sector cells." );
	r_procPalette = Cvar_Get( "r_procPalette", "8", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procPalette, "Palette size for pattern region indices (2-64)." );
	r_procScatterRegion = Cvar_Get( "r_procScatterRegion", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_procScatterRegion,
		"When 1, open-world scatter falls back to sprites/region_<id>.ents then sprites/palette_<n>.ents." );

	procParams.pattern = WPROC_VORONOI;
	procParams.seed = 1;
	procParams.scale = 4096.0f;
	procParams.gridW = 4;
	procParams.gridH = 4;
	procParams.palette = 8;

	Com_Printf( "[world_proc] procedural patterns initialized (r_proc 1)\n" );
}

void WorldProc_SetParams( const worldProcParams_t *params ) {
	if ( !params ) {
		return;
	}
	procParams = *params;
}

const worldProcParams_t *WorldProc_GetParams( void ) {
	return &procParams;
}

static void WorldProc_SyncFromCvars( void ) {
	worldProcParams_t p = procParams;

	if ( r_procPattern && r_procPattern->string[0] ) {
		worldProcPattern_t parsed = WorldProc_ParsePattern( r_procPattern->string );
		if ( parsed < WPROC_COUNT ) {
			p.pattern = parsed;
		}
	}
	if ( r_procSeed ) {
		p.seed = r_procSeed->integer;
	}
	if ( r_procScale ) {
		p.scale = r_procScale->value;
	}
	if ( r_procGridW ) {
		p.gridW = r_procGridW->integer;
		if ( p.gridW < 1 ) {
			p.gridW = 1;
		}
	}
	if ( r_procGridH ) {
		p.gridH = r_procGridH->integer;
		if ( p.gridH < 1 ) {
			p.gridH = 1;
		}
	}
	if ( r_procPalette ) {
		p.palette = r_procPalette->integer;
	}
	procParams = p;
}

static worldProcSample_t WorldProc_SampleGrid( int cellX, int cellY ) {
	worldProcSample_t s;
	int gw = procParams.gridW > 0 ? procParams.gridW : 1;
	int gh = procParams.gridH > 0 ? procParams.gridH : 1;

	Com_Memset( &s, 0, sizeof( s ) );
	s.regionId = ( cellX / gw ) * 1000 + ( cellY / gh );
	s.paletteIndex = WorldProc_WrapPalette( ( cellX / gw ) + ( cellY / gh ) );
	return s;
}

static worldProcSample_t WorldProc_SampleChecker( int cellX, int cellY ) {
	worldProcSample_t s;

	Com_Memset( &s, 0, sizeof( s ) );
	s.regionId = ( cellX + cellY ) & 1;
	s.paletteIndex = s.regionId;
	return s;
}

static worldProcSample_t WorldProc_SampleVoronoi( float worldX, float worldY ) {
	worldProcSample_t s;
	float scale = WorldProc_Scale();
	int bx = (int)floor( worldX / scale );
	int by = (int)floor( worldY / scale );
	int dx, dy;
	float bestDist = 1e20f;
	int bestBx = bx;
	int bestBy = by;

	Com_Memset( &s, 0, sizeof( s ) );

	for ( dy = -1; dy <= 1; dy++ ) {
		for ( dx = -1; dx <= 1; dx++ ) {
			int nbx = bx + dx;
			int nby = by + dy;
			float sx = ( (float)nbx + WorldProc_Rand01( nbx, nby, procParams.seed ) ) * scale;
			float sy = ( (float)nby + WorldProc_Rand01( nby, nbx, procParams.seed + 17 ) ) * scale;
			float dist = ( worldX - sx ) * ( worldX - sx ) + ( worldY - sy ) * ( worldY - sy );

			if ( dist < bestDist ) {
				bestDist = dist;
				bestBx = nbx;
				bestBy = nby;
			}
		}
	}

	s.regionId = bestBx * 10000 + bestBy;
	s.paletteIndex = WorldProc_WrapPalette( WorldProc_Rand( bestBx, bestBy, procParams.seed ) );
	s.voronoiDist = (float)sqrt( bestDist );
	return s;
}

static worldProcSample_t WorldProc_SampleHex( float worldX, float worldY ) {
	worldProcSample_t s;
	float scale = WorldProc_Scale();
	float qf;
	float rf;
	int qi;
	int ri;

	Com_Memset( &s, 0, sizeof( s ) );

	qf = ( 0.577350269f * worldX - 0.333333333f * worldY ) / scale;
	rf = ( 0.666666667f * worldY ) / scale;
	qi = (int)floor( qf + 0.5f );
	ri = (int)floor( rf + 0.5f );

	s.regionId = qi * 10000 + ri;
	s.paletteIndex = WorldProc_WrapPalette( qi - ri );
	return s;
}

static worldProcSample_t WorldProc_SampleRadial( float worldX, float worldY ) {
	worldProcSample_t s;
	float scale = WorldProc_Scale();
	float dist = (float)sqrt( worldX * worldX + worldY * worldY );
	int ring;

	Com_Memset( &s, 0, sizeof( s ) );
	ring = (int)floor( dist / scale );
	s.regionId = ring;
	s.paletteIndex = WorldProc_WrapPalette( ring );
	return s;
}

static worldProcSample_t WorldProc_SampleStripeH( float worldY ) {
	worldProcSample_t s;
	float scale = WorldProc_Scale();
	int band;

	Com_Memset( &s, 0, sizeof( s ) );
	band = (int)floor( worldY / scale );
	s.regionId = band;
	s.paletteIndex = WorldProc_WrapPalette( band );
	return s;
}

static worldProcSample_t WorldProc_SampleStripeV( float worldX ) {
	worldProcSample_t s;
	float scale = WorldProc_Scale();
	int band;

	Com_Memset( &s, 0, sizeof( s ) );
	band = (int)floor( worldX / scale );
	s.regionId = band;
	s.paletteIndex = WorldProc_WrapPalette( band );
	return s;
}

static worldProcSample_t WorldProc_SampleNoise( float worldX, float worldY ) {
	worldProcSample_t s;
	int ix = (int)floor( worldX / 128.0f );
	int iy = (int)floor( worldY / 128.0f );
	uint32_t h;

	Com_Memset( &s, 0, sizeof( s ) );
	h = WorldProc_Rand( ix, iy, procParams.seed );
	s.noiseValue = (float)( h & 0xFFFF ) / 65535.0f;
	s.regionId = (int)( h & 0x7fff );
	s.paletteIndex = WorldProc_WrapPalette( (int)( h % (uint32_t)procParams.palette ) );
	return s;
}

worldProcSample_t WorldProc_SampleWorld( float worldX, float worldY ) {
	int cellX;
	int cellY;
	float sectorSize;

	if ( !r_proc || !r_proc->integer ) {
		worldProcSample_t empty;
		Com_Memset( &empty, 0, sizeof( empty ) );
		return empty;
	}

	WorldProc_SyncFromCvars();
	sectorSize = WorldProc_Scale();
	cellX = (int)floor( worldX / sectorSize );
	cellY = (int)floor( worldY / sectorSize );

	switch ( procParams.pattern ) {
		case WPROC_GRID:
			return WorldProc_SampleGrid( cellX, cellY );
		case WPROC_CHECKER:
			return WorldProc_SampleChecker( cellX, cellY );
		case WPROC_VORONOI:
			return WorldProc_SampleVoronoi( worldX, worldY );
		case WPROC_HEX:
			return WorldProc_SampleHex( worldX, worldY );
		case WPROC_RADIAL:
			return WorldProc_SampleRadial( worldX, worldY );
		case WPROC_STRIPE_H:
			return WorldProc_SampleStripeH( worldY );
		case WPROC_STRIPE_V:
			return WorldProc_SampleStripeV( worldX );
		case WPROC_NOISE:
			return WorldProc_SampleNoise( worldX, worldY );
		default:
			return WorldProc_SampleGrid( cellX, cellY );
	}
}

worldProcSample_t WorldProc_SampleSector( int cellX, int cellY, float sectorSize ) {
	vec3_t center;

	if ( sectorSize < 256.0f ) {
		sectorSize = 256.0f;
	}
	center[0] = ( (float)cellX + 0.5f ) * sectorSize;
	center[1] = ( (float)cellY + 0.5f ) * sectorSize;
	return WorldProc_SampleWorld( center[0], center[1] );
}

int WorldProc_RegionAtSector( int cellX, int cellY, float sectorSize ) {
	return WorldProc_SampleSector( cellX, cellY, sectorSize ).regionId;
}

void WorldProc_ListPatterns( void ) {
	int i;

	Com_Printf( "Procedural patterns (r_procPattern):\n" );
	for ( i = 0; i < (int)WPROC_COUNT; i++ ) {
		Com_Printf( "  %s\n", WorldProc_PatternName( (worldProcPattern_t)i ) );
	}
	Com_Printf( "Active: %s seed=%d scale=%.0f grid=%dx%d palette=%d\n",
		WorldProc_PatternName( procParams.pattern ),
		procParams.seed, procParams.scale,
		procParams.gridW, procParams.gridH, procParams.palette );
}

void WorldProc_FormatScatterSectorPath( int cellX, int cellY, char *path, int pathSize ) {
	if ( !path || pathSize < 1 ) {
		return;
	}
	Com_sprintf( path, pathSize, "sprites/sector_%d_%d.ents", cellX, cellY );
}

void WorldProc_FormatScatterRegionPath( int cellX, int cellY, float sectorSize, char *path, int pathSize ) {
	worldProcSample_t s;

	if ( !path || pathSize < 1 ) {
		return;
	}
	s = WorldProc_SampleSector( cellX, cellY, sectorSize );
	Com_sprintf( path, pathSize, "sprites/region_%d.ents", s.regionId );
}

void WorldProc_FormatScatterPalettePath( int cellX, int cellY, float sectorSize, char *path, int pathSize ) {
	worldProcSample_t s;

	if ( !path || pathSize < 1 ) {
		return;
	}
	s = WorldProc_SampleSector( cellX, cellY, sectorSize );
	Com_sprintf( path, pathSize, "sprites/palette_%d.ents", s.paletteIndex );
}

void WorldProc_DumpSample( float worldX, float worldY ) {
	worldProcSample_t s = WorldProc_SampleWorld( worldX, worldY );

	Com_Printf( "proc sample (%.0f, %.0f) pattern=%s region=%d palette=%d",
		worldX, worldY,
		WorldProc_PatternName( procParams.pattern ),
		s.regionId, s.paletteIndex );
	if ( procParams.pattern == WPROC_VORONOI ) {
		Com_Printf( " voronoiDist=%.1f", s.voronoiDist );
	}
	if ( procParams.pattern == WPROC_NOISE ) {
		Com_Printf( " noise=%.3f", s.noiseValue );
	}
	Com_Printf( "\n" );
}
