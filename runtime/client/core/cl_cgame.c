/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_cgame.c  -- client system interaction with client game

#include "client.h"
#include "cl_cvars.h"
#include "../../physics/phys_bullet.h"
#include "../../qcommon/script_emit.h"

#include "../../botlib/botlib.h"
#include "../../game/g_entity_bridge.h"
#include "cl_compat_math.h"
#include "cl_engine_sprites.h"
#include "cl_engine_decals.h"
#include "cl_openworld.h"
#include "../../game/bg_public.h"
#include "../../qcommon/cm_public.h"

void CM_Stream_Merge_ClearAll( void );
int CM_Stream_MergedCount( void );

extern	botlib_export_t	*botlib_export;

//extern qboolean loadCamera(const char *name);
//extern void startCamera(int time);
//extern qboolean getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

/*
====================
CL_GetGameState
====================
*/
static void CL_GetGameState( gameState_t *gs ) {
	*gs = cl.gameState;
}

typedef struct {
	int  stringOffsets[1024];
	char stringData[16000];
	int  dataCount;
} legacyGameState_t;

#define RETAIL_QVM_MAX_GENTITIES	1024

/* Retail cgame.qvm refEntity_t ends at shaderTime (no radius/rotation fields). */
typedef struct {
	refEntityType_t	reType;
	int				renderfx;
	qhandle_t		hModel;
	vec3_t			lightingOrigin;
	float			shadowPlane;
	vec3_t			axis[3];
	qboolean		nonNormalizedAxes;
	float			origin[3];
	int				frame;
	float			oldorigin[3];
	int				oldframe;
	float			backlerp;
	int				skinNum;
	qhandle_t		customSkin;
	qhandle_t		customShader;
	color4ub_t		shader;
	float			shaderTexCoord[2];
	floatint_t		shaderTime;
} retailRefEntity_t;

/*
 * Retail cgame.qvm keeps enum-backed qboolean, so trace_t is wider there than
 * in the native client build. CM trace syscalls must marshal into that layout.
 */
typedef struct {
	int			allsolid;
	int			startsolid;
	float		fraction;
	vec3_t		endpos;
	cplane_t	plane;
	int			surfaceFlags;
	int			contents;
	int			entityNum;
} legacy_trace_t;

STATIC_ASSERT( sizeof( legacy_trace_t ) == sizeof( trace_t ) + 4, "legacy_trace_t must match retail cgame trace layout" );

static int CL_GameEntityNumToEngine( int entityNum ) {
	if ( !cgvm || cgvm->dllHandle ) {
		return entityNum;
	}

	if ( entityNum == RETAIL_QVM_MAX_GENTITIES - 1 ) {
		return ENTITYNUM_NONE;
	}

	if ( entityNum == RETAIL_QVM_MAX_GENTITIES - 2 ) {
		return ENTITYNUM_WORLD;
	}

	return entityNum;
}

static int CL_EngineEntityNumToGame( int entityNum ) {
	if ( !cgvm || cgvm->dllHandle ) {
		return entityNum;
	}

	if ( entityNum == ENTITYNUM_NONE ) {
		return RETAIL_QVM_MAX_GENTITIES - 1;
	}

	if ( entityNum == ENTITYNUM_WORLD ) {
		return RETAIL_QVM_MAX_GENTITIES - 2;
	}

	if ( entityNum < 0 || entityNum >= RETAIL_QVM_MAX_GENTITIES ) {
		return RETAIL_QVM_MAX_GENTITIES - 1;
	}

	return entityNum;
}

static void CL_AddRetailRefEntityToScene( const retailRefEntity_t *src, qboolean intShaderTime ) {
	refEntity_t ent;

	Com_Memset( &ent, 0, sizeof( ent ) );
	Com_Memcpy( &ent, src, sizeof( *src ) );
	re.AddRefEntityToScene( &ent, intShaderTime );
}

static void CL_FillLegacyTrace( legacy_trace_t *out, const trace_t *in ) {
	out->allsolid = in->allsolid ? 1 : 0;
	out->startsolid = in->startsolid ? 1 : 0;
	out->fraction = in->fraction;
	VectorCopy( in->endpos, out->endpos );
	out->plane = in->plane;
	out->surfaceFlags = in->surfaceFlags;
	out->contents = in->contents;
	out->entityNum = CL_EngineEntityNumToGame( in->entityNum );
}

/*
 * Retail cgame.qvm snapshot_t layout (cg_public.h). Must match the QVM struct
 * exactly — do not grow without a new CGAME_IMPORT_API_VERSION.
 */
typedef struct {
	int				snapFlags;
	int				ping;
	int				serverTime;
	byte			areamask[MAX_MAP_AREA_BYTES];
	playerState_t	ps;
	int				numEntities;
	entityState_t	entities[MAX_ENTITIES_IN_SNAPSHOT];
	int				numServerCommands;
	int				serverCommandSequence;
} legacySnapshot_t;

static void CL_GetLegacyGameState( legacyGameState_t *gs ) {
	int i;

	Com_Memset( gs, 0, sizeof( *gs ) );
	gs->dataCount = 1;

	for ( i = 0; i < (int)ARRAY_LEN( gs->stringOffsets ) && i < MAX_CONFIGSTRINGS; i++ ) {
		const int offset = cl.gameState.stringOffsets[i];
		const char *s;
		size_t len;

		if ( !offset ) {
			continue;
		}

		s = cl.gameState.stringData + offset;
		len = strlen( s );
		if ( gs->dataCount + (int)len + 1 > (int)sizeof( gs->stringData ) ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: truncating legacy cgame gamestate at configstring %d\n", i );
			break;
		}

		gs->stringOffsets[i] = gs->dataCount;
		Com_Memcpy( gs->stringData + gs->dataCount, s, len + 1 );
		gs->dataCount += (int)len + 1;
	}
}


/*
====================
CL_GetGlconfig
====================
*/
static void CL_GetGlconfig( glconfig_t *glconfig ) {
	*glconfig = cls.glconfig;
}


/*
====================
CL_GetUserCmd
====================
*/
static qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( cl.cmdNumber - cmdNumber < 0 ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: cmdNumber (%i) > cl.cmdNumber (%i)", cmdNumber, cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( cl.cmdNumber - cmdNumber >= CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}


/*
====================
CL_GetCurrentCmdNumber
====================
*/
static int CL_GetCurrentCmdNumber( void ) {
	return cl.cmdNumber;
}


/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
static void CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = cl.snap.messageNum;
	*serverTime = cl.snap.serverTime;
}


