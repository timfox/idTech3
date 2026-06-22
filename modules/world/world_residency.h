/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Consistent submodular sector residency — value-aware selection under
per-layer budgets with bounded symmetric-difference updates.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"
#include "world_open.h"

#define WORLD_RESIDENCY_MAX_SET       128
#define WORLD_RESIDENCY_MAX_CANDIDATES 256
#define WORLD_RESIDENCY_CM_MERGE_MAX  64

typedef enum {
	WR_LAYER_COLLISION = 0,
	WR_LAYER_NAV,
	WR_LAYER_SPRITES,
	WR_LAYER_COUNT
} worldResidencyLayer_t;

typedef struct {
	int cellX;
	int cellY;
} worldResidencyCell_t;

typedef struct {
	int   cellX;
	int   cellY;
	float score;
	int   regionId;
} worldResidencyCandidate_t;

void     WorldResidency_Init( void );
void     WorldResidency_Shutdown( void );
qboolean WorldResidency_IsEnabled( void );
qboolean WorldResidency_ServerEnabled( void );

void WorldResidency_SetDistrictFilter( qboolean active, int x0, int y0, int x1, int y1 );
void WorldResidency_ClearDistrictFilter( void );

void WorldResidency_SetServerCollisionAllowList( const worldResidencyCell_t *cells, int count );
void WorldResidency_ClearServerCollisionAllowList( void );

void WorldResidency_UpdateView( const vec3_t viewOrigin, float radius, worldOpenLayerMask_t enabledLayers );
void WorldResidency_UpdateServerOrigins( const vec3_t *origins, int originCount, float radius );

float WorldResidency_ScoreCell( worldResidencyLayer_t layer, int cellX, int cellY,
	const vec3_t viewOrigin, float loadRadius, float sectorSize, qboolean sticky );

int WorldResidency_SelectCardinality( worldResidencyLayer_t layer,
	const worldResidencyCandidate_t *candidates, int candidateCount,
	const worldResidencyCell_t *current, int currentCount,
	worldResidencyCell_t *out, int outMax, qboolean useMatroid );

int WorldResidency_SymmetricDifference( const worldResidencyCell_t *a, int aCount,
	const worldResidencyCell_t *b, int bCount );

#ifdef WORLD_RESIDENCY_UNIT_TEST
void WorldResidency_ResetStateForTest( void );
#endif

#ifdef __cplusplus
}
#endif
