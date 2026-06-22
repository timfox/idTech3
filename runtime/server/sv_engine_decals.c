/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Networked engine decal entityState contract:
  modelindex   1-based CS_ENGINE_DECAL_SHADERS index
  generic1     radius / 4
  eventParm    fade duration in server ticks (0 = default)
  apos.trBase[0] pitch (degrees)
  apos.trBase[1] yaw (degrees)
  eFlags       EF_DECAL
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/engine_decal_map.h"
#include "../game/bg_public.h"
#include "server.h"
#include "sv_engine_decals.h"

static cvar_t *sv_engineDecals;
static cvar_t *sv_engineDecalsSpawn;
static int sv_numEngineDecalShaders;
static engineDecalMapList_t sv_mapDecalList;
static int sv_mapDecalSpawnCount;

static void SV_EngineDecalFillEntityState( const engineDecalMapDef_t *def, int shaderIndex,
	entityState_t *state )
{
	int radiusQuarters;
	int fadeTicks;

	if ( !def || !state || shaderIndex <= 0 ) {
		return;
	}

	Com_Memset( state, 0, sizeof( *state ) );
	state->eType = ET_GENERAL;
	state->pos.trType = TR_STATIONARY;
	VectorCopy( def->origin, state->pos.trBase );
	state->modelindex = shaderIndex;
	state->eFlags |= EF_DECAL;

	radiusQuarters = (int)( def->radius * 0.25f );
	if ( radiusQuarters <= 0 ) {
		radiusQuarters = 8;
	}
	state->generic1 = radiusQuarters;

	fadeTicks = (int)( def->fadeSec * 1000.0f );
	if ( fadeTicks < 0 ) {
		fadeTicks = 0;
	}
	state->eventParm = fadeTicks;

	state->apos.trType = TR_STATIONARY;
	state->apos.trBase[0] = def->pitch;
	state->apos.trBase[1] = def->yaw;
}

static int SV_EngineDecal_LinkFromDef( const engineDecalMapDef_t *def, int shaderIndex ) {
	sharedEntity_t *ent;
	int entNum;

	if ( !def || shaderIndex <= 0 ) {
		return -1;
	}
	if ( !sv.gentities || sv.gentitySize <= 0 ) {
		return -1;
	}
	if ( sv.num_entities >= MAX_GENTITIES - 2 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: entity limit hit; engine decal spawn failed\n" );
		return -1;
	}

	entNum = sv.num_entities++;
	ent = SV_GentityNum( entNum );
	Com_Memset( ent, 0, sv.gentitySize );

	ent->s.number = entNum;
	SV_EngineDecalFillEntityState( def, shaderIndex, &ent->s );

	ent->r.linked = qfalse;
	ent->r.svFlags = 0;
	ent->r.mins[0] = ent->r.mins[1] = ent->r.mins[2] = -4.0f;
	ent->r.maxs[0] = ent->r.maxs[1] = ent->r.maxs[2] = 4.0f;
	VectorCopy( def->origin, ent->r.currentOrigin );
	VectorClear( ent->r.currentAngles );

	SV_LinkEntity( ent );
	return entNum;
}

int SV_EngineDecal_SpawnFromDef( const engineDecalMapDef_t *def ) {
	int shaderIndex;

	if ( !sv_engineDecals || !sv_engineDecals->integer ) {
		return -1;
	}
	if ( !def || !def->shader[0] ) {
		return -1;
	}

	shaderIndex = SV_EngineDecalShaderIndex( def->shader );
	if ( shaderIndex <= 0 ) {
		return -1;
	}

	return SV_EngineDecal_LinkFromDef( def, shaderIndex );
}

static void SV_DecalSpawn_f( void ) {
	engineDecalMapDef_t def;
	const char *shaderArg;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "usage: sv_decal_spawn <shader> <x> <y> <z> [radius] [pitch] [yaw] [fade]\n" );
		return;
	}

	Com_Memset( &def, 0, sizeof( def ) );
	shaderArg = Cmd_Argv( 1 );
	Q_strncpyz( def.shader, shaderArg, sizeof( def.shader ) );
	def.origin[0] = (float)atof( Cmd_Argv( 2 ) );
	def.origin[1] = (float)atof( Cmd_Argv( 3 ) );
	def.origin[2] = (float)atof( Cmd_Argv( 4 ) );
	def.radius = ( Cmd_Argc() > 5 ) ? (float)atof( Cmd_Argv( 5 ) ) : 32.0f;
	def.pitch = ( Cmd_Argc() > 6 ) ? (float)atof( Cmd_Argv( 6 ) ) : 0.0f;
	def.yaw = ( Cmd_Argc() > 7 ) ? (float)atof( Cmd_Argv( 7 ) ) : 0.0f;
	def.fadeSec = ( Cmd_Argc() > 8 ) ? (float)atof( Cmd_Argv( 8 ) ) : 0.0f;

	Com_Printf( "sv_decal_spawn -> ent %d\n", SV_EngineDecal_SpawnFromDef( &def ) );
}