/*
====================
CL_FillSnapshot

Shared snapshot assembly for native and QVM cgame paths.
====================
*/
static qboolean CL_FillSnapshot( int snapshotNumber, int *snapFlags, int *serverCommandSequence,
	int *ping, int *serverTime, byte *areamask, playerState_t *ps, int *numEntities,
	entityState_t *entities, int maxEntities ) {
	clSnapshot_t	*clSnap;
	int				i, count;

	if ( cl.snap.messageNum - snapshotNumber < 0 ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber (%i) > cl.snapshot.messageNum (%i)", snapshotNumber, cl.snap.messageNum );
	}

	if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	*snapFlags = clSnap->snapFlags;
	*serverCommandSequence = clSnap->serverCommandNum;
	*ping = clSnap->ping;
	*serverTime = clSnap->serverTime;
	Com_Memcpy( areamask, clSnap->areamask, sizeof( clSnap->areamask ) );
	*ps = clSnap->ps;

	count = clSnap->numEntities;
	if ( count > maxEntities ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, maxEntities );
		count = maxEntities;
	}
	*numEntities = count;
	for ( i = 0; i < count; i++ ) {
		entities[i] =
			cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES - 1) ];
	}

	return qtrue;
}

static int CL_SanitizeLegacyEntityNum( int entityNum ) {
	if ( !cgvm || cgvm->dllHandle ) {
		return entityNum;
	}

	if ( entityNum == ENTITYNUM_NONE ) {
		return RETAIL_QVM_MAX_GENTITIES - 1;
	}

	if ( entityNum == ENTITYNUM_WORLD ) {
		return RETAIL_QVM_MAX_GENTITIES - 2;
	}

	if ( entityNum < 0 || entityNum >= RETAIL_QVM_MAX_GENTITIES ) {
		return RETAIL_QVM_MAX_GENTITIES - 1;
	}

	return entityNum;
}

static void CL_SanitizeLegacySnapshotEntities( legacySnapshot_t *snapshot ) {
	int i;

	if ( !snapshot ) {
		return;
	}

	snapshot->ps.groundEntityNum = CL_SanitizeLegacyEntityNum( snapshot->ps.groundEntityNum );

	for ( i = 0; i < snapshot->numEntities; i++ ) {
		entityState_t *es = &snapshot->entities[i];

		es->groundEntityNum = CL_SanitizeLegacyEntityNum( es->groundEntityNum );
		es->otherEntityNum = CL_SanitizeLegacyEntityNum( es->otherEntityNum );
		es->otherEntityNum2 = CL_SanitizeLegacyEntityNum( es->otherEntityNum2 );

		if ( es->eFlags & ( EF_BILLBOARD | EF_FLIPBOOK | EF_IMPOSTER | EF_DECAL ) ) {
			es->eFlags &= ~( EF_BILLBOARD | EF_FLIPBOOK | EF_IMPOSTER | EF_DECAL );
			es->modelindex = 0;
			es->modelindex2 = 0;
			es->generic1 = 0;
		}
	}
}

static qboolean CL_GetLegacySnapshot( int snapshotNumber, legacySnapshot_t *snapshot ) {
	qboolean ok;

	ok = CL_FillSnapshot( snapshotNumber, &snapshot->snapFlags, &snapshot->serverCommandSequence,
		&snapshot->ping, &snapshot->serverTime, snapshot->areamask, &snapshot->ps,
		&snapshot->numEntities, snapshot->entities, MAX_ENTITIES_IN_SNAPSHOT );
	if ( ok ) {
		CL_SanitizeLegacySnapshotEntities( snapshot );
	}
	return ok;
}

/*
====================
CL_GetSnapshot
====================
*/
static qboolean CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	return CL_FillSnapshot( snapshotNumber, &snapshot->snapFlags, &snapshot->serverCommandSequence,
		&snapshot->ping, &snapshot->serverTime, snapshot->areamask, &snapshot->ps,
		&snapshot->numEntities, snapshot->entities, MAX_ENTITIES_IN_SNAPSHOT );
}


/*
=====================
CL_SetUserCmdValue
=====================
*/
static void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale ) {
	cl.cgameUserCmdValue = userCmdValue;
	cl.cgameSensitivity = sensitivityScale;
}


/*
=====================
CL_AddCgameCommand
=====================
*/
static void CL_AddCgameCommand( const char *cmdName ) {
	Cmd_AddCommand( cmdName, NULL );
}


/*
=====================
CL_ConfigstringModified
=====================
*/
static void CL_ConfigstringModified( void ) {
	const char	*old, *s;
	int			i, index;
	const char	*dup;
	gameState_t	oldGs;
	int			len;

	index = atoi( Cmd_Argv(1) );
	if ( (unsigned) index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "%s: bad configstring index %i", __func__, index );
	}
	// get everything after "cs <num>"
	s = Cmd_ArgsFrom(2);

	old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		// unchanged
	}

	// build the new gameState_t
	oldGs = cl.gameState;

	Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	// leave the first 0 for uninitialized strings
	cl.gameState.dataCount = 1;

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;		// leave with the default empty string
		}

		len = strlen( dup );

		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Error( ERR_DROP, "%s: MAX_GAMESTATE_CHARS exceeded", __func__ );
		}

		// append it to the gameState string buffer
		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars
		CL_SystemInfoChanged( qfalse );
	}

	if ( index == CS_ENGINE_OPENWORLD_SECTORS && !CL_StockBaseq3Mode() ) {
		CL_OpenWorld_OnConfigstring( s );
	}
}


/*
===================
CL_GetServerCommand

Set up argc/argv for the given command
===================
*/
static qboolean CL_GetServerCommand( int serverCommandNumber ) {
	const char *s;
	const char *cmd;
	static char bigConfigString[BIG_INFO_STRING];
	int argc, index;

	// if we have irretrievably lost a reliable command, drop the connection
	if ( clc.serverCommandSequence - serverCommandNumber >= MAX_RELIABLE_COMMANDS ) {
		// when a demo record was started after the client got a whole bunch of
		// reliable commands then the client never got those first reliable commands
		if ( clc.demoplaying ) {
			Cmd_Clear();
			return qfalse;
		}
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( clc.serverCommandSequence - serverCommandNumber < 0 ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	index = serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 );
	s = clc.serverCommands[ index ];
	clc.lastExecutedServerCommand = serverCommandNumber;

	Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );

	if ( clc.serverCommandsIgnore[ index ] ) {
		Cmd_Clear();
		return qfalse;
	}

rescan:
	Cmd_TokenizeString( s );
	cmd = Cmd_Argv(0);
	argc = Cmd_Argc();

	if ( !strcmp( cmd, "disconnect" ) ) {
		// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=552
		// allow server to indicate why they were disconnected
		if ( argc >= 2 )
			Com_Error( ERR_SERVERDISCONNECT, "Server disconnected - %s", Cmd_Argv( 1 ) );
		else
			Com_Error( ERR_SERVERDISCONNECT, "Server disconnected" );
	}

	if ( !strcmp( cmd, "bcs0" ) ) {
		Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs1" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs2" ) ) {
		s = Cmd_Argv(2);
		if( strlen(bigConfigString) + strlen(s) + 1 >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		strcat( bigConfigString, "\"" );
		s = bigConfigString;
		goto rescan;
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		return qtrue;
	}

	if ( !strcmp( cmd, "map_restart" ) ) {
		// clear notify lines and outgoing commands before passing
		// the restart to the cgame
		Con_ClearNotify();
		// reparse the string, because Con_ClearNotify() may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		Com_Memset( cl.cmds, 0, sizeof( cl.cmds ) );
		cls.lastVidRestart = Sys_Milliseconds(); // hack for OSP mod
		return qtrue;
	}

	// the clientLevelShot command is used during development
	// to generate 128*128 screenshots from the intermission
	// point of levels for the menu system to use
	// we pass it along to the cgame to make appropriate adjustments,
	// but we also clear the console and notify lines here
	if ( !strcmp( cmd, "clientLevelShot" ) ) {
		// don't do it if we aren't running the server locally,
		// otherwise malicious remote servers could overwrite
		// the existing thumbnails
		if ( !com_sv_running->integer ) {
			return qfalse;
		}
		// close the console
		Con_Close();
		// take a special screenshot next frame
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return qtrue;
	}

	// we may want to put a "connect to other server" command here

	// cgame can now act on the command
	return qtrue;
}


