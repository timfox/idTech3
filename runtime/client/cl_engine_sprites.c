/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Networked engine sprites (EF_BILLBOARD / EF_FLIPBOOK / EF_IMPOSTER):
  modelindex   1-based CS_ENGINE_SPRITE_SHADERS index (shader path; preferred)
  generic1     legacy local shader qhandle (single-player / dev only)
  modelindex2  flipbook: cols (low 8) | rows (high 8)
  generic1     radius / 4 (0 -> default 32) when modelindex shader path is used
  eventParm    flipbook fps (default 8)
  apos.trBase[0]  rotation degrees when apos.trType == TR_STATIONARY
  pos          world trajectory
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/engine_sprite_map.h"
#include "../renderers/common/tr_public.h"
#include "../game/bg_public.h"
#include "client.h"
#include "cl_engine_sprites.h"

extern refexport_t re;

static cvar_t *cl_engineSprites;

static const char *CL_EngineSpriteConfigString( int index ) {
	int ofs;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return "";
	}
	ofs = cl.gameState.stringOffsets[index];
	if ( !ofs ) {
		return "";
	}
	return cl.gameState.stringData + ofs;
}

static qboolean CL_EngineSpriteUsesCsShader( const entityState_t *es ) {
	const char *path;

	if ( !es || es->modelindex <= 0 || es->modelindex > MAX_ENGINE_SPRITE_SHADERS ) {
		return qfalse;
	}
	path = CL_EngineSpriteConfigString( CS_ENGINE_SPRITE_SHADERS + es->modelindex - 1 );
	return ( path && path[0] ) ? qtrue : qfalse;
}

static qhandle_t CL_EngineSpriteShaderFromEntity( const entityState_t *es ) {
	qhandle_t shader = 0;
	const char *path;

	if ( es->modelindex > 0 && es->modelindex <= MAX_ENGINE_SPRITE_SHADERS ) {
		path = CL_EngineSpriteConfigString( CS_ENGINE_SPRITE_SHADERS + es->modelindex - 1 );
		if ( path && path[0] && re.RegisterShader ) {
			shader = re.RegisterShader( path );
		}
	}

	if ( !shader && es->generic1 ) {
		shader = (qhandle_t)es->generic1;
	}

	return shader;
}

static void CL_EngineSprites_SyncMapParse( void ) {
	const char *meta;
	int spawned;

	if ( !cl_engineSprites || !cl_engineSprites->integer ) {
		return;
	}

	meta = CL_EngineSpriteConfigString( CS_ENGINE_SPRITE_META );
	spawned = ( meta && meta[0] ) ? atoi( meta ) : 0;

	if ( cls.state >= CA_CONNECTED && spawned > 0 ) {
		Cvar_Set( "r_spritePropsMapParse", "0" );
	} else {
		Cvar_Set( "r_spritePropsMapParse", "1" );
	}
}

void CL_EngineSprites_Init( void ) {
	cl_engineSprites = Cvar_Get( "cl_engineSprites", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_engineSprites,
		"Draw networked EF_BILLBOARD / EF_FLIPBOOK / EF_IMPOSTER entities from snapshots. "
		"Shader path via modelindex -> CS_ENGINE_SPRITE_SHADERS (see sv_engineSprites)." );
	Cmd_AddCommand( "sprite_spawn", CL_SpriteSpawn_f );
	if ( cl_engineSprites && cl_engineSprites->integer ) {
		Com_Printf( "[engine][sprites] cl_engineSprites=1 (snapshot billboard/flipbook/imposter bridge)\n" );
	}
	CL_EngineSprites_SyncMapParse();
}

static void CL_EvalTrajectory( const trajectory_t *tr, int atTime, vec3_t out ) {
	float deltaTime;

	if ( !tr || !out ) {
		return;
	}

	switch ( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorCopy( tr->trBase, out );
		break;
	case TR_LINEAR:
	case TR_LINEAR_STOP:
		deltaTime = ( atTime - tr->trTime ) * 0.001f;
		if ( tr->trType == TR_LINEAR_STOP && tr->trDuration > 0 &&
			atTime > tr->trTime + tr->trDuration ) {
			deltaTime = (float)tr->trDuration * 0.001f;
		}
		VectorMA( tr->trBase, deltaTime, tr->trDelta, out );
		break;
	default:
		VectorCopy( tr->trBase, out );
		break;
	}
}