int SV_EngineDecalShaderIndex( const char *shaderName ) {
	int i;

	if ( !shaderName || !shaderName[0] ) {
		return 0;
	}

	for ( i = 0; i < sv_numEngineDecalShaders; i++ ) {
		const char *existing = sv.configstrings[CS_ENGINE_DECAL_SHADERS + i];
		if ( existing && existing[0] && !Q_stricmp( existing, shaderName ) ) {
			return i + 1;
		}
	}

	if ( sv_numEngineDecalShaders >= MAX_ENGINE_DECAL_SHADERS ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: MAX_ENGINE_DECAL_SHADERS hit\n" );
		return 0;
	}

	SV_SetConfigstring( CS_ENGINE_DECAL_SHADERS + sv_numEngineDecalShaders, shaderName );
	sv_numEngineDecalShaders++;
	return sv_numEngineDecalShaders;
}

void SV_EngineDecals_Clear( void ) {
	int i;

	sv_numEngineDecalShaders = 0;
	sv_mapDecalSpawnCount = 0;
	EngineDecalMap_Clear( &sv_mapDecalList );

	for ( i = 0; i < MAX_ENGINE_DECAL_SHADERS; i++ ) {
		SV_SetConfigstring( CS_ENGINE_DECAL_SHADERS + i, "" );
	}
	SV_SetConfigstring( CS_ENGINE_DECAL_META, "" );
}

void SV_EngineDecals_LoadMap( const char *entityString ) {
	int i;

	SV_EngineDecals_Clear();

	if ( !sv_engineDecals || !sv_engineDecals->integer ) {
		return;
	}

	EngineDecalMap_Parse( entityString, &sv_mapDecalList );

	for ( i = 0; i < sv_mapDecalList.count; i++ ) {
		SV_EngineDecalShaderIndex( sv_mapDecalList.defs[i].shader );
	}

	if ( sv_mapDecalList.count > 0 ) {
		Com_Printf( "[engine][decals] map parse: %d misc_decal defs, %d CS shaders\n",
			sv_mapDecalList.count, sv_numEngineDecalShaders );
	}
}

void SV_EngineDecals_SpawnMapEntities( void ) {
	int i;

	if ( !sv_engineDecalsSpawn || !sv_engineDecalsSpawn->integer ) {
		return;
	}
	if ( !sv_engineDecals || !sv_engineDecals->integer ) {
		return;
	}

	sv_mapDecalSpawnCount = 0;
	for ( i = 0; i < sv_mapDecalList.count; i++ ) {
		if ( SV_EngineDecal_SpawnFromDef( &sv_mapDecalList.defs[i] ) >= 0 ) {
			sv_mapDecalSpawnCount++;
		}
	}

	if ( sv_mapDecalSpawnCount > 0 ) {
		SV_SetConfigstring( CS_ENGINE_DECAL_META, va( "%d", sv_mapDecalSpawnCount ) );
		Com_Printf( "[engine][decals] spawned %d map decals (CS_ENGINE_DECAL_META)\n",
			sv_mapDecalSpawnCount );
	}
}

void SV_EngineDecals_Init( void ) {
	sv_engineDecals = Cvar_Get( "sv_engineDecals", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_engineDecals,
		"Engine-native decal catalog (CS_ENGINE_DECAL_SHADERS) for networked EF_DECAL ents." );
	sv_engineDecalsSpawn = Cvar_Get( "sv_engineDecalsSpawn", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_engineDecalsSpawn,
		"Spawn misc_decal map entities into snapshots (disables client BSP map parse via CS_ENGINE_DECAL_META)." );
	Cmd_AddCommand( "sv_decal_spawn", SV_DecalSpawn_f );
	if ( sv_engineDecals->integer ) {
		Com_Printf( "[engine][decals] sv_engineDecals=1\n" );
	}
}
