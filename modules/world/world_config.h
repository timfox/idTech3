/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Named world configurations: alternate geometry / nav / spawn layouts /
lighting keys with optional validation (sightlines + gameplay bounds).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define WORLD_CONFIG_NAME_MAX       64
#define WORLD_CONFIG_MAX            32
#define WORLD_CONFIG_SIGHTLINE_MAX  32
#define WORLD_CONFIG_SPAWN_MAX      128
#define WORLD_CONFIG_PATH_MAX       MAX_QPATH

#ifdef USE_OPEN_WORLD

typedef enum {
	WC_SIGHT_CLEAR = 0,
	WC_SIGHT_BLOCKED
} worldConfigSightExpect_t;

typedef struct worldConfigSightline_s {
	vec3_t                   start;
	vec3_t                   end;
	worldConfigSightExpect_t expect;
	char                     label[32];
} worldConfigSightline_t;

typedef struct worldConfigSpawn_s {
	vec3_t   origin;
	vec3_t   angles;
	char     layout[WORLD_CONFIG_NAME_MAX];
	char     spawnType[64];
	float    minIntensity;
	float    maxIntensity;
	qboolean active;
} worldConfigSpawn_t;

typedef struct worldConfigEntry_s {
	char     name[WORLD_CONFIG_NAME_MAX];
	char     geometrySuffix[32];
	char     navSuffix[32];
	char     spawnLayout[WORLD_CONFIG_NAME_MAX];
	float    ndgiTime;
	qboolean hasNdgiTime;
	float    nivScale;
	qboolean hasNivScale;
	vec3_t   boundsMins;
	vec3_t   boundsMaxs;
	qboolean hasBounds;
	worldConfigSightline_t sightlines[WORLD_CONFIG_SIGHTLINE_MAX];
	int      sightlineCount;
	qboolean defined;
} worldConfigEntry_t;

typedef void ( *worldConfigOnApply_f )( const char *oldName, const char *newName, int generation );

void     WorldConfig_Init( void );
void     WorldConfig_Shutdown( void );
void     WorldConfig_Clear( void );

qboolean WorldConfig_LoadManifest( const char *mapBaseName );
qboolean WorldConfig_LoadManifestPath( const char *path );

qboolean WorldConfig_SetActive( const char *name );
qboolean WorldConfig_SetSpawnLayout( const char *layoutName );
const char *WorldConfig_GetActive( void );
const char *WorldConfig_GetSpawnLayout( void );
int      WorldConfig_GetGeneration( void );
qboolean WorldConfig_IsEnabled( void );

const worldConfigEntry_t *WorldConfig_GetEntry( const char *name );
int      WorldConfig_GetCount( void );
const worldConfigEntry_t *WorldConfig_GetByIndex( int index );

float    WorldConfig_GetNdgiTime( qboolean *hasValue );
float    WorldConfig_GetNivScale( qboolean *hasValue );

void     WorldConfig_FormatSectorBsp( int cellX, int cellY, char *out, int outSize );
void     WorldConfig_FormatSectorNav( int cellX, int cellY, char *out, int outSize );
void     WorldConfig_FormatScatter( int cellX, int cellY, char *out, int outSize );
qboolean WorldConfig_ResolveReadable( const char *preferred, const char *fallback,
	char *out, int outSize );

void     WorldConfig_SetOnApply( worldConfigOnApply_f fn );

void     WorldConfig_ClearSpawns( void );
int      WorldConfig_AddSpawn( const vec3_t origin, const vec3_t angles,
	const char *layout, const char *spawnType, float minIntensity, float maxIntensity );
int      WorldConfig_GetSpawnCount( void );
const worldConfigSpawn_t *WorldConfig_GetSpawn( int index );
int      WorldConfig_CollectSpawnsForLayout( const char *layout,
	const worldConfigSpawn_t **out, int maxOut );

int      WorldConfig_Validate( const char *nameOrNull, char *report, int reportSize );
void     WorldConfig_List( void );
void     WorldConfig_Status( void );

#else /* !USE_OPEN_WORLD */

static inline void WorldConfig_Init( void ) {}
static inline void WorldConfig_Shutdown( void ) {}
static inline void WorldConfig_Clear( void ) {}
static inline qboolean WorldConfig_LoadManifest( const char *mapBaseName ) { (void)mapBaseName; return qfalse; }
static inline qboolean WorldConfig_IsEnabled( void ) { return qfalse; }
static inline const char *WorldConfig_GetActive( void ) { return "default"; }
static inline const char *WorldConfig_GetSpawnLayout( void ) { return "default"; }
static inline int WorldConfig_GetGeneration( void ) { return 0; }
static inline void WorldConfig_FormatSectorBsp( int cellX, int cellY, char *out, int outSize ) {
	Com_sprintf( out, outSize, "maps/sector_%d_%d.bsp", cellX, cellY );
}
static inline void WorldConfig_FormatSectorNav( int cellX, int cellY, char *out, int outSize ) {
	Com_sprintf( out, outSize, "nav/sector_%d_%d.nav", cellX, cellY );
}
static inline void WorldConfig_FormatScatter( int cellX, int cellY, char *out, int outSize ) {
	Com_sprintf( out, outSize, "sprites/sector_%d_%d.ents", cellX, cellY );
}
static inline qboolean WorldConfig_ResolveReadable( const char *preferred, const char *fallback,
	char *out, int outSize ) {
	if ( preferred && preferred[0] ) {
		Q_strncpyz( out, preferred, outSize );
		return qtrue;
	}
	if ( fallback && fallback[0] ) {
		Q_strncpyz( out, fallback, outSize );
		return qtrue;
	}
	out[0] = '\0';
	return qfalse;
}

#endif /* USE_OPEN_WORLD */

#ifdef __cplusplus
}
#endif