/*
====================
CL_CM_LoadMap

Just adds default parameters that cgame doesn't need to know about
====================
*/
static void CL_CM_LoadMap( const char *mapname ) {
	int		checksum;

	CM_LoadMap( mapname, qtrue, &checksum );
	if ( !cgvm || cgvm->dllHandle ) {
		EntityBridge_ParseEntities( CM_EntityString() );
	}
}


/*
====================
CL_ShutdonwCGame

====================
*/
void CL_ShutdownCGame( void ) {

	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CGAME );
	cls.cgameStarted = qfalse;
	cls.stockBaseq3 = qfalse;

	if ( !cgvm ) {
		return;
	}

	re.VertexLighting( qfalse );

	VM_Call( cgvm, 0, CG_SHUTDOWN );
	VM_Free( cgvm );
	cgvm = NULL;
	FS_VM_CloseFiles( H_CGAME );
}


static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || cgvm == NULL )
	  return NULL;

	if ( cgvm->entryPoint )
		return (void *)(intValue);
	else
		return (void *)(cgvm->dataBase + (intValue & cgvm->dataMask));
}


static qboolean CL_GetValue( char* value, int valueSize, const char* key ) {

	if ( !Q_stricmp( key, "trap_R_AddRefEntityToScene2" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_R_ADDREFENTITYTOSCENE2 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_ForceFixedDLights" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_R_FORCEFIXEDDLIGHTS );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_R_AddLinearLightToScene_Q3E" ) && re.AddLinearLightToScene ) {
		Com_sprintf( value, valueSize, "%i", CG_R_ADDLINEARLIGHTTOSCENE );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_IsRecordingDemo" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_IS_RECORDING_DEMO );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Phys_CreateBody" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_CREATEBODY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_DestroyBody" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_DESTROYBODY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_ApplyForceBody" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_APPLYFORCEBODY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_ApplyImpulse" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_APPLYIMPULSE );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_GetBodyTransform" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_GETBODYTRANSFORM );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_SetBodyTransform" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_SETBODYTRANSFORM );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_SetBodyVelocity" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_SETBODYVELOCITY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_StepSimulation" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_STEPSIMULATION );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_RayCast" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_RAYCAST );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_LoadBSPCollision" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_PHYS_LOADBSPCOLLISION );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EmitJSEvent" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_EMIT_JSEVENT );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineSpriteAddLocal" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_ENGINE_SPRITE_ADD_LOCAL );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineDecalAddLocal" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_ENGINE_DECAL_ADD_LOCAL );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) ) {
		Com_sprintf( value, valueSize, "%i", CG_CVAR_SETDESCRIPTION );
		return qtrue;
	}

	return qfalse;
}


static void CL_ForceFixedDlights( void ) {
	cvar_t *cv;

	cv = Cvar_Get( "r_dlightMode", "1", 0 );
	if ( cv ) {
		Cvar_CheckRange( cv, "1", "2", CV_INTEGER );
	}
}


