/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Networked engine sprite entityState contract (see also cl_engine_sprites.c):
  modelindex   1-based index into CS_ENGINE_SPRITE_SHADERS (shader path string)
  modelindex2  flipbook: cols (low 8) | rows (high 8)
  generic1     radius / 4 in world units (0 -> default 32)
  eventParm    flipbook fps (default 8)
  apos.trBase[0]  rotation degrees (TR_STATIONARY)
  eFlags       EF_BILLBOARD | EF_FLIPBOOK | EF_IMPOSTER
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "engine_sprite_map.h"
#include "../game/bg_public.h"
#include "server.h"
#include "sv_engine_sprites.h"

static cvar_t *sv_engineSprites;
static cvar_t *sv_engineSpritesSpawn;
static int sv_numEngineSpriteShaders;
static engineSpriteMapList_t sv_mapSpriteList;
static int sv_mapSpriteSpawnCount;

static void SV_EngineSpriteFillEntityState( const engineSpriteMapDef_t *def, int shaderIndex,
	entityState_t *state )
{
	int radiusQuarters;

	if ( !def || !state || shaderIndex <= 0 ) {
		return;
	}

	Com_Memset( state, 0, sizeof( *state ) );
	state->eType = ET_GENERAL;
	state->pos.trType = TR_STATIONARY;
	VectorCopy( def->origin, state->pos.trBase );
	state->modelindex = shaderIndex;

	if ( def->type == ENGINE_SPRITE_FLIPBOOK ) {
		state->eFlags |= EF_FLIPBOOK;
		state->modelindex2 = ( def->cols & 0xFF ) | ( ( def->rows & 0xFF ) << 8 );
		state->eventParm = (int)def->fps;
		if ( state->eventParm <= 0 ) {
			state->eventParm = 8;
		}
	} else if ( def->type == ENGINE_SPRITE_IMPOSTER ) {
		state->eFlags |= EF_IMPOSTER;
	} else {
		state->eFlags |= EF_BILLBOARD;
	}

	radiusQuarters = (int)( def->radius * 0.25f );
	if ( radiusQuarters <= 0 ) {
		radiusQuarters = 8;
	}
	state->generic1 = radiusQuarters;

	state->apos.trType = TR_STATIONARY;
	state->apos.trBase[0] = def->rotation;
}

static int SV_EngineSprite_LinkFromDef( const engineSpriteMapDef_t *def, int shaderIndex ) {
	sharedEntity_t *ent;
	int entNum;

	if ( !def || shaderIndex <= 0 ) {
		return -1;
	}
	if ( !sv.gentities || sv.gentitySize <= 0 ) {
		return -1;
	}
	if ( sv.num_entities >= MAX_GENTITIES - 2 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: entity limit hit; engine sprite spawn failed\n" );
		return -1;
	}

	entNum = SV_AllocEngineEntityNum();
	if ( entNum < 0 ) {
		return -1;
	}
	ent = SV_GentityNum( entNum );
	Com_Memset( ent, 0, sv.gentitySize );

	ent->s.number = entNum;
	SV_EngineSpriteFillEntityState( def, shaderIndex, &ent->s );

	ent->r.linked = qfalse;
	ent->r.svFlags = 0;
	ent->r.mins[0] = ent->r.mins[1] = ent->r.mins[2] = -4.0f;
	ent->r.maxs[0] = ent->r.maxs[1] = ent->r.maxs[2] = 4.0f;
	VectorCopy( def->origin, ent->r.currentOrigin );
	VectorClear( ent->r.currentAngles );

	SV_LinkEntity( ent );
	return entNum;
}

int SV_EngineSprite_SpawnFromDef( const engineSpriteMapDef_t *def ) {
	int shaderIndex;

	if ( !sv_engineSprites || !sv_engineSprites->integer ) {
		return -1;
	}
	if ( !def || !def->shader[0] ) {
		return -1;
	}

	shaderIndex = SV_EngineSpriteShaderIndex( def->shader );
	if ( shaderIndex <= 0 ) {
		return -1;
	}

	return SV_EngineSprite_LinkFromDef( def, shaderIndex );
}

