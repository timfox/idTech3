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

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int cellX;
	int cellY;
	int regionId;
	float weight;
} wrSetEntry_t;

typedef struct {
	wrSetEntry_t current[WORLD_RESIDENCY_MAX_SET];
	int          currentCount;
	wrSetEntry_t target[WORLD_RESIDENCY_MAX_SET];
	int          targetCount;
	wrSetEntry_t pendingAdd[WORLD_RESIDENCY_MAX_SET];
	wrSetEntry_t pendingRemove[WORLD_RESIDENCY_MAX_SET];
	int          pendingAddCount;
	int          pendingRemoveCount;
	qboolean     inTransition;
	int          transitionStep;
	int          transitionTotal;
	int          tick;
} wrLayerState_t;

static cvar_t *r_openWorldResidency;
static cvar_t *sv_openWorldResidency;
static cvar_t *r_openWorldResidencyEpsilon;
static cvar_t *r_openWorldMaxSectors;
static cvar_t *r_openWorldMaxNavSectors;
static cvar_t *r_openWorldMaxSpriteSectors;
static cvar_t *r_openWorldResidencyMaxSwaps;
static cvar_t *r_openWorldLoadRadius;
static cvar_t *r_openWorldUnloadRadius;
static cvar_t *r_openWorldResidencyMatroid;
static cvar_t *r_openWorldResidencyW_dist;
static cvar_t *r_openWorldResidencyW_proc;
static cvar_t *r_openWorldResidencyW_sticky;
static cvar_t *cl_openWorldResidencyNavLocal;

static wrLayerState_t s_layers[WR_LAYER_COUNT];
static qboolean s_districtFilterActive;
static int s_districtX0, s_districtY0, s_districtX1, s_districtY1;
static worldResidencyCell_t s_serverAllow[WORLD_RESIDENCY_MAX_SET];
static int s_serverAllowCount;
static int s_scheduleShift;
static qboolean s_loggedEnable;

static int WR_BudgetForLayer( worldResidencyLayer_t layer ) {
	int k;

	switch ( layer ) {
	case WR_LAYER_COLLISION:
		k = r_openWorldMaxSectors ? r_openWorldMaxSectors->integer : 64;
		if ( k > WORLD_RESIDENCY_CM_MERGE_MAX ) {
			k = WORLD_RESIDENCY_CM_MERGE_MAX;
		}
		if ( k < 1 ) {
			k = 1;
		}
		return k;
	case WR_LAYER_NAV:
		k = r_openWorldMaxNavSectors ? r_openWorldMaxNavSectors->integer : 32;
		return k < 1 ? 1 : k;
	case WR_LAYER_SPRITES:
		k = r_openWorldMaxSpriteSectors ? r_openWorldMaxSpriteSectors->integer : 64;
		return k < 1 ? 1 : k;
	default:
		return 1;
	}
}

static float WR_Epsilon( void ) {
	float e = r_openWorldResidencyEpsilon ? r_openWorldResidencyEpsilon->value : 0.05f;
	if ( e < 0.001f ) {
		e = 0.001f;
	}
	if ( e > 0.49f ) {
		e = 0.49f;
	}
	return e;
}

static int WR_MaxSwaps( void ) {
	int m = r_openWorldResidencyMaxSwaps ? r_openWorldResidencyMaxSwaps->integer : 4;
	return m < 1 ? 1 : m;
}

static qboolean WR_CellEqual( int ax, int ay, int bx, int by ) {
	return ax == bx && ay == by;
}