/*
====================
CL_CgameSystemCalls

The cgame module is making a system call
====================
*/
static intptr_t CL_CgameSystemCalls( intptr_t *args ) {
	com_activeVmLastSyscall = (int)args[0];
	switch( args[0] ) {
	case CG_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;
	case CG_ERROR:
		Com_Error( ERR_DROP, "%s", (const char*)VMA(1) );
		return 0;
	case CG_MILLISECONDS:
		return Sys_Milliseconds();
	case CG_CVAR_REGISTER:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( vmCvar_t ) );
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4], cgvm->privateFlag );
		return 0;
	case CG_CVAR_UPDATE:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( vmCvar_t ) );
		Cvar_Update( VMA(1), cgvm->privateFlag );
		return 0;
	case CG_CVAR_SET:
		Cvar_SetSafe( VMA(1), VMA(2) );
		return 0;
	case CG_CVAR_VARIABLESTRINGBUFFER:
		VM_CHECKBOUNDS( cgvm, args[2], args[3] );
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], CVAR_PRIVATE );
		return 0;
	case CG_ARGC:
		return Cmd_Argc();
	case CG_ARGV:
		VM_CHECKBOUNDS( cgvm, args[2], args[3] );
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case CG_ARGS:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		Cmd_ArgsBuffer( VMA(1), args[2] );
		return 0;

	case CG_FS_FOPENFILE:
		return FS_VM_OpenFile( VMA(1), VMA(2), args[3], H_CGAME );
	case CG_FS_READ:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_ReadFile( VMA(1), args[2], args[3], H_CGAME );
		return 0;
	case CG_FS_WRITE:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		FS_VM_WriteFile( VMA(1), args[2], args[3], H_CGAME );
		return 0;
	case CG_FS_FCLOSEFILE:
		FS_VM_CloseFile( args[1], H_CGAME );
		return 0;
	case CG_FS_SEEK:
		return FS_VM_SeekFile( args[1], args[2], args[3], H_CGAME );

	case CG_SENDCONSOLECOMMAND: {
		const char *cmd = VMA(1);
		Cbuf_NestedAdd( cmd );
		return 0;
	}
	case CG_ADDCOMMAND:
		CL_AddCgameCommand( VMA(1) );
		return 0;
	case CG_REMOVECOMMAND:
		Cmd_RemoveCommandSafe( VMA(1) );
		return 0;
	case CG_SENDCLIENTCOMMAND:
		CL_AddReliableCommand( VMA(1), qfalse );
		return 0;
	case CG_UPDATESCREEN:
		// this is used during lengthy level loading, so pump message loop
		/* Com_EventLoop(); if server restarts here, client state may be inconsistent. */
		// We can't call Com_EventLoop here, a restart will crash and this _does_ happen
		// if there is a map change while we are downloading at pk3.
		// ZOID
		SCR_UpdateScreen();
		return 0;
	case CG_CM_LOADMAP:
		CL_CM_LoadMap( VMA(1) );
		if ( Cvar_VariableIntegerValue( "cl_physicsEnabled" ) ) {
			Phys_LoadBSPCollision();
		}
		return 0;
	case CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case CG_CM_INLINEMODEL:
		return CM_InlineModel( args[1] );
	case CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qfalse );
	case CG_CM_TEMPCAPSULEMODEL:
		return CM_TempBoxModel( VMA(1), VMA(2), /*int capsule*/ qtrue );
	case CG_CM_POINTCONTENTS:
		return CM_PointContents( VMA(1), CL_GameEntityNumToEngine( args[2] ) );
	case CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( VMA(1), CL_GameEntityNumToEngine( args[2] ), VMA(3), VMA(4) );
	case CG_CM_BOXTRACE:
		if ( cgvm && !cgvm->dllHandle ) {
			trace_t trace;

			VM_CHECKBOUNDS( cgvm, args[1], sizeof( legacy_trace_t ) );
			CM_BoxTrace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qfalse );
			CL_FillLegacyTrace( (legacy_trace_t *)VMA(1), &trace );
		} else {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( trace_t ) );
			CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qfalse );
		}
		return 0;
	case CG_CM_CAPSULETRACE:
		if ( cgvm && !cgvm->dllHandle ) {
			trace_t trace;

			VM_CHECKBOUNDS( cgvm, args[1], sizeof( legacy_trace_t ) );
			CM_BoxTrace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qtrue );
			CL_FillLegacyTrace( (legacy_trace_t *)VMA(1), &trace );
		} else {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( trace_t ) );
			CM_BoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qtrue );
		}
		return 0;
	case CG_CM_TRANSFORMEDBOXTRACE:
		if ( cgvm && !cgvm->dllHandle ) {
			trace_t trace;

			VM_CHECKBOUNDS( cgvm, args[1], sizeof( legacy_trace_t ) );
			CM_TransformedBoxTrace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], VMA(8), VMA(9), /*int capsule*/ qfalse );
			CL_FillLegacyTrace( (legacy_trace_t *)VMA(1), &trace );
		} else {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( trace_t ) );
			CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], VMA(8), VMA(9), /*int capsule*/ qfalse );
		}
		return 0;
	case CG_CM_TRANSFORMEDCAPSULETRACE:
		if ( cgvm && !cgvm->dllHandle ) {
			trace_t trace;

			VM_CHECKBOUNDS( cgvm, args[1], sizeof( legacy_trace_t ) );
			CM_TransformedBoxTrace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], VMA(8), VMA(9), /*int capsule*/ qtrue );
			CL_FillLegacyTrace( (legacy_trace_t *)VMA(1), &trace );
		} else {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( trace_t ) );
			CM_TransformedBoxTrace( VMA(1), VMA(2), VMA(3), VMA(4), VMA(5),
				CL_GameEntityNumToEngine( args[6] ), args[7], VMA(8), VMA(9), /*int capsule*/ qtrue );
		}
		return 0;
	case CG_CM_MARKFRAGMENTS:
		return re.MarkFragments( args[1], VMA(2), VMA(3), args[4], VMA(5), args[6], VMA(7) );
	case CG_S_STARTSOUND:
		S_StartSound( VMA(1), args[2], args[3], args[4] );
		return 0;
	case CG_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds(args[1]);
		return 0;
	case CG_S_ADDLOOPINGSOUND:
		S_AddLoopingSound( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_ADDREALLOOPINGSOUND:
		S_AddRealLoopingSound( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_STOPLOOPINGSOUND:
		S_StopLoopingSound( args[1] );
		return 0;
	case CG_S_UPDATEENTITYPOSITION:
		S_UpdateEntityPosition( args[1], VMA(2) );
		return 0;
	case CG_S_RESPATIALIZE:
		S_Respatialize( args[1], VMA(2), VMA(3), args[4] );
		return 0;
	case CG_S_REGISTERSOUND:
		return S_RegisterSound( VMA(1), args[2] );
	case CG_S_STARTBACKGROUNDTRACK:
		S_StartBackgroundTrack( VMA(1), VMA(2) );
		return 0;
	case CG_R_LOADWORLDMAP:
		re.LoadWorld( VMA(1) );
		return 0;
	case CG_R_REGISTERMODEL:
		return re.RegisterModel( VMA(1) );
	case CG_R_REGISTERSKIN:
		return re.RegisterSkin( VMA(1) );
	case CG_R_REGISTERSHADER:
		return re.RegisterShader( VMA(1) );
	case CG_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip( VMA(1) );
	case CG_R_REGISTERFONT:
		re.RegisterFont( VMA(1), args[2], VMA(3));
		return 0;
	case CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case CG_R_ADDREFENTITYTOSCENE:
		if ( cgvm && !cgvm->dllHandle ) {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( retailRefEntity_t ) );
			CL_AddRetailRefEntityToScene( VMA(1), qfalse );
		} else {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( refEntity_t ) );
			re.AddRefEntityToScene( VMA(1), qfalse );
		}
		return 0;
	case CG_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), 1 );
		return 0;
	case CG_R_ADDPOLYSTOSCENE:
		re.AddPolyToScene( args[1], args[2], VMA(3), args[4] );
		return 0;
	case CG_R_LIGHTFORPOINT:
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( vec3_t ) );
		VM_CHECKBOUNDS( cgvm, args[3], sizeof( vec3_t ) );
		VM_CHECKBOUNDS( cgvm, args[4], sizeof( vec3_t ) );
		return re.LightForPoint( VMA(1), VMA(2), VMA(3), VMA(4) );
	case CG_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_ADDADDITIVELIGHTTOSCENE:
		re.AddAdditiveLightToScene( VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_RENDERSCENE:
		if ( !CL_StockBaseq3Mode() ) {
			CL_EngineSprites_AddFromSnapshot();
			CL_EngineDecals_AddFromSnapshot();
		}
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( refdef_t ) );
		re.RenderScene( VMA(1) );
		return 0;
	case CG_R_SETCOLOR:
		re.SetColor( VMA(1) );
		return 0;
	case CG_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;
	case CG_R_MODELBOUNDS:
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( vec3_t ) );
		VM_CHECKBOUNDS( cgvm, args[3], sizeof( vec3_t ) );
		re.ModelBounds( args[1], VMA(2), VMA(3) );
		return 0;
	case CG_R_LERPTAG:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( orientation_t ) );
		return re.LerpTag( VMA(1), args[2], args[3], args[4], VMF(5), VMA(6) );
	case CG_GETGLCONFIG:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( glconfig_t ) );
		CL_GetGlconfig( VMA(1) );
		return 0;
	case CG_GETGAMESTATE:
		if ( cgvm->dllHandle ) {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( gameState_t ) );
			CL_GetGameState( VMA(1) );
		} else {
			/* Retail/OA cgame.qvm ships with legacy gameState_t (1024 CS, 16k data). */
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( legacyGameState_t ) );
			CL_GetLegacyGameState( VMA(1) );
		}
		return 0;
	case CG_GETCURRENTSNAPSHOTNUMBER:
		VM_CHECKBOUNDS( cgvm, args[1], sizeof( int ) );
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( int ) );
		CL_GetCurrentSnapshotNumber( VMA(1), VMA(2) );
		return 0;
	case CG_GETSNAPSHOT:
		if ( cgvm->dllHandle ) {
			VM_CHECKBOUNDS( cgvm, args[2], sizeof( snapshot_t ) );
			return CL_GetSnapshot( args[1], VMA(2) );
		}
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( legacySnapshot_t ) );
		return CL_GetLegacySnapshot( args[1], VMA(2) );
	case CG_GETSERVERCOMMAND:
		return CL_GetServerCommand( args[1] );
	case CG_GETCURRENTCMDNUMBER:
		return CL_GetCurrentCmdNumber();
	case CG_GETUSERCMD:
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( usercmd_t ) );
		return CL_GetUserCmd( args[1], VMA(2) );
	case CG_SETUSERCMDVALUE:
		CL_SetUserCmdValue( args[1], VMF(2) );
		return 0;
	case CG_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();
	case CG_KEY_ISDOWN:
		return Key_IsDown( args[1] );
	case CG_KEY_GETCATCHER:
		return Key_GetCatcher();
	case CG_KEY_SETCATCHER:
		// Don't allow the cgame module to close the console
		Key_SetCatcher( args[1] | ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) );
		return 0;
	case CG_KEY_GETKEY:
		return Key_GetKey( VMA(1) );

	// shared syscalls
	case TRAP_MEMSET:
		VM_CHECKBOUNDS( cgvm, args[1], args[3] );
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];
	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( cgvm, args[1], args[2], args[3] );
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( cgvm, args[1], args[3] );
		Q_strncpy( VMA(1), VMA(2), args[3] );
		return args[1];
	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );
	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );
	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );
	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case CG_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );
	case CG_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );
	case CG_TESTPRINTINT:
		return Com_sprintf( VMA(1), MAX_STRING_CHARS, "%i", (int)args[2] );
	case CG_TESTPRINTFLOAT:
		return Com_sprintf( VMA(1), MAX_STRING_CHARS, "%f", VMF(2) );
	case CG_ACOS:
		return FloatAsInt( Q_acos( VMF(1) ) );

	case CG_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case CG_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case CG_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case CG_PC_READ_TOKEN:
		VM_CHECKBOUNDS( cgvm, args[2], sizeof( pc_token_t ) );
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case CG_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case CG_S_STOPBACKGROUNDTRACK:
		S_StopBackgroundTrack();
		return 0;

	case CG_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case CG_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

	case CG_CIN_PLAYCINEMATIC:
		return CIN_PlayCinematic(VMA(1), args[2], args[3], args[4], args[5], args[6]);

	case CG_CIN_STOPCINEMATIC:
		return CIN_StopCinematic(args[1]);

	case CG_CIN_RUNCINEMATIC:
		return CIN_RunCinematic(args[1]);

	case CG_CIN_DRAWCINEMATIC:
		CIN_DrawCinematic(args[1]);
		return 0;

	case CG_CIN_SETEXTENTS:
		CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
		return 0;

	case CG_R_REMAP_SHADER:
		re.RemapShader( VMA(1), VMA(2), VMA(3) );
		return 0;

