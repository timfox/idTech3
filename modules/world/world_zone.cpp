/* Engine-original bounded zone graph; no legacy compiler or SDK code. */
extern "C" {
#include "q_shared.h"
#include "qcommon.h"
#include "world_zone.h"

#include <math.h>

static worldZone_t s_zones[WORLD_ZONE_MAX];
static int s_zoneCount;
static worldZoneLoad_f s_loadFn;
static worldZoneUnload_f s_unloadFn;
static cvar_t *r_worldZones;
static cvar_t *r_worldZoneBudget;
static cvar_t *r_worldZoneLoadRadius;
static cvar_t *r_worldZoneUnloadRadius;

static qboolean WZ_PointInBounds( const worldZone_t *zone, const vec3_t point ) {
	return point[0] >= zone->boundsMin[0] && point[0] <= zone->boundsMax[0] &&
		point[1] >= zone->boundsMin[1] && point[1] <= zone->boundsMax[1] &&
		point[2] >= zone->boundsMin[2] && point[2] <= zone->boundsMax[2];
}

static float WZ_DistanceToBounds( const worldZone_t *zone, const vec3_t point ) {
	float d2 = 0.0f;
	int axis;
	for ( axis = 0; axis < 3; ++axis ) {
		float delta = 0.0f;
		if ( point[axis] < zone->boundsMin[axis] ) delta = zone->boundsMin[axis] - point[axis];
		else if ( point[axis] > zone->boundsMax[axis] ) delta = point[axis] - zone->boundsMax[axis];
		d2 += delta * delta;
	}
	return sqrtf( d2 );
}

static float WZ_LoadRadius( const worldZone_t *zone ) {
	float radius = zone->loadRadius > 0.0f ? zone->loadRadius : r_worldZoneLoadRadius->value;
	return MAX( radius, 1.0f );
}

static float WZ_UnloadRadius( const worldZone_t *zone ) {
	float radius = zone->unloadRadius > 0.0f ? zone->unloadRadius : r_worldZoneUnloadRadius->value;
	return MAX( radius, WZ_LoadRadius( zone ) );
}

static int WZ_Score( const worldZone_t *zone, const vec3_t point, qboolean inside ) {
	float distance = WZ_DistanceToBounds( zone, point );
	float radius = WZ_LoadRadius( zone );
	float normalized = 1.0f - MIN( distance / radius, 1.0f );
	return (int)( normalized * 1000.0f + zone->priority * 100.0f + (inside ? 10000.0f : 0.0f) );
}

void WorldZone_Init( void ) {
	r_worldZones = Cvar_Get( "r_worldZones", "1", CVAR_ARCHIVE );
	r_worldZoneBudget = Cvar_Get( "r_worldZoneBudget", "16", CVAR_ARCHIVE );
	r_worldZoneLoadRadius = Cvar_Get( "r_worldZoneLoadRadius", "1024", CVAR_ARCHIVE );
	r_worldZoneUnloadRadius = Cvar_Get( "r_worldZoneUnloadRadius", "1536", CVAR_ARCHIVE );
	Cvar_SetDescription( r_worldZones, "Enable authored spatial zone residency hints." );
	Cvar_SetDescription( r_worldZoneBudget, "Maximum resident zones selected per view." );
	Cvar_SetDescription( r_worldZoneLoadRadius, "Default zone activation radius in world units." );
	Cvar_SetDescription( r_worldZoneUnloadRadius, "Default zone release radius in world units." );
	WorldZone_Clear();
}

void WorldZone_Shutdown( void ) {
	WorldZone_Clear();
	s_loadFn = NULL;
	s_unloadFn = NULL;
}

void WorldZone_SetCallbacks( worldZoneLoad_f loadFn, worldZoneUnload_f unloadFn ) {
	s_loadFn = loadFn;
	s_unloadFn = unloadFn;
}

void WorldZone_Clear( void ) {
	int i;
	for ( i = 0; i < s_zoneCount; ++i ) {
		if ( s_zones[i].active && s_zones[i].state == WZ_STATE_RESIDENT && s_unloadFn ) {
			s_unloadFn( i, &s_zones[i] );
		}
	}
	Com_Memset( s_zones, 0, sizeof( s_zones ) );
	s_zoneCount = 0;
}

void WorldZone_Import( int count, const worldZone_t *zones ) {
	int i;
	WorldZone_Clear();
	if ( !zones || count <= 0 ) return;
	count = MIN( count, WORLD_ZONE_MAX );
	for ( i = 0; i < count; ++i ) {
		s_zones[i] = zones[i];
		s_zones[i].active = qtrue;
		s_zones[i].state = WZ_STATE_INACTIVE;
		s_zones[i].neighborCount = MIN( s_zones[i].neighborCount, WORLD_ZONE_NEIGHBOR_MAX );
	}
	s_zoneCount = count;
}

static int WZ_Budget( void ) {
	int budget = r_worldZoneBudget ? r_worldZoneBudget->integer : 16;
	return MAX( 1, MIN( budget, WORLD_ZONE_MAX ) );
}

void WorldZone_UpdateView( const vec3_t point ) {
	int scores[WORLD_ZONE_MAX];
	qboolean wanted[WORLD_ZONE_MAX];
	int i, j, budget;
	if ( !point || !r_worldZones || !r_worldZones->integer ) return;
	budget = WZ_Budget();
	Com_Memset( wanted, 0, sizeof( wanted ) );
	for ( i = 0; i < s_zoneCount; ++i ) {
		qboolean inside = WZ_PointInBounds( &s_zones[i], point );
		float distance = WZ_DistanceToBounds( &s_zones[i], point );
		scores[i] = WZ_Score( &s_zones[i], point, inside );
		if ( distance <= WZ_LoadRadius( &s_zones[i] ) ) wanted[i] = qtrue;
		if ( s_zones[i].state == WZ_STATE_RESIDENT && distance <= WZ_UnloadRadius( &s_zones[i] ) ) wanted[i] = qtrue;
	}
	/* Always retain the highest scoring candidates within the fixed budget. */
	for ( j = 0; j < budget; ++j ) {
		int best = -1;
		for ( i = 0; i < s_zoneCount; ++i ) {
			if ( wanted[i] && (best < 0 || scores[i] > scores[best]) ) best = i;
		}
		if ( best < 0 ) break;
		wanted[best] = qfalse;
		if ( s_zones[best].state != WZ_STATE_RESIDENT ) {
			s_zones[best].lastScore = scores[best];
			s_zones[best].state = WZ_STATE_PENDING_LOAD;
			if ( !s_loadFn || s_loadFn( best, &s_zones[best] ) ) s_zones[best].state = WZ_STATE_RESIDENT;
		}
	}
	for ( i = 0; i < s_zoneCount; ++i ) {
		if ( s_zones[i].state == WZ_STATE_RESIDENT && WZ_DistanceToBounds( &s_zones[i], point ) > WZ_UnloadRadius( &s_zones[i] ) ) {
			s_zones[i].state = WZ_STATE_PENDING_UNLOAD;
			if ( s_unloadFn ) s_unloadFn( i, &s_zones[i] );
			s_zones[i].state = WZ_STATE_INACTIVE;
		}
	}
}

int WorldZone_GetCount( void ) { return s_zoneCount; }

const worldZone_t *WorldZone_Get( int index ) {
	if ( index < 0 || index >= s_zoneCount || !s_zones[index].active ) return NULL;
	return &s_zones[index];
}

int WorldZone_FindAtPoint( const vec3_t point ) {
	int i;
	if ( !point ) return -1;
	for ( i = 0; i < s_zoneCount; ++i ) if ( WZ_PointInBounds( &s_zones[i], point ) ) return i;
	return -1;
}

void WorldZone_Status( void ) {
	int i;
	for ( i = 0; i < s_zoneCount; ++i ) {
		Com_Printf( "zone %d %-24s state=%d bounds=(%.0f %.0f %.0f)-(%.0f %.0f %.0f) score=%d\n",
			i, s_zones[i].name, s_zones[i].state,
			s_zones[i].boundsMin[0], s_zones[i].boundsMin[1], s_zones[i].boundsMin[2],
			s_zones[i].boundsMax[0], s_zones[i].boundsMax[1], s_zones[i].boundsMax[2], s_zones[i].lastScore );
	}
}
}