static void SV_SpriteSpawn_f( void ) {
	engineSpriteMapDef_t def;
	const char *typeArg;
	const char *shaderArg;
	int entNum;

	if ( !sv_engineSprites || !sv_engineSprites->integer ) {
		Com_Printf( "sv_engineSprites is disabled\n" );
		return;
	}

	Com_Memset( &def, 0, sizeof( def ) );
	def.type = ENGINE_SPRITE_BILLBOARD;
	def.radius = 48.0f;
	def.cols = 1;
	def.rows = 1;
	def.fps = 8.0f;

	typeArg = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "billboard";
	shaderArg = Cmd_Argc() > 2 ? Cmd_Argv( 2 ) : "sprites/demo_billboard";
	Q_strncpyz( def.shader, shaderArg, sizeof( def.shader ) );

	if ( !Q_stricmp( typeArg, "flipbook" ) ) {
		def.type = ENGINE_SPRITE_FLIPBOOK;
		def.cols = Cmd_Argc() > 3 ? atoi( Cmd_Argv( 3 ) ) : 2;
		def.rows = Cmd_Argc() > 4 ? atoi( Cmd_Argv( 4 ) ) : 2;
		def.fps = Cmd_Argc() > 5 ? (float)atof( Cmd_Argv( 5 ) ) : 8.0f;
	} else if ( !Q_stricmp( typeArg, "imposter" ) ) {
		def.type = ENGINE_SPRITE_IMPOSTER;
	}

	if ( Cmd_Argc() > 6 ) {
		def.origin[0] = (float)atof( Cmd_Argv( 6 ) );
		def.origin[1] = Cmd_Argc() > 7 ? (float)atof( Cmd_Argv( 7 ) ) : 0.0f;
		def.origin[2] = Cmd_Argc() > 8 ? (float)atof( Cmd_Argv( 8 ) ) : 64.0f;
	} else {
		VectorClear( def.origin );
		def.origin[2] = 64.0f;
	}

	if ( Cmd_Argc() > 9 ) {
		def.radius = (float)atof( Cmd_Argv( 9 ) );
	}
	if ( Cmd_Argc() > 10 ) {
		def.rotation = (float)atof( Cmd_Argv( 10 ) );
	}

	entNum = SV_EngineSprite_SpawnFromDef( &def );
	if ( entNum < 0 ) {
		Com_Printf( "sv_sprite_spawn failed for '%s'\n", def.shader );
		return;
	}

	Com_Printf( "sv_sprite_spawn: ent %d %s '%s' at (%.0f %.0f %.0f)\n",
		entNum, typeArg, def.shader, def.origin[0], def.origin[1], def.origin[2] );
}