/*
	case CG_LOADCAMERA:
		return loadCamera(VMA(1));

	case CG_STARTCAMERA:
		startCamera(args[1]);
		return 0;

	case CG_GETCAMERAINFO:
		return getCameraInfo(args[1], VMA(2), VMA(3));
*/
	case CG_GET_ENTITY_TOKEN:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		return re.GetEntityToken( VMA(1), args[2] );

	case CG_R_INPVS:
		return re.inPVS( VMA(1), VMA(2) );

	// engine extensions
	case CG_R_ADDREFENTITYTOSCENE2:
		if ( cgvm && !cgvm->dllHandle ) {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( retailRefEntity_t ) );
			CL_AddRetailRefEntityToScene( VMA(1), qtrue );
		} else {
			VM_CHECKBOUNDS( cgvm, args[1], sizeof( refEntity_t ) );
			re.AddRefEntityToScene( VMA(1), qtrue );
		}
		return 0;

	case CG_R_ADDLINEARLIGHTTOSCENE:
		re.AddLinearLightToScene( VMA(1), VMA(2), VMF(3), VMF(4), VMF(5), VMF(6) );
		return 0;

	case CG_R_FORCEFIXEDDLIGHTS:
		CL_ForceFixedDlights();
		return 0;

	case CG_IS_RECORDING_DEMO:
		return clc.demorecording;

	case CG_CVAR_SETDESCRIPTION:
		Cvar_SetDescription2( (const char*)VMA(1), (const char*)VMA(2) );
		return 0;

	case CG_TRAP_GETVALUE:
		VM_CHECKBOUNDS( cgvm, args[1], args[2] );
		return CL_GetValue( VMA(1), args[2], VMA(3) );

	/* ---- Physics syscalls (Gopex) ---- */

	case CG_PHYS_CREATEBODY:
		return Phys_CreateBody( (const physBodyDef_t *)VMA(1) );

	case CG_PHYS_DESTROYBODY:
		Phys_DestroyBody( (physBodyHandle_t)args[1] );
		return 0;

	case CG_PHYS_APPLYFORCEBODY:
		Phys_ApplyForce( (physBodyHandle_t)args[1], (const float *)VMA(2), (const float *)VMA(3) );
		return 0;

	case CG_PHYS_APPLYIMPULSE:
		Phys_ApplyImpulse( (physBodyHandle_t)args[1], (const float *)VMA(2), (const float *)VMA(3) );
		return 0;

	case CG_PHYS_GETBODYTRANSFORM:
		Phys_GetBodyTransform( (physBodyHandle_t)args[1], (physTransform_t *)VMA(2) );
		return 0;

	case CG_PHYS_SETBODYTRANSFORM:
		Phys_SetBodyTransform( (physBodyHandle_t)args[1], (const float *)VMA(2), (const float *)VMA(3) );
		return 0;

	case CG_PHYS_SETBODYVELOCITY:
		Phys_SetBodyVelocity( (physBodyHandle_t)args[1], (const float *)VMA(2), (const float *)VMA(3) );
		return 0;

	case CG_PHYS_STEPSIMULATION:
		Phys_StepSimulation( VMF(1) );
		return 0;

	case CG_PHYS_RAYCAST:
		return Phys_RayCast( (const float *)VMA(1), (const float *)VMA(2), (physRayResult_t *)VMA(3) );

	case CG_PHYS_LOADBSPCOLLISION:
		return Phys_LoadBSPCollision();

	case CG_EMIT_JSEVENT:
		Com_ScriptEmitEvent( (const char *)VMA(1), (const char *)VMA(2), (const char *)VMA(3), args[4], args[5] );
		return 0;

	case CG_ENGINE_SPRITE_ADD_LOCAL: {
		engineSpriteDesc_t desc;
		int timeMs;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.type = (engineSpriteType_t)args[1];
		desc.shader = (qhandle_t)args[2];
		desc.origin[0] = VMF( 3 );
		desc.origin[1] = VMF( 4 );
		desc.origin[2] = VMF( 5 );
		desc.radius = VMF( 6 );
		desc.rotation = VMF( 7 );
		desc.cols = args[8] > 0 ? (int)args[8] : 1;
		desc.rows = args[9] > 0 ? (int)args[9] : 1;
		desc.fps = VMF( 10 );
		if ( desc.fps <= 0.0f ) {
			desc.fps = 8.0f;
		}
		timeMs = (int)args[11];
		if ( timeMs <= 0 ) {
			timeMs = cls.realtime;
		}
		CL_EngineSprite_AddLocalAtTime( &desc, timeMs );
		return 0;
	}

	case CG_ENGINE_DECAL_ADD_LOCAL: {
		engineDecalDesc_t desc;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.shader = (qhandle_t)args[1];
		desc.origin[0] = VMF( 2 );
		desc.origin[1] = VMF( 3 );
		desc.origin[2] = VMF( 4 );
		desc.radius = VMF( 5 );
		desc.pitch = VMF( 6 );
		desc.yaw = VMF( 7 );
		CL_EngineDecal_AddLocal( &desc );
		return 0;
	}

	default:
		Com_Error( ERR_DROP, "Bad cgame system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
CL_DllSyscall
====================
*/
static intptr_t QDECL CL_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t	args[10]; // max.count for cgame
	va_list	ap;
	int i;

	args[0] = arg;
	va_start( ap, arg );
	for (i = 1; (size_t)i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return CL_CgameSystemCalls( args );
#else
	return CL_CgameSystemCalls( &arg );
#endif
}


/*
====================
CL_EnsureClientGameVersionConfigstring

Stock cgame.qvm compares CS_GAME_VERSION to GAME_VERSION ("baseq3-1") in CG_INIT.
Retail qagame normally sets it in worldspawn; if the gamestate arrives without
that configstring, derive it from serverinfo gamename (stock qagame sets gamename).
====================
*/
static void CL_EnsureClientGameVersionConfigstring( void ) {
	const char *version;
	const char *info;
	const char *gamename;
	char built[32];
	int offset;
	size_t len;

	offset = cl.gameState.stringOffsets[CS_GAME_VERSION];
	version = ( offset > 0 ) ? ( cl.gameState.stringData + offset ) : "";
	if ( version && version[0] ) {
		return;
	}

	info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
	gamename = Info_ValueForKey( info, "gamename" );
	len = CL_BuildFallbackGameVersion( gamename, "baseq3", built, sizeof( built ) );
	if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
		Com_Error( ERR_DROP, "%s: MAX_GAMESTATE_CHARS exceeded", __func__ );
	}

	cl.gameState.stringOffsets[CS_GAME_VERSION] = cl.gameState.dataCount;
	Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, built, len + 1 );
	cl.gameState.dataCount += (int)len + 1;

	Com_Printf( S_COLOR_YELLOW
		"[client] CS_GAME_VERSION missing in gamestate; using %s for stock cgame\n", built );
}

