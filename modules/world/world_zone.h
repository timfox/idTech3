/*
 * Engine-original spatial zone graph.
 *
 * Zones provide bounded, authored residency and visibility hints. They are
 * intentionally independent of legacy BSP formats and can be populated by
 * USDA, glTF, or a future world compiler.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define WORLD_ZONE_MAX 128
#define WORLD_ZONE_NAME_MAX 64
#define WORLD_ZONE_NEIGHBOR_MAX 8
#define WORLD_ZONE_RESIDENCY_DISTRICT  ( 1u << 0 )
#define WORLD_ZONE_RESIDENCY_TEXTURE   ( 1u << 1 )
#define WORLD_ZONE_RESIDENCY_SHADOW    ( 1u << 2 )
#define WORLD_ZONE_RESIDENCY_ALL       ( WORLD_ZONE_RESIDENCY_DISTRICT | WORLD_ZONE_RESIDENCY_TEXTURE | WORLD_ZONE_RESIDENCY_SHADOW )

typedef enum {
	WZ_STATE_INACTIVE = 0,
	WZ_STATE_RESIDENT,
	WZ_STATE_PENDING_LOAD,
	WZ_STATE_PENDING_UNLOAD
} worldZoneState_t;

typedef struct worldZone_s {
	qboolean active;
	char name[WORLD_ZONE_NAME_MAX];
	vec3_t boundsMin;
	vec3_t boundsMax;
	float loadRadius;
	float unloadRadius;
	float priority;
	uint32_t residencyMask;
	int districtIndex;
	int neighbors[WORLD_ZONE_NEIGHBOR_MAX];
	int neighborCount;
	worldZoneState_t state;
	int lastScore;
} worldZone_t;

typedef qboolean ( *worldZoneLoad_f )( int index, const worldZone_t *zone );
typedef void ( *worldZoneUnload_f )( int index, const worldZone_t *zone );
typedef void ( *worldZoneResidency_f )( int index, const worldZone_t *zone, uint32_t mask, qboolean resident );

void WorldZone_Init( void );
void WorldZone_Shutdown( void );
void WorldZone_SetCallbacks( worldZoneLoad_f loadFn, worldZoneUnload_f unloadFn );
void WorldZone_SetResidencyCallback( worldZoneResidency_f fn );
void WorldZone_Import( int count, const worldZone_t *zones );
void WorldZone_Clear( void );
void WorldZone_UpdateView( const vec3_t viewOrigin );
int WorldZone_GetCount( void );
const worldZone_t *WorldZone_Get( int index );
int WorldZone_FindAtPoint( const vec3_t point );
qboolean WorldZone_IsLayerResidentAtPoint( const vec3_t point, uint32_t layer );
void WorldZone_Status( void );

#ifdef __cplusplus
}
#endif