static void CL_AddSnapshotSprite( const entityState_t *es, int serverTime ) {
	engineSpriteDesc_t desc;
	qhandle_t shader;
	qboolean csShader;

	if ( !es || ( !re.AddEngineSpriteToSceneAtTime && !re.AddEngineSpriteToScene ) ) {
		return;
	}

	csShader = CL_EngineSpriteUsesCsShader( es );
	shader = CL_EngineSpriteShaderFromEntity( es );
	if ( !shader ) {
		return;
	}

	Com_Memset( &desc, 0, sizeof( desc ) );
	CL_EvalTrajectory( &es->pos, serverTime, desc.origin );
	desc.shader = shader;

	if ( csShader ) {
		desc.radius = ( es->generic1 > 0 ) ? (float)es->generic1 * 4.0f : 32.0f;
		if ( es->apos.trType == TR_STATIONARY ) {
			desc.rotation = es->apos.trBase[0];
		}
	} else {
		desc.radius = ( es->angles[0] > 0.0f ) ? es->angles[0] : 32.0f;
		desc.rotation = es->angles[1];
	}

	if ( es->eFlags & EF_IMPOSTER ) {
		desc.type = ENGINE_SPRITE_IMPOSTER;
	} else if ( es->eFlags & EF_FLIPBOOK ) {
		desc.type = ENGINE_SPRITE_FLIPBOOK;
		if ( csShader ) {
			desc.cols = es->modelindex2 & 0xFF;
			desc.rows = ( es->modelindex2 >> 8 ) & 0xFF;
		} else {
			desc.cols = es->modelindex > 0 ? es->modelindex : 1;
			desc.rows = es->modelindex2 > 0 ? es->modelindex2 : 1;
		}
		if ( desc.cols < 1 ) {
			desc.cols = 1;
		}
		if ( desc.rows < 1 ) {
			desc.rows = 1;
		}
		desc.fps = (float)( es->eventParm > 0 ? es->eventParm : 8 );
	} else {
		desc.type = ENGINE_SPRITE_BILLBOARD;
	}

	if ( re.AddEngineSpriteToSceneAtTime ) {
		re.AddEngineSpriteToSceneAtTime( &desc, serverTime );
	} else {
		re.AddEngineSpriteToScene( &desc );
	}
}

void CL_EngineSprites_AddFromSnapshot( void ) {
	int i;

	CL_EngineSprites_SyncMapParse();

	if ( !cl_engineSprites || !cl_engineSprites->integer ) {
		return;
	}
	if ( !cls.cgameStarted || cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	if ( !re.AddEngineSpriteToScene ) {
		return;
	}

	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		const entityState_t *es = &cl.parseEntities[
			( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];
		int flags;

		if ( !es || es->eType == ET_INVISIBLE || es->eType == ET_EVENTS ) {
			continue;
		}

		flags = es->eFlags & ( EF_BILLBOARD | EF_FLIPBOOK | EF_IMPOSTER );
		if ( !flags ) {
			continue;
		}

		CL_AddSnapshotSprite( es, cl.snap.serverTime );
	}
}

void CL_EngineSprite_AddLocal( const engineSpriteDesc_t *desc ) {
	CL_EngineSprite_AddLocalAtTime( desc, cls.realtime );
}

void CL_EngineSprite_AddLocalAtTime( const engineSpriteDesc_t *desc, int timeMs ) {
	if ( !desc || !desc->shader ) {
		return;
	}
	if ( re.AddEngineSpriteToSceneAtTime ) {
		re.AddEngineSpriteToSceneAtTime( desc, timeMs );
	} else if ( re.AddEngineSpriteToScene ) {
		re.AddEngineSpriteToScene( desc );
	}
}

void CL_SpriteSpawn_f( void ) {
	engineSpriteDesc_t desc;
	const char *typeArg;
	const char *shaderArg;
	vec3_t org;

	if ( !re.AddEngineSpriteToScene || !re.RegisterShader ) {
		Com_Printf( "Renderer not ready for sprite_spawn\n" );
		return;
	}

	typeArg = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "billboard";
	shaderArg = Cmd_Argc() > 2 ? Cmd_Argv( 2 ) : "sprites/demo_billboard";

	Com_Memset( &desc, 0, sizeof( desc ) );
	if ( !Q_stricmp( typeArg, "flipbook" ) ) {
		desc.type = ENGINE_SPRITE_FLIPBOOK;
		desc.cols = Cmd_Argc() > 3 ? atoi( Cmd_Argv( 3 ) ) : 2;
		desc.rows = Cmd_Argc() > 4 ? atoi( Cmd_Argv( 4 ) ) : 2;
		desc.fps = Cmd_Argc() > 5 ? (float)atof( Cmd_Argv( 5 ) ) : 8.0f;
	} else if ( !Q_stricmp( typeArg, "imposter" ) ) {
		desc.type = ENGINE_SPRITE_IMPOSTER;
	} else {
		desc.type = ENGINE_SPRITE_BILLBOARD;
	}

	desc.shader = re.RegisterShader( shaderArg );
	if ( !desc.shader ) {
		Com_Printf( "sprite_spawn: could not register shader '%s'\n", shaderArg );
		return;
	}

	desc.radius = 48.0f;
	if ( cls.cgameStarted && !( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) ) {
		VectorCopy( cl.snap.ps.origin, org );
		org[2] += 48.0f;
	} else {
		VectorClear( org );
		org[2] = 64.0f;
	}
	VectorCopy( org, desc.origin );

	CL_EngineSprite_AddLocal( &desc );
	Com_Printf( "sprite_spawn: %s '%s' at (%.0f %.0f %.0f)\n",
		typeArg, shaderArg, desc.origin[0], desc.origin[1], desc.origin[2] );
}
