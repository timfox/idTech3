/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Procedural world patterns — Voronoi, grid, hex, radial, stripes, hash noise.
Deterministic from seed + scale; used for open-world sector typing and scatter.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

typedef enum {
	WPROC_GRID = 0,
	WPROC_CHECKER,
	WPROC_VORONOI,
	WPROC_HEX,
	WPROC_RADIAL,
	WPROC_STRIPE_H,
	WPROC_STRIPE_V,
	WPROC_NOISE,
	WPROC_COUNT
} worldProcPattern_t;

typedef struct worldProcParams_s {
	worldProcPattern_t pattern;
	int                seed;
	float              scale;
	int                gridW;
	int                gridH;
	int                palette;
} worldProcParams_t;

typedef struct worldProcSample_s {
	int   regionId;
	int   paletteIndex;
	float voronoiDist;
	float noiseValue;
} worldProcSample_t;

void              WorldProc_Init( void );
void              WorldProc_SetParams( const worldProcParams_t *params );
const worldProcParams_t *WorldProc_GetParams( void );

worldProcPattern_t WorldProc_ParsePattern( const char *name );
const char       *WorldProc_PatternName( worldProcPattern_t pattern );

worldProcSample_t WorldProc_SampleWorld( float worldX, float worldY );
worldProcSample_t WorldProc_SampleSector( int cellX, int cellY, float sectorSize );
int               WorldProc_RegionAtSector( int cellX, int cellY, float sectorSize );

void              WorldProc_ListPatterns( void );
void              WorldProc_DumpSample( float worldX, float worldY );

/* Scatter asset path helpers (sprites/sector_X_Y.ents → region/palette fallbacks). */
void              WorldProc_FormatScatterSectorPath( int cellX, int cellY, char *path, int pathSize );
void              WorldProc_FormatScatterRegionPath( int cellX, int cellY, float sectorSize, char *path, int pathSize );
void              WorldProc_FormatScatterPalettePath( int cellX, int cellY, float sectorSize, char *path, int pathSize );

#ifdef __cplusplus
}
#endif