static qboolean WR_InCurrent( const wrLayerState_t *st, int cellX, int cellY ) {
	int i;

	for ( i = 0; i < st->currentCount; i++ ) {
		if ( WR_CellEqual( st->current[i].cellX, st->current[i].cellY, cellX, cellY ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean WR_InSet( const worldResidencyCell_t *set, int count, int cellX, int cellY ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		if ( WR_CellEqual( set[i].cellX, set[i].cellY, cellX, cellY ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean WR_ServerAllows( int cellX, int cellY ) {
	int i;

	if ( s_serverAllowCount <= 0 ) {
		return qtrue;
	}
	for ( i = 0; i < s_serverAllowCount; i++ ) {
		if ( WR_CellEqual( s_serverAllow[i].cellX, s_serverAllow[i].cellY, cellX, cellY ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean WR_PassDistrictFilter( int cellX, int cellY ) {
	if ( !s_districtFilterActive ) {
		return qtrue;
	}
	return cellX >= s_districtX0 && cellX <= s_districtX1 &&
		cellY >= s_districtY0 && cellY <= s_districtY1;
}

static int WR_CompareCandidateDesc( const void *a, const void *b ) {
	const worldResidencyCandidate_t *ca = (const worldResidencyCandidate_t *)a;
	const worldResidencyCandidate_t *cb = (const worldResidencyCandidate_t *)b;
	if ( ca->score > cb->score ) {
		return -1;
	}
	if ( ca->score < cb->score ) {
		return 1;
	}
	if ( ca->cellX != cb->cellX ) {
		return ca->cellX - cb->cellX;
	}
	return ca->cellY - cb->cellY;
}

static float WR_RandomUnit( void ) {
	return (float)( rand() & 0x7fff ) / (float)0x8000;
}

void WorldResidency_Init( void ) {
	r_openWorldResidency = Cvar_Get( "r_openWorldResidency", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldResidency,
		"Consistent value-aware sector residency (submodular-style selector). 0 = legacy radius disk." );
	sv_openWorldResidency = Cvar_Get( "sv_openWorldResidency", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_openWorldResidency,
		"Server-side consistent collision residency for MP (union over player origins)." );
	r_openWorldResidencyEpsilon = Cvar_Get( "r_openWorldResidencyEpsilon", "0.05", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldResidencyEpsilon,
		"Residency precision epsilon (transition fraction + robustness)." );
	r_openWorldMaxSectors = Cvar_Get( "r_openWorldMaxSectors", "64", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldMaxSectors,
		"Max resident collision sectors (clamped to CM merge limit)." );
	r_openWorldMaxNavSectors = Cvar_Get( "r_openWorldMaxNavSectors", "32", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldMaxNavSectors, "Max resident nav sector tiles." );
	r_openWorldMaxSpriteSectors = Cvar_Get( "r_openWorldMaxSpriteSectors", "64", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldMaxSpriteSectors, "Max resident scatter sectors." );
	r_openWorldResidencyMaxSwaps = Cvar_Get( "r_openWorldResidencyMaxSwaps", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldResidencyMaxSwaps,
		"Max sector load/unload ops per frame outside transition windows." );
	r_openWorldLoadRadius = Cvar_Get( "r_openWorldLoadRadius", "12288", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldLoadRadius, "Candidate load radius for residency hysteresis." );
	r_openWorldUnloadRadius = Cvar_Get( "r_openWorldUnloadRadius", "14336", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldUnloadRadius, "Candidate unload radius (>= load radius)." );
	r_openWorldResidencyMatroid = Cvar_Get( "r_openWorldResidencyMatroid", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_openWorldResidencyMatroid,
		"Partition-matroid ROBUST-SWAP using r_proc regionId (one sector per region)." );
	r_openWorldResidencyW_dist = Cvar_Get( "r_openWorldResidencyW_dist", "1.0", CVAR_ARCHIVE );
	r_openWorldResidencyW_proc = Cvar_Get( "r_openWorldResidencyW_proc", "0.25", CVAR_ARCHIVE );
	r_openWorldResidencyW_sticky = Cvar_Get( "r_openWorldResidencyW_sticky", "0.35", CVAR_ARCHIVE );
	cl_openWorldResidencyNavLocal = Cvar_Get( "cl_openWorldResidencyNavLocal", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_openWorldResidencyNavLocal,
		"Allow client-only nav sectors beyond server collision list (default off)." );

	Com_Memset( s_layers, 0, sizeof( s_layers ) );
	s_districtFilterActive = qfalse;
	s_serverAllowCount = 0;
	s_scheduleShift = com_frameTime & 4095;
	s_loggedEnable = qfalse;
}

void WorldResidency_Shutdown( void ) {
	Com_Memset( s_layers, 0, sizeof( s_layers ) );
	s_serverAllowCount = 0;
	s_districtFilterActive = qfalse;
	s_loggedEnable = qfalse;
}

qboolean WorldResidency_IsEnabled( void ) {
	return r_openWorldResidency && r_openWorldResidency->integer;
}

qboolean WorldResidency_ServerEnabled( void ) {
	return sv_openWorldResidency && sv_openWorldResidency->integer;
}

void WorldResidency_SetDistrictFilter( qboolean active, int x0, int y0, int x1, int y1 ) {
	s_districtFilterActive = active;
	s_districtX0 = x0;
	s_districtY0 = y0;
	s_districtX1 = x1;
	s_districtY1 = y1;
}

void WorldResidency_ClearDistrictFilter( void ) {
	s_districtFilterActive = qfalse;
}

void WorldResidency_SetServerCollisionAllowList( const worldResidencyCell_t *cells, int count ) {
	int i;

	s_serverAllowCount = 0;
	if ( !cells || count <= 0 ) {
		return;
	}
	if ( count > WORLD_RESIDENCY_MAX_SET ) {
		count = WORLD_RESIDENCY_MAX_SET;
	}
	for ( i = 0; i < count; i++ ) {
		s_serverAllow[i] = cells[i];
	}
	s_serverAllowCount = count;
}

void WorldResidency_ClearServerCollisionAllowList( void ) {
	s_serverAllowCount = 0;
}

float WorldResidency_ScoreCell( worldResidencyLayer_t layer, int cellX, int cellY,
	const vec3_t viewOrigin, float loadRadius, float sectorSize, qboolean sticky ) {
	vec3_t center;
	float dist;
	float f_dist;
	float w_dist;
	float w_proc;
	float w_sticky;
	float procBonus = 0.0f;
	worldProcSample_t ps;

	(void)layer;

	if ( !viewOrigin || loadRadius <= 0.0f || sectorSize < 256.0f ) {
		return 0.0f;
	}

	center[0] = ( (float)cellX + 0.5f ) * sectorSize;
	center[1] = ( (float)cellY + 0.5f ) * sectorSize;
	center[2] = viewOrigin[2];
	dist = Distance( viewOrigin, center );
	f_dist = 1.0f - dist / loadRadius;
	if ( f_dist < 0.0f ) {
		f_dist = 0.0f;
	}

	w_dist = r_openWorldResidencyW_dist ? r_openWorldResidencyW_dist->value : 1.0f;
	w_proc = r_openWorldResidencyW_proc ? r_openWorldResidencyW_proc->value : 0.25f;
	w_sticky = r_openWorldResidencyW_sticky ? r_openWorldResidencyW_sticky->value : 0.35f;

	if ( Cvar_VariableIntegerValue( "r_proc" ) ) {
		ps = WorldProc_SampleSector( cellX, cellY, sectorSize );
		procBonus = 0.25f + 0.01f * (float)( ps.paletteIndex & 15 );
		(void)ps.regionId;
	}

	return w_dist * f_dist + w_proc * procBonus + ( sticky ? w_sticky : 0.0f );
}

static int WR_FindRegionEntry( const wrSetEntry_t *set, int count, int regionId ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		if ( set[i].regionId == regionId ) {
			return i;
		}
	}
	return -1;
}

static void WR_SyncCurrentFromWorld( worldResidencyLayer_t layer, float sectorSize ) {
	wrLayerState_t *st = &s_layers[layer];
	int i, n;
	const worldOpenSector_t *sec;
	worldOpenLayer_t woLayer;

	st->currentCount = 0;
	n = WorldOpen_GetSectorCount();
	switch ( layer ) {
	case WR_LAYER_COLLISION:
		woLayer = WO_LAYER_COLLISION;
		break;
	case WR_LAYER_NAV:
		woLayer = WO_LAYER_NAV;
		break;
	case WR_LAYER_SPRITES:
		woLayer = WO_LAYER_SPRITES;
		break;
	default:
		return;
	}

	for ( i = 0; i < n && st->currentCount < WORLD_RESIDENCY_MAX_SET; i++ ) {
		sec = WorldOpen_GetSector( i );
		if ( !sec || !sec->active ) {
			continue;
		}
		switch ( woLayer ) {
		case WO_LAYER_COLLISION:
			if ( !sec->collision ) {
				continue;
			}
			break;
		case WO_LAYER_NAV:
			if ( !sec->nav ) {
				continue;
			}
			break;
		case WO_LAYER_SPRITES:
			if ( !sec->sprites ) {
				continue;
			}
			break;
		default:
			continue;
		}
		st->current[st->currentCount].cellX = sec->cellX;
		st->current[st->currentCount].cellY = sec->cellY;
		st->current[st->currentCount].regionId =
			WorldProc_RegionAtSector( sec->cellX, sec->cellY, sectorSize );
		st->current[st->currentCount].weight = 1.0f;
		st->currentCount++;
	}
}

static void WR_SyncCurrentCollisionFromStream( float sectorSize ) {
	wrLayerState_t *st = &s_layers[WR_LAYER_COLLISION];
	char list[256];
	char buf[256];
	char *p;
	char *tok;
	int cellX, cellY;

	st->currentCount = 0;
	CM_Stream_BuildLoadedList( list, sizeof( list ) );
	Q_strncpyz( buf, list, sizeof( buf ) );
	for ( p = buf; *p; p++ ) {
		if ( *p == ',' ) {
			*p = ' ';
		}
	}
	p = buf;
	while ( *p && st->currentCount < WORLD_RESIDENCY_MAX_SET ) {
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
			st->current[st->currentCount].cellX = cellX;
			st->current[st->currentCount].cellY = cellY;
			st->current[st->currentCount].regionId =
				WorldProc_RegionAtSector( cellX, cellY, sectorSize );
			st->current[st->currentCount].weight = 1.0f;
			st->currentCount++;
		}
	}
}

int WorldResidency_SelectCardinality( worldResidencyLayer_t layer,
	const worldResidencyCandidate_t *candidates, int candidateCount,
	const worldResidencyCell_t *current, int currentCount,
	worldResidencyCell_t *out, int outMax, qboolean useMatroid ) {
	float eps;
	int k;
	int d;
	int robustCap;
	int i;
	int outCount = 0;
	worldResidencyCandidate_t work[WORLD_RESIDENCY_MAX_CANDIDATES];
	wrSetEntry_t selected[WORLD_RESIDENCY_MAX_SET];
	int selCount = 0;
	int workCount;

	if ( !candidates || candidateCount <= 0 || !out || outMax <= 0 ) {
		return 0;
	}

	k = WR_BudgetForLayer( layer );
	if ( outMax > k ) {
		outMax = k;
	}
	eps = WR_Epsilon();
	d = (int)( eps * eps * (float)k );
	if ( d < 1 ) {
		d = 1;
	}
	robustCap = k - (int)( ( 2.0f * (float)d ) / ( eps * (float)k + 0.5f ) );
	if ( robustCap < 1 ) {
		robustCap = 1;
	}

	workCount = candidateCount;
	if ( workCount > WORLD_RESIDENCY_MAX_CANDIDATES ) {
		workCount = WORLD_RESIDENCY_MAX_CANDIDATES;
	}
	Com_Memcpy( work, candidates, (size_t)workCount * sizeof( work[0] ) );
	qsort( work, (size_t)workCount, sizeof( work[0] ), WR_CompareCandidateDesc );

	if ( useMatroid ) {
		int cIdx;
		int topPool;
		float invSum;
		int pick;
		float r;

		topPool = (int)( (float)d / ( eps + 0.001f ) );
		if ( topPool < 1 ) {
			topPool = 1;
		}
		if ( topPool > workCount ) {
			topPool = workCount;
		}

		while ( workCount > topPool && selCount < robustCap ) {
			invSum = 0.0f;
			for ( cIdx = 0; cIdx < topPool; cIdx++ ) {
				invSum += 1.0f / ( work[cIdx].score + 0.001f );
			}
			r = WR_RandomUnit() * invSum;
			pick = 0;
			for ( cIdx = 0; cIdx < topPool; cIdx++ ) {
				r -= 1.0f / ( work[cIdx].score + 0.001f );
				if ( r <= 0.0f ) {
					pick = cIdx;
					break;
				}
			}
			{
				worldResidencyCandidate_t chosen = work[pick];
				int regIdx = WR_FindRegionEntry( selected, selCount, chosen.regionId );

				if ( regIdx < 0 && selCount < WORLD_RESIDENCY_MAX_SET ) {
					selected[selCount].cellX = chosen.cellX;
					selected[selCount].cellY = chosen.cellY;
					selected[selCount].regionId = chosen.regionId;
					selected[selCount].weight = chosen.score;
					selCount++;
				} else if ( regIdx >= 0 && chosen.score >= 2.0f * selected[regIdx].weight ) {
					selected[regIdx].cellX = chosen.cellX;
					selected[regIdx].cellY = chosen.cellY;
					selected[regIdx].weight = chosen.score;
				}
			}
			work[pick] = work[topPool - 1];
			topPool--;
			workCount--;
		}
	} else {
		int cIdx;
		int topPool;
		float invSum;
		int pick;
		float r;

		topPool = (int)( (float)d / ( eps + 0.001f ) );
		if ( topPool < 1 ) {
			topPool = 1;
		}
		if ( topPool > workCount ) {
			topPool = workCount;
		}

		while ( workCount > topPool && selCount < robustCap ) {
			invSum = 0.0f;
			for ( cIdx = 0; cIdx < topPool; cIdx++ ) {
				invSum += 1.0f / ( work[cIdx].score + 0.001f );
			}
			r = WR_RandomUnit() * invSum;
			pick = 0;
			for ( cIdx = 0; cIdx < topPool; cIdx++ ) {
				r -= 1.0f / ( work[cIdx].score + 0.001f );
				if ( r <= 0.0f ) {
					pick = cIdx;
					break;
				}
			}
			if ( selCount < WORLD_RESIDENCY_MAX_SET ) {
				selected[selCount].cellX = work[pick].cellX;
				selected[selCount].cellY = work[pick].cellY;
				selected[selCount].regionId = work[pick].regionId;
				selected[selCount].weight = work[pick].score;
				selCount++;
			}
			work[pick] = work[topPool - 1];
			topPool--;
			workCount--;
		}
	}

	for ( i = 0; i < workCount && selCount < outMax; i++ ) {
		int cIdx;
		qboolean dup = qfalse;

		for ( cIdx = 0; cIdx < selCount; cIdx++ ) {
			if ( WR_CellEqual( selected[cIdx].cellX, selected[cIdx].cellY,
				work[i].cellX, work[i].cellY ) ) {
				dup = qtrue;
				break;
			}
		}
		if ( dup ) {
			continue;
		}
		if ( useMatroid ) {
			int regIdx = WR_FindRegionEntry( selected, selCount, work[i].regionId );
			if ( regIdx >= 0 ) {
				if ( work[i].score <= selected[regIdx].weight ) {
					continue;
				}
				selected[regIdx].cellX = work[i].cellX;
				selected[regIdx].cellY = work[i].cellY;
				selected[regIdx].weight = work[i].score;
				continue;
			}
		}
		if ( selCount < WORLD_RESIDENCY_MAX_SET ) {
			selected[selCount].cellX = work[i].cellX;
			selected[selCount].cellY = work[i].cellY;
			selected[selCount].regionId = work[i].regionId;
			selected[selCount].weight = work[i].score;
			selCount++;
		}
	}

	for ( i = 0; i < currentCount && selCount < outMax; i++ ) {
		qboolean already = qfalse;
		int j;
		for ( j = 0; j < selCount; j++ ) {
			if ( WR_CellEqual( selected[j].cellX, selected[j].cellY,
				current[i].cellX, current[i].cellY ) ) {
				already = qtrue;
				break;
			}
		}
		if ( !already && selCount < WORLD_RESIDENCY_MAX_SET ) {
			selected[selCount].cellX = current[i].cellX;
			selected[selCount].cellY = current[i].cellY;
			selected[selCount].regionId =
				WorldProc_RegionAtSector( current[i].cellX, current[i].cellY, 4096.0f );
			selected[selCount].weight = 0.5f;
			selCount++;
		}
	}

	for ( i = 0; i < selCount && outCount < outMax; i++ ) {
		out[outCount].cellX = selected[i].cellX;
		out[outCount].cellY = selected[i].cellY;
		outCount++;
	}

	(void)layer;
	return outCount;
}

int WorldResidency_SymmetricDifference( const worldResidencyCell_t *a, int aCount,
	const worldResidencyCell_t *b, int bCount ) {
	int i, j;
	int diff = 0;

	for ( i = 0; i < aCount; i++ ) {
		if ( !WR_InSet( b, bCount, a[i].cellX, a[i].cellY ) ) {
			diff++;
		}
	}
	for ( j = 0; j < bCount; j++ ) {
		if ( !WR_InSet( a, aCount, b[j].cellX, b[j].cellY ) ) {
			diff++;
		}
	}
	return diff;
}

static void WR_AddCandidateUnique( worldResidencyCandidate_t *candidates, int *candidateCount,
	int cellX, int cellY, float score, int regionId ) {
	int i;

	for ( i = 0; i < *candidateCount; i++ ) {
		if ( candidates[i].cellX == cellX && candidates[i].cellY == cellY ) {
			if ( score > candidates[i].score ) {
				candidates[i].score = score;
			}
			return;
		}
	}
	if ( *candidateCount >= WORLD_RESIDENCY_MAX_CANDIDATES ) {
		return;
	}
	candidates[*candidateCount].cellX = cellX;
	candidates[*candidateCount].cellY = cellY;
	candidates[*candidateCount].score = score;
	candidates[*candidateCount].regionId = regionId;
	( *candidateCount )++;
}

static void WR_EnumerateCandidatesAround( const vec3_t viewOrigin, float loadRadius,
	float unloadRadius, float sectorSize, worldResidencyLayer_t layer,
	wrLayerState_t *st, worldResidencyCandidate_t *candidates, int *candidateCount ) {
	int centerX, centerY;
	int minX, maxX, minY, maxY;
	int cellRadiusUnload;
	int x, y;

	CM_Stream_WorldToCell( viewOrigin, sectorSize, &centerX, &centerY );
	cellRadiusUnload = (int)ceil( unloadRadius / sectorSize ) + 1;
	minX = centerX - cellRadiusUnload;
	maxX = centerX + cellRadiusUnload;
	minY = centerY - cellRadiusUnload;
	maxY = centerY + cellRadiusUnload;

	for ( y = minY; y <= maxY; y++ ) {
		for ( x = minX; x <= maxX; x++ ) {
			vec3_t center;
			float dist;
			qboolean sticky;
			qboolean inLoad;
			qboolean inKeep;
			float score;
			int regionId;

			if ( !WR_PassDistrictFilter( x, y ) ) {
				continue;
			}

			center[0] = ( (float)x + 0.5f ) * sectorSize;
			center[1] = ( (float)y + 0.5f ) * sectorSize;
			center[2] = viewOrigin[2];
			dist = Distance( viewOrigin, center );
			inLoad = dist <= loadRadius;
			inKeep = dist <= unloadRadius;
			sticky = WR_InCurrent( st, x, y );

			if ( !inKeep && !sticky ) {
				continue;
			}
			if ( !inLoad && !sticky ) {
				continue;
			}
			if ( layer == WR_LAYER_COLLISION && !WR_ServerAllows( x, y ) ) {
				continue;
			}

			regionId = WorldProc_RegionAtSector( x, y, sectorSize );
			score = WorldResidency_ScoreCell( layer, x, y, viewOrigin, loadRadius, sectorSize, sticky );
			if ( score <= 0.0f && !sticky ) {
				continue;
			}
			WR_AddCandidateUnique( candidates, candidateCount, x, y, score, regionId );
		}
	}
}

static void WR_BuildTarget( worldResidencyLayer_t layer, const vec3_t viewOrigin,
	const vec3_t *extraOrigins, int extraOriginCount,
	float loadRadius, float unloadRadius, float sectorSize, qboolean useMatroid,
	qboolean collisionServerMode ) {
	wrLayerState_t *st = &s_layers[layer];
	worldResidencyCandidate_t candidates[WORLD_RESIDENCY_MAX_CANDIDATES];
	worldResidencyCell_t current[WORLD_RESIDENCY_MAX_SET];
	int candidateCount = 0;
	int currentCount = 0;
	int i;
	int o;

	if ( collisionServerMode ) {
		WR_SyncCurrentCollisionFromStream( sectorSize );
	} else {
		WR_SyncCurrentFromWorld( layer, sectorSize );
	}
	currentCount = st->currentCount;
	for ( i = 0; i < currentCount; i++ ) {
		current[i].cellX = st->current[i].cellX;
		current[i].cellY = st->current[i].cellY;
	}

	WR_EnumerateCandidatesAround( viewOrigin, loadRadius, unloadRadius, sectorSize,
		layer, st, candidates, &candidateCount );
	for ( o = 0; o < extraOriginCount; o++ ) {
		if ( extraOrigins ) {
			WR_EnumerateCandidatesAround( extraOrigins[o], loadRadius, unloadRadius, sectorSize,
				layer, st, candidates, &candidateCount );
		}
	}

	st->targetCount = WorldResidency_SelectCardinality( layer, candidates, candidateCount,
		current, currentCount,
		(worldResidencyCell_t *)st->target, WORLD_RESIDENCY_MAX_SET, useMatroid );
	for ( i = 0; i < st->targetCount; i++ ) {
		st->target[i].regionId = WorldProc_RegionAtSector(
			st->target[i].cellX, st->target[i].cellY, sectorSize );
		st->target[i].weight = WorldResidency_ScoreCell(
			layer, st->target[i].cellX, st->target[i].cellY,
			viewOrigin, loadRadius, sectorSize, qfalse );
	}
}

static void WR_ComputePendingDelta( worldResidencyLayer_t layer ) {
	wrLayerState_t *st = &s_layers[layer];
	int i, j;

	st->pendingAddCount = 0;
	st->pendingRemoveCount = 0;

	for ( i = 0; i < st->targetCount; i++ ) {
		if ( !WR_InCurrent( st, st->target[i].cellX, st->target[i].cellY ) ) {
			if ( st->pendingAddCount < WORLD_RESIDENCY_MAX_SET ) {
				st->pendingAdd[st->pendingAddCount].cellX = st->target[i].cellX;
				st->pendingAdd[st->pendingAddCount].cellY = st->target[i].cellY;
				st->pendingAdd[st->pendingAddCount].regionId = st->target[i].regionId;
				st->pendingAdd[st->pendingAddCount].weight = st->target[i].weight;
				st->pendingAddCount++;
			}
		}
	}
	for ( j = 0; j < st->currentCount; j++ ) {
		qboolean inTarget = qfalse;
		for ( i = 0; i < st->targetCount; i++ ) {
			if ( WR_CellEqual( st->current[j].cellX, st->current[j].cellY,
				st->target[i].cellX, st->target[i].cellY ) ) {
				inTarget = qtrue;
				break;
			}
		}
		if ( !inTarget && st->pendingRemoveCount < WORLD_RESIDENCY_MAX_SET ) {
			st->pendingRemove[st->pendingRemoveCount] = st->current[j];
			st->pendingRemoveCount++;
		}
	}
}

static qboolean WR_InTransitionWindow( int tick, int k ) {
	float eps = WR_Epsilon();
	int period = k > 0 ? k : 64;
	int phase;

	phase = ( tick + s_scheduleShift ) % period;
	return phase < (int)( eps * (float)period + 0.5f );
}

static void WR_StartTransitionIfNeeded( worldResidencyLayer_t layer ) {
	wrLayerState_t *st = &s_layers[layer];
	int diff;
	int k = WR_BudgetForLayer( layer );

	diff = WorldResidency_SymmetricDifference(
		(const worldResidencyCell_t *)st->current, st->currentCount,
		(const worldResidencyCell_t *)st->target, st->targetCount );

	if ( diff == 0 ) {
		st->inTransition = qfalse;
		st->pendingAddCount = 0;
		st->pendingRemoveCount = 0;
		return;
	}

	WR_ComputePendingDelta( layer );
	st->transitionTotal = (int)( WR_Epsilon() * (float)k + 0.5f );
	if ( st->transitionTotal < 1 ) {
		st->transitionTotal = 1;
	}
	if ( st->transitionTotal > diff ) {
		st->transitionTotal = diff;
	}
	st->transitionStep = 0;
	st->inTransition = qtrue;
}

static void WR_ApplyLayerStep( worldResidencyLayer_t layer, worldOpenLayerMask_t mask,
	qboolean collisionOnlyStream ) {
	wrLayerState_t *st = &s_layers[layer];
	int maxOps = WR_MaxSwaps();
	int ops = 0;

	if ( st->inTransition && WR_InTransitionWindow( st->tick, WR_BudgetForLayer( layer ) ) ) {
		int perStep = ( st->pendingAddCount + st->pendingRemoveCount + st->transitionTotal - 1 )
			/ st->transitionTotal;
		if ( perStep < 1 ) {
			perStep = 1;
		}
		maxOps = perStep;
	}

	while ( st->pendingRemoveCount > 0 && ops < maxOps ) {
		int cx = st->pendingRemove[st->pendingRemoveCount - 1].cellX;
		int cy = st->pendingRemove[st->pendingRemoveCount - 1].cellY;
		st->pendingRemoveCount--;
		if ( collisionOnlyStream ) {
			CM_Stream_UnloadSector( cx, cy );
		} else {
			WorldOpen_UnloadSectorLayers( cx, cy, mask );
		}
		ops++;
	}

	while ( st->pendingAddCount > 0 && ops < maxOps ) {
		int cx = st->pendingAdd[st->pendingAddCount - 1].cellX;
		int cy = st->pendingAdd[st->pendingAddCount - 1].cellY;
		st->pendingAddCount--;
		if ( collisionOnlyStream ) {
			(void)CM_Stream_LoadSector( cx, cy );
		} else {
			(void)WorldOpen_LoadSector( cx, cy, mask );
		}
		ops++;
	}

	if ( st->inTransition ) {
		st->transitionStep++;
		if ( st->transitionStep >= st->transitionTotal ||
			( st->pendingAddCount == 0 && st->pendingRemoveCount == 0 ) ) {
			st->inTransition = qfalse;
		}
	} else if ( st->pendingAddCount == 0 && st->pendingRemoveCount == 0 ) {
		WR_ComputePendingDelta( layer );
	}
}

static void WR_UpdateLayer( worldResidencyLayer_t layer, const vec3_t viewOrigin,
	const vec3_t *extraOrigins, int extraOriginCount,
	float loadRadius, float unloadRadius, float sectorSize,
	worldOpenLayerMask_t mask, qboolean useMatroid, qboolean collisionServerMode ) {
	wrLayerState_t *st = &s_layers[layer];

	st->tick++;
	WR_BuildTarget( layer, viewOrigin, extraOrigins, extraOriginCount,
		loadRadius, unloadRadius, sectorSize, useMatroid, collisionServerMode );

	if ( !st->inTransition ) {
		WR_StartTransitionIfNeeded( layer );
	}
	if ( !st->inTransition ) {
		WR_ComputePendingDelta( layer );
	}

	WR_ApplyLayerStep( layer, mask, collisionServerMode );
}

static void WR_LogEnableOnce( void ) {
	if ( s_loggedEnable ) {
		return;
	}
	s_loggedEnable = qtrue;
	Com_Printf( "[world_residency] enabled epsilon=%.3f k_col=%d max_swaps=%d matroid=%s\n",
		WR_Epsilon(),
		WR_BudgetForLayer( WR_LAYER_COLLISION ),
		WR_MaxSwaps(),
		( r_openWorldResidencyMatroid && r_openWorldResidencyMatroid->integer ) ? "on" : "off" );
}

void WorldResidency_UpdateView( const vec3_t viewOrigin, float radius, worldOpenLayerMask_t enabledLayers ) {
	float sectorSize;
	float loadRadius;
	float unloadRadius;
	qboolean useMatroid;

	if ( !WorldResidency_IsEnabled() || !viewOrigin ) {
		return;
	}

	WR_LogEnableOnce();

	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}
	loadRadius = r_openWorldLoadRadius ? r_openWorldLoadRadius->value : 12288.0f;
	unloadRadius = r_openWorldUnloadRadius ? r_openWorldUnloadRadius->value : 14336.0f;
	if ( radius > 0.0f ) {
		loadRadius = radius;
		if ( unloadRadius < loadRadius ) {
			unloadRadius = loadRadius * 1.15f;
		}
	}
	if ( unloadRadius < loadRadius ) {
		unloadRadius = loadRadius;
	}

	useMatroid = r_openWorldResidencyMatroid && r_openWorldResidencyMatroid->integer
		&& Cvar_VariableIntegerValue( "r_proc" );

	if ( enabledLayers & WO_LAYER_MASK_COLLISION ) {
		WR_UpdateLayer( WR_LAYER_COLLISION, viewOrigin, NULL, 0, loadRadius, unloadRadius, sectorSize,
			WO_LAYER_MASK_COLLISION, useMatroid, qfalse );
	}
	if ( enabledLayers & WO_LAYER_MASK_NAV ) {
		if ( !cl_openWorldResidencyNavLocal || !cl_openWorldResidencyNavLocal->integer ) {
			if ( s_serverAllowCount == 0 ) {
				WR_UpdateLayer( WR_LAYER_NAV, viewOrigin, NULL, 0, loadRadius, unloadRadius, sectorSize,
					WO_LAYER_MASK_NAV, qfalse, qfalse );
			}
		} else {
			WR_UpdateLayer( WR_LAYER_NAV, viewOrigin, NULL, 0, loadRadius, unloadRadius, sectorSize,
				WO_LAYER_MASK_NAV, qfalse, qfalse );
		}
	}
	if ( enabledLayers & WO_LAYER_MASK_SPRITES ) {
		WR_UpdateLayer( WR_LAYER_SPRITES, viewOrigin, NULL, 0, loadRadius, unloadRadius, sectorSize,
			WO_LAYER_MASK_SPRITES, qfalse, qfalse );
	}

	if ( Cvar_VariableIntegerValue( "cm_stream" ) &&
		( enabledLayers & WO_LAYER_MASK_COLLISION ) &&
		Cvar_VariableIntegerValue( "cm_openWorldCollision" ) ) {
		CM_Stream_UpdateView( viewOrigin, loadRadius, sectorSize, qtrue );
	}
}

void WorldResidency_UpdateServerOrigins( const vec3_t *origins, int originCount, float radius ) {
	vec3_t centroid;
	float sectorSize;
	float loadRadius;
	float unloadRadius;
	qboolean useMatroid;

	if ( !WorldResidency_ServerEnabled() || !origins || originCount <= 0 ) {
		return;
	}

	WR_LogEnableOnce();

	VectorCopy( origins[0], centroid );
	if ( originCount > 1 ) {
		int i;
		VectorClear( centroid );
		for ( i = 0; i < originCount; i++ ) {
			VectorAdd( centroid, origins[i], centroid );
		}
		centroid[0] /= (float)originCount;
		centroid[1] /= (float)originCount;
		centroid[2] /= (float)originCount;
	}

	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}
	loadRadius = r_openWorldLoadRadius ? r_openWorldLoadRadius->value : 12288.0f;
	unloadRadius = r_openWorldUnloadRadius ? r_openWorldUnloadRadius->value : 14336.0f;
	if ( radius > 0.0f ) {
		loadRadius = radius;
	}
	if ( unloadRadius < loadRadius ) {
		unloadRadius = loadRadius * 1.15f;
	}

	useMatroid = r_openWorldResidencyMatroid && r_openWorldResidencyMatroid->integer
		&& Cvar_VariableIntegerValue( "r_proc" );

	WorldResidency_ClearServerCollisionAllowList();

	WR_UpdateLayer( WR_LAYER_COLLISION, centroid, origins, originCount,
		loadRadius, unloadRadius, sectorSize,
		WO_LAYER_MASK_COLLISION, useMatroid, qtrue );
}

#ifdef WORLD_RESIDENCY_UNIT_TEST
void WorldResidency_ResetStateForTest( void ) {
	Com_Memset( s_layers, 0, sizeof( s_layers ) );
	s_serverAllowCount = 0;
	s_districtFilterActive = qfalse;
	s_scheduleShift = 0;
}
#endif
