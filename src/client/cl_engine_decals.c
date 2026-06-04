/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderers/common/tr_public.h"
#include "../game/bg_public.h"
#include "client.h"
#include "cl_engine_decals.h"

extern refexport_t re;

static cvar_t *cl_engineDecals;

static void CL_DecalSpawn_f( void );

static const char *CL_EngineDecalConfigString( int index ) {
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

static void CL_EngineDecals_SyncMapParse( void ) {
	const char *meta;
	int spawned;

	if ( !cl_engineDecals || !cl_engineDecals->integer ) {
		return;
	}

	meta = CL_EngineDecalConfigString( CS_ENGINE_DECAL_META );
	spawned = ( meta && meta[0] ) ? atoi( meta ) : 0;

	if ( cls.state >= CA_CONNECTED && spawned > 0 ) {
		Cvar_Set( "r_decalPropsMapParse", "0" );
	} else {
		Cvar_Set( "r_decalPropsMapParse", "1" );
	}
}

void CL_EngineDecals_Init( void ) {
	cl_engineDecals = Cvar_Get( "cl_engineDecals", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_engineDecals,
		"Draw networked EF_DECAL entities from snapshots (CS_ENGINE_DECAL_SHADERS)." );
	Cmd_AddCommand( "decal_spawn", CL_DecalSpawn_f );
	if ( cl_engineDecals && cl_engineDecals->integer ) {
		Com_Printf( "[engine][decals] cl_engineDecals=1\n" );
	}
	CL_EngineDecals_SyncMapParse();
}

static qhandle_t CL_EngineDecalShaderFromEntity( const entityState_t *es ) {
	const char *path;

	if ( !es || es->modelindex <= 0 || es->modelindex > MAX_ENGINE_DECAL_SHADERS ) {
		return 0;
	}
	path = CL_EngineDecalConfigString( CS_ENGINE_DECAL_SHADERS + es->modelindex - 1 );
	if ( path && path[0] && re.RegisterShader ) {
		return re.RegisterShader( path );
	}
	return 0;
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

static void CL_FillDecalDescFromEntity( const entityState_t *es, int serverTime, engineDecalDesc_t *desc ) {
	qhandle_t shader;

	if ( !es || !desc ) {
		return;
	}

	shader = CL_EngineDecalShaderFromEntity( es );
	if ( !shader ) {
		return;
	}

	Com_Memset( desc, 0, sizeof( *desc ) );
	CL_EvalTrajectory( &es->pos, serverTime, desc->origin );
	desc->shader = shader;
	desc->radius = ( es->generic1 > 0 ) ? (float)es->generic1 * 4.0f : 32.0f;
	if ( es->apos.trType == TR_STATIONARY ) {
		desc->pitch = es->apos.trBase[0];
		desc->yaw = es->apos.trBase[1];
	}
	if ( es->eventParm > 0 ) {
		desc->fadeSec = (float)es->eventParm * 0.001f;
	}
}

static void CL_AddSnapshotDecal( const entityState_t *es, int serverTime ) {
	engineDecalDesc_t desc;

	if ( !es || !re.AddEngineDecalToScene ) {
		return;
	}

	CL_FillDecalDescFromEntity( es, serverTime, &desc );
	if ( !desc.shader ) {
		return;
	}

	re.AddEngineDecalToScene( &desc );
}

void CL_EngineDecals_AddFromSnapshot( void ) {
	int i;

	if ( !cl_engineDecals || !cl_engineDecals->integer ) {
		return;
	}
	if ( !cls.cgameStarted || cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	if ( !re.AddEngineDecalToScene ) {
		return;
	}

	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		const entityState_t *es = &cl.parseEntities[
			( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];

		if ( !es || es->eType == ET_INVISIBLE || es->eType == ET_EVENTS ) {
			continue;
		}
		if ( !( es->eFlags & EF_DECAL ) ) {
			continue;
		}
		CL_AddSnapshotDecal( es, cl.snap.serverTime );
	}
}

void CL_EngineDecal_AddLocalAtTime( const engineDecalDesc_t *desc, int timeMs ) {
	(void)timeMs;
	if ( !desc || !re.AddEngineDecalToScene ) {
		return;
	}
	re.AddEngineDecalToScene( desc );
}

void CL_EngineDecal_AddLocal( const engineDecalDesc_t *desc ) {
	CL_EngineDecal_AddLocalAtTime( desc, cls.realtime );
}

void CL_DecalSpawn_f( void ) {
	engineDecalDesc_t desc;
	const char *shaderArg;

	if ( Cmd_Argc() < 5 ) {
		Com_Printf( "usage: decal_spawn <shader> <x> <y> <z> [radius] [pitch] [yaw]\n" );
		return;
	}

	if ( !re.RegisterShader ) {
		Com_Printf( "renderer not ready\n" );
		return;
	}

	Com_Memset( &desc, 0, sizeof( desc ) );
	shaderArg = Cmd_Argv( 1 );
	desc.shader = re.RegisterShader( shaderArg );
	if ( !desc.shader ) {
		Com_Printf( "shader not found: %s\n", shaderArg );
		return;
	}
	desc.origin[0] = (float)atof( Cmd_Argv( 2 ) );
	desc.origin[1] = (float)atof( Cmd_Argv( 3 ) );
	desc.origin[2] = (float)atof( Cmd_Argv( 4 ) );
	desc.radius = ( Cmd_Argc() > 5 ) ? (float)atof( Cmd_Argv( 5 ) ) : 32.0f;
	desc.pitch = ( Cmd_Argc() > 6 ) ? (float)atof( Cmd_Argv( 6 ) ) : 0.0f;
	desc.yaw = ( Cmd_Argc() > 7 ) ? (float)atof( Cmd_Argv( 7 ) ) : 0.0f;

	CL_EngineDecal_AddLocal( &desc );
}