/*
====================
CL_IsBaseQ3Game

True when playing retail Quake III data (not a standalone fs_game mod).
====================
*/
static qboolean CL_IsBaseQ3Game( void ) {
	const char *fs_game;
	const char *fs_basegame;
	const char *info;
	const char *gamename;

	fs_game = Cvar_VariableString( "fs_game" );
	if ( fs_game && fs_game[0] ) {
		return ( qboolean)( Q_stricmp( fs_game, "baseq3" ) == 0 );
	}

	fs_basegame = Cvar_VariableString( "fs_basegame" );
	if ( fs_basegame && fs_basegame[0] && Q_stristr( fs_basegame, "baseq3" ) ) {
		return qtrue;
	}

	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	gamename = Info_ValueForKey( info, "gamename" );
	if ( gamename && gamename[0] && !Q_stricmp( gamename, "baseq3" ) ) {
		return qtrue;
	}

	return qfalse;
}

/*
====================
CL_IsOpenArenaGame

Detect OpenArena-native content so we can avoid the modern full-conversion
profile, which enables engine systems OA was not authored against.
====================
*/
static qboolean CL_IsOpenArenaGame( void ) {
	const char *fs_game;
	const char *fs_basegame;
	const char *info;
	const char *gamename;

	fs_game = Cvar_VariableString( "fs_game" );
	if ( fs_game && fs_game[0] ) {
		if ( !Q_stricmp( fs_game, "openarena" ) || !Q_stricmp( fs_game, "baseoa" ) ) {
			return qtrue;
		}
	}

	fs_basegame = Cvar_VariableString( "fs_basegame" );
	if ( fs_basegame && fs_basegame[0] ) {
		if ( Q_stristr( fs_basegame, "openarena" ) || Q_stristr( fs_basegame, "baseoa" ) ) {
			return qtrue;
		}
	}

	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	gamename = Info_ValueForKey( info, "gamename" );
	if ( gamename && gamename[0] ) {
		if ( !Q_stricmp( gamename, "openarena" ) || !Q_stricmp( gamename, "baseoa" ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
====================
CL_ApplyClassicBaseq3Cvars

Synchronous retail-safe defaults before cgame.qvm CG_INIT (exec classic_baseq3.cfg
is still queued for user overrides / archived seta values).
====================
*/
static void CL_ApplyClassicBaseq3Cvars( void ) {
	cls.stockBaseq3 = qtrue;

	Cvar_Set( "r_classicLighting", "1" );
	Cvar_Set( "r_pbr", "0" );
	Cvar_Set( "r_fbo", "0" );
	Cvar_Set( "r_taa", "0" );
	Cvar_Set( "r_ssr", "0" );
	Cvar_Set( "r_pbrSunShadow", "0" );
	Cvar_Set( "r_forwardPlusOverflowShade", "0" );
	Cvar_Set( "r_shWorldLighting", "0" );
	Cvar_Set( "cl_physicsEnabled", "0" );
	Cvar_Set( "r_openWorld", "0" );
	Cvar_Set( "cl_openWorldSync", "0" );
	Cvar_Set( "r_bspStream", "0" );
	Cvar_Set( "r_volumetricFog", "0" );
	Cvar_Set( "r_district", "0" );
	Cvar_Set( "cm_stream", "0" );
	Cvar_Set( "cm_streamMerge", "0" );
	Cvar_Set( "cm_openWorldCollision", "0" );
	Cvar_Set( "cl_builtInTtfConsole", "0" );
	Cvar_Set( "r_sdfEnable", "0" );
	Cvar_Set( "cl_engineSprites", "0" );
	Cvar_Set( "r_spriteProps", "0" );
	Cvar_Set( "cl_navEnabled", "0" );
	Cvar_Set( "cl_particlesEnabled", "0" );
	Cvar_Set( "g_ecsMotion", "0" );
	Cvar_Set( "sv_engineSprites", "0" );
	Cvar_Set( "sv_engineSpritesSpawn", "0" );
	Cvar_Set( "sv_engineDecals", "0" );
	Cvar_Set( "sv_engineDecalsSpawn", "0" );
	Cvar_Set( "sv_openWorld", "0" );
	Cvar_Set( "r_imgui", "0" );
	Cvar_Set( "r_studio_tools", "0" );

	CM_Stream_Merge_ClearAll();

	Com_Printf( "[client] stock baseq3 mode: modern overlays and middleware disabled\n" );
}

/*
====================
CL_TryEarlyStockBaseq3Profile

Listen-server map loads call SCR_UpdateScreen before cgame init; disable ImGui
and studio overlays early for retail baseq3 when auto profile is on.
====================
*/
void CL_TryEarlyStockBaseq3Profile( void ) {
	if ( !cl_autoGraphicsProfile || !cl_autoGraphicsProfile->integer ) {
		return;
	}
	if ( !CL_IsBaseQ3Game() && !CL_IsOpenArenaGame() ) {
		return;
	}

	Cvar_Set( "r_imgui", "0" );
	Cvar_Set( "r_studio_tools", "0" );
	Cvar_Set( "r_bspStream", "0" );
	Cvar_Set( "cm_stream", "0" );
	Cvar_Set( "cm_streamMerge", "0" );
	Cvar_Set( "cm_openWorldCollision", "0" );
	CM_Stream_Merge_ClearAll();
}

/*
====================
CL_ApplyGraphicsProfile

baseq3 + cgame.qvm -> classic retail look; native cgame -> modern Vulkan stack.
====================
*/
static void CL_ApplyGraphicsProfile( vm_t *vm ) {
	const qboolean isQvm = ( vm && !vm->dllHandle );
	const qboolean isBaseQ3 = CL_IsBaseQ3Game();
	const qboolean isOpenArena = CL_IsOpenArenaGame();

	if ( !cl_autoGraphicsProfile || !cl_autoGraphicsProfile->integer ) {
		return;
	}

	if ( isBaseQ3 && isQvm ) {
		Com_Printf( "[client] cl_autoGraphicsProfile: classic baseq3 (cgame.qvm)\n" );
		CL_ApplyClassicBaseq3Cvars();
		Cbuf_AddText( "exec classic_baseq3.cfg\n" );
		return;
	}

	if ( isOpenArena && !isQvm ) {
		Com_Printf( "[client] cl_autoGraphicsProfile: classic native OpenArena\n" );
		CL_ApplyClassicBaseq3Cvars();
		cls.stockBaseq3 = qfalse;
		Cbuf_AddText( "exec classic_openarena_native.cfg\n" );
		return;
	}

	if ( !isQvm ) {
		Com_Printf( "[client] cl_autoGraphicsProfile: modern native cgame\n" );
		cls.stockBaseq3 = qfalse;
		Cbuf_AddText( "exec modern_native.cfg\nvid_restart\n" );
		return;
	}

	if ( isQvm ) {
		cls.stockBaseq3 = qfalse;
		Com_Printf( "[client] cl_autoGraphicsProfile: QVM mod (classic lighting default preserved)\n" );
	}
}

/*
====================
CL_InitCGame

Should only be called by CL_StartHunkUsers
====================
*/
void CL_InitCGame( void ) {
	const char			*info;
	const char			*mapname;
	int					t1, t2;
	vmInterpret_t		interpret;

	Cbuf_NestedReset();

	t1 = Sys_Milliseconds();

	// put away the console
	Con_Close();

	// find the current mapname
	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );
	Com_ScriptEmitEvent( "map_load", mapname, NULL, 0, 0 );

	// allow vertex lighting for in-game elements
	re.VertexLighting( qtrue );

	// load the dll or bytecode
	interpret = Cvar_VariableIntegerValue( "vm_cgame" );
	if ( CL_IsOpenArenaGame() ) {
		interpret = VMI_NATIVE;
	}
	if ( cl_connectedToPureServer )
	{
		// if sv_pure is set we only allow qvms to be loaded
		if ( interpret != VMI_COMPILED && interpret != VMI_BYTECODE )
			interpret = VMI_COMPILED;
	}

	cgvm = VM_Create( VM_CGAME, CL_CgameSystemCalls, CL_DllSyscall, interpret );
	if ( !cgvm ) {
		Com_Error( ERR_DROP, "VM_Create on cgame failed" );
	}
	cls.state = CA_LOADING;

	CL_EnsureClientGameVersionConfigstring();

	CL_ApplyGraphicsProfile( cgvm );

	// init for this gamestate
	// use the lastExecutedServerCommand instead of the serverCommandSequence
	// otherwise server commands sent just before a gamestate are dropped
	VM_Call( cgvm, 3, CG_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum );

	if ( !cgvm->dllHandle && Cvar_VariableIntegerValue( "cl_physicsEnabled" ) ) {
		Cvar_Set( "cl_physicsEnabled", "0" );
		Com_Printf( "[client] cl_physicsEnabled 0 for cgame.qvm compatibility\n" );
	}

	// reset any CVAR_CHEAT cvars registered by cgame
	if ( !clc.demoplaying && !cl_connectedToCheatServer )
		Cvar_SetCheatState();

	// we will send a usercmd this frame, which
	// will cause the server to send us the first snapshot
	cls.state = CA_PRIMED;

	t2 = Sys_Milliseconds();

	Com_Printf( "CL_InitCGame: %5.2f seconds\n", (t2-t1)/1000.0 );

	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	// make sure everything is paged in
	if (!Sys_LowPhysicalMemory()) {
		Com_TouchMemory();
	}

	// clear anything that got printed
	Con_ClearNotify ();

	// do not allow vid_restart for first time
	cls.lastVidRestart = Sys_Milliseconds();
}


/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/

qboolean CL_GameCommand( void ) {
	qboolean bRes;

	if ( !cgvm ) {
		return qfalse;
	}

	bRes = (qboolean)VM_Call( cgvm, 0, CG_CONSOLE_COMMAND );

	Cbuf_NestedReset();

	return bRes;
}


/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
	VM_Call( cgvm, 3, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereo, clc.demoplaying );
#ifdef DEBUG
	VM_Debug( 0 );
#endif
	if ( cgvm && cgvm->dllHandle ) {
		CL_PhysDebugDrawSubmit();
	}
}


/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have cl.serverTime exactly equal the server's view
of time plus the timeNudge, but with variable latencies over
the internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time approach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives with a rational
latency, which keeps the adjustment process framerate independent and
prevents massive overadjustment during times of significant packet loss
or bursted delayed packets.
=================
*/

#define	RESET_TIME	500

static void CL_AdjustTimeDelta( void ) {
	int		newDelta;
	int		deltaDelta;

	cl.newSnapshots = qfalse;

	// the delta never drifts when replaying a demo
	if ( clc.demoplaying ) {
		return;
	}

	newDelta = cl.snap.serverTime - cls.realtime;
	deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > RESET_TIME ) {
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.snap.serverTime;	/* May affect cgame time delta. */
		cl.serverTime = cl.snap.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
	} else {
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( cl.extrapolatedSnapshot ) {
				cl.extrapolatedSnapshot = qfalse;
				cl.serverTimeDelta -= 2;
			} else {
				// otherwise, move our sense of time forward to minimize total latency
				cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
}


/*
==================
CL_ValidateStockSpawnOrigin

After cgame loads CM, verify spawn is inside world collision and no sector overlay is active.
==================
*/
static void CL_ValidateStockSpawnOrigin( void ) {
	vec3_t mins, maxs, org;
	int contents, merged;
	qboolean inside;

	if ( !CL_StockBaseq3Mode() ) {
		return;
	}

	VectorCopy( cl.snap.ps.origin, org );
	CM_ModelBounds( CM_InlineModel( 0 ), mins, maxs );
	inside = CL_PointInsideAABB( org, mins, maxs );
	contents = CM_PointContents( org, 0 );
	merged = CM_Stream_MergedCount();

	Com_Printf( "[client] spawn CM AABB [(%.0f %.0f %.0f)-(%.0f %.0f %.0f)] inside=%s contents=0x%x merged_sectors=%d\n",
		mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
		inside ? "yes" : "NO", contents, merged );

	if ( !inside || merged > 0 ) {
		Com_Printf( S_COLOR_YELLOW
			"WARNING: stock spawn outside map CM or sector overlay active — reset cm_stream/cm_streamMerge/r_openWorld archives\n" );
	}
}

/*
==================
CL_FirstSnapshot
==================
*/
static void CL_FirstSnapshot( void ) {
	// ignore snapshots that don't have entities
	if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	cls.state = CA_ACTIVE;

	Com_Printf( "[client] spawn origin (%.1f %.1f %.1f) map=%s\n",
		cl.snap.ps.origin[0], cl.snap.ps.origin[1], cl.snap.ps.origin[2],
		cl.mapname[0] ? cl.mapname : "?" );

	CL_ValidateStockSpawnOrigin();

	// clear old game so we will not switch back to old mod on disconnect
	CL_ResetOldGame();

	// set the timedelta so we are exactly on this first frame
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.oldServerTime = cl.snap.serverTime;

	clc.timeDemoBaseTime = cl.snap.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// this is to allow scripting a timedemo to start right
	// after loading
	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cbuf_AddText( "\n" );
		Cvar_Set( "activeAction", "" );
	}

	Sys_BeginProfiling();
}


/*
==================
CL_AvgPing

Calculates Average Ping from snapshots in buffer. Used by AutoNudge.
==================
*/
static float CL_AvgPing( void ) {
	int ping[PACKET_BACKUP];
	int count = 0;
	int i, j, iTemp;
	float result;

	for ( i = 0; i < PACKET_BACKUP; i++ ) {
		if ( cl.snapshots[i].ping > 0 && cl.snapshots[i].ping < 999 ) {
			ping[count] = cl.snapshots[i].ping;
			count++;
		}
	}

	if ( count == 0 )
		return 0;

	// sort ping array
	for ( i = count - 1; i > 0; --i ) {
		for ( j = 0; j < i; ++j ) {
			if (ping[j] > ping[j + 1]) {
				iTemp = ping[j];
				ping[j] = ping[j + 1];
				ping[j + 1] = iTemp;
			}
		}
	}

	// use median average ping
	if ( (count % 2) == 0 )
		result = (ping[count / 2] + ping[(count / 2) - 1]) / 2.0f;
	else
		result = ping[count / 2];

	return result;
}


/*
==================
CL_TimeNudge

Returns either auto-nudge or cl_timeNudge value.
==================
*/
static int CL_TimeNudge( void ) {
	float autoNudge = cl_autoNudge->value;

	if ( autoNudge != 0.0f )
		return (int)((CL_AvgPing() * autoNudge) + 0.5f) * -1;
	else
		return cl_timeNudge->integer;
}


/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( void ) {
	qboolean demoFreezed;

	// getting a valid frame message ends the connection process
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
		if ( clc.demoplaying ) {
			// we shouldn't get the first snapshot on the same frame
			// as the gamestate, because it causes a bad time skip
			if ( !clc.firstDemoFrameSkipped ) {
				clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}
		if ( cls.state != CA_ACTIVE ) {
			return;
		}
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	if ( !cl.snap.valid ) {
		Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	if ( sv_paused->integer && CL_CheckPaused() && com_sv_running->integer ) {
		// paused
		return;
	}

	if ( cl.snap.serverTime - cl.oldFrameServerTime < 0 ) {
		Com_Error( ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
	}
	cl.oldFrameServerTime = cl.snap.serverTime;

	// get our current view of time
	demoFreezed = clc.demoplaying && com_timescale->value == 0.0f;
	if ( demoFreezed ) {
		// \timescale 0 is used to lock a demo in place for single frame advances
		cl.serverTimeDelta -= cls.frametime;
	} else {
		// cl_timeNudge is a user adjustable cvar that allows more
		// or less latency to be added in the interest of better
		// smoothness or better responsiveness.
		cl.serverTime = cls.realtime + cl.serverTimeDelta - CL_TimeNudge();

		// guarantee that time will never flow backwards, even if
		// serverTimeDelta made an adjustment or cl_timeNudge was changed
		if ( cl.serverTime - cl.oldServerTime < 0 ) {
			cl.serverTime = cl.oldServerTime;
		}
		cl.oldServerTime = cl.serverTime;

		// note if we are almost past the latest frame (without timeNudge),
		// so we will try and adjust back a bit when the next snapshot arrives
		//if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
		if ( cls.realtime + cl.serverTimeDelta - cl.snap.serverTime >= -5 ) {
			cl.extrapolatedSnapshot = qtrue;
		}
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}

	if ( !clc.demoplaying ) {
		return;
	}

	// if we are playing a demo back, we can just keep reading
	// messages from the demo file until the cgame definitely
	// has valid snapshots to interpolate between

	// a timedemo will always use a deterministic set of time samples
	// no matter what speed machine it is run on,
	// while a normal demo may have different time samples
	// each time it is played back
	if ( com_timedemo->integer ) {
		if ( !clc.timeDemoStart ) {
			clc.timeDemoStart = Sys_Milliseconds();
		}
		clc.timeDemoFrames++;
		cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * 50;
	}

	//while ( cl.serverTime >= cl.snap.serverTime ) {
	while ( cl.serverTime - cl.snap.serverTime >= 0 ) {
		// feed another message, which should change
		// the contents of cl.snap
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) {
			return; // end of demo
		}
	}
}