void SV_EngineSprites_Init( void ) {
	sv_engineSprites = Cvar_Get( "sv_engineSprites", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( sv_engineSprites,
		"Register engine sprite shader paths from map entities into configstrings "
		"(CS_ENGINE_SPRITE_SHADERS) for networked EF_BILLBOARD/FLIPBOOK/IMPOSTER ents." );
	sv_engineSpritesSpawn = Cvar_Get( "sv_engineSpritesSpawn", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( sv_engineSpritesSpawn,
		"Spawn misc_billboard / misc_flipbook / misc_imposter map entities as ET_GENERAL "
		"snapshot ents (disables client BSP map parse via CS_ENGINE_SPRITE_META)." );
	if ( sv_engineSprites && sv_engineSprites->integer ) {
		Com_Printf( "[engine][sprites] sv_engineSprites=1 (map shader configstrings)\n" );
	}
	if ( sv_engineSpritesSpawn && sv_engineSpritesSpawn->integer ) {
		Com_Printf( "[engine][sprites] sv_engineSpritesSpawn=1 (network map sprite ents)\n" );
	}
	Cmd_AddCommand( "sv_sprite_spawn", SV_SpriteSpawn_f );
}

void SV_EngineSprites_Clear( void ) {
	int i;

	for ( i = 0; i < sv_numEngineSpriteShaders; i++ ) {
		SV_SetConfigstring( CS_ENGINE_SPRITE_SHADERS + i, "" );
	}
	SV_SetConfigstring( CS_ENGINE_SPRITE_META, "" );
	sv_numEngineSpriteShaders = 0;
	sv_mapSpriteSpawnCount = 0;
	EngineSpriteMap_Clear( &sv_mapSpriteList );
}

int SV_EngineSpriteShaderIndex( const char *shaderName ) {
	int i;

	if ( !shaderName || !shaderName[0] ) {
		return 0;
	}
	if ( !sv_engineSprites || !sv_engineSprites->integer ) {
		return 0;
	}

	for ( i = 0; i < sv_numEngineSpriteShaders; i++ ) {
		const char *existing = sv.configstrings[CS_ENGINE_SPRITE_SHADERS + i];
		if ( existing && existing[0] && !Q_stricmp( existing, shaderName ) ) {
			return i + 1;
		}
	}

	if ( sv_numEngineSpriteShaders >= MAX_ENGINE_SPRITE_SHADERS ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: MAX_ENGINE_SPRITE_SHADERS hit; skipping '%s'\n", shaderName );
		return 0;
	}

	SV_SetConfigstring( CS_ENGINE_SPRITE_SHADERS + sv_numEngineSpriteShaders, shaderName );
	sv_numEngineSpriteShaders++;
	return sv_numEngineSpriteShaders;
}

void SV_EngineSprites_LoadMap( const char *entityString ) {
	int i;

	SV_EngineSprites_Clear();
	if ( !entityString || !entityString[0] ) {
		return;
	}
	if ( !sv_engineSprites || !sv_engineSprites->integer ) {
		return;
	}

	EngineSpriteMap_Parse( entityString, &sv_mapSpriteList );

	for ( i = 0; i < sv_mapSpriteList.count; i++ ) {
		SV_EngineSpriteShaderIndex( sv_mapSpriteList.defs[i].shader );
	}

	if ( sv_numEngineSpriteShaders > 0 ) {
		Com_Printf( "[engine][sprites] server registered %d sprite shader(s) from map (%d props)\n",
			sv_numEngineSpriteShaders, sv_mapSpriteList.count );
	}
}

void SV_EngineSprites_SpawnMapEntities( void ) {
	int i;

	sv_mapSpriteSpawnCount = 0;
	SV_SetConfigstring( CS_ENGINE_SPRITE_META, "" );

	if ( !sv_engineSprites || !sv_engineSprites->integer ) {
		return;
	}
	if ( !sv_engineSpritesSpawn || !sv_engineSpritesSpawn->integer ) {
		return;
	}
	if ( sv_mapSpriteList.count <= 0 ) {
		return;
	}
	if ( !sv.gentities || sv.gentitySize <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: sv_engineSpritesSpawn: game entities not ready\n" );
		return;
	}

	for ( i = 0; i < sv_mapSpriteList.count; i++ ) {
		const engineSpriteMapDef_t *def = &sv_mapSpriteList.defs[i];
		int entNum;

		entNum = SV_EngineSprite_SpawnFromDef( def );
		if ( entNum >= 0 ) {
			sv_mapSpriteSpawnCount++;
		}
	}

	if ( sv_mapSpriteSpawnCount > 0 ) {
		SV_SetConfigstring( CS_ENGINE_SPRITE_META, va( "%d", sv_mapSpriteSpawnCount ) );
		Com_Printf( "[engine][sprites] spawned %d map sprite ent(s) in snapshots\n",
			sv_mapSpriteSpawnCount );
	}
}
