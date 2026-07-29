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
// sv_game.c -- interface to the game dll

#include "server.h"
#include "sv_engine_sprites.h"
#include "sv_engine_decals.h"
#include "phys_character.h"
#include "phys_bullet.h"
#include "phys_ragdoll_bind.h"
#include "com_loc.h"
#include "engine_db.h"
#include "modules/rts/rts_public.h"

#include "botlib.h"

botlib_export_t	*botlib_export;

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	int		num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}


sharedEntity_t *SV_GentityNum( int num ) {
	sharedEntity_t *ent;

	ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));

	return ent;
}


playerState_t *SV_GameClientNum( int num ) {
	playerState_t	*ps;

	ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));

	return ps;
}


svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}


sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int		num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}


/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
static void SV_GameSendServerCommand( int clientNum, const char *text ) {
	if ( clientNum == -1 ) {
		SV_SendServerCommand( NULL, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv.maxclients ) {
			return;
		}
		SV_SendServerCommand( svs.clients + clientNum, "%s", text );
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
static void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
static void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;

	if ( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if ( name[0] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}

	ent->s.modelindex = atoi( name + 1 );

	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy (mins, ent->r.mins);
	VectorCopy (maxs, ent->r.maxs);
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;		// we don't know exactly what is in the brushes

	SV_LinkEntity( ent );		/* Could be moved to game module. */
}


/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS( const vec3_t p1, const vec3_t p2 )
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
static qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 )
{
	int		leafnum;
	int		cluster;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
static void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
	svEntity_t	*svEnt;

	svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_EntityContact
==================
*/
static qboolean SV_EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, const int capsule ) {
	const float	*origin, *angles;
	clipHandle_t	ch;
	trace_t			trace;

	// check for exact collision
	origin = gEnt->r.currentOrigin;
	angles = gEnt->r.currentAngles;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, origin, angles, capsule );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo
===============
*/
static void SV_GetServerinfo( char *buffer, int bufferSize ) {

	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	if ( sv.state != SS_GAME || !sv.configstrings[ CS_SERVERINFO ] ) {
		SV_BuildServerInfoString( buffer, bufferSize );
	} else {
		Q_strncpyz( buffer, sv.configstrings[ CS_SERVERINFO ], bufferSize );
	}
}


#define RETAIL_QVM_MAX_GENTITIES	1024

static int sv_qvmGameEntCap;
static qboolean sv_qvmEntityMappingLogged;

static qboolean SV_UseLegacyNativeEntityNums( void );

/*
 * Retail qagame.qvm uses the original enum-backed qboolean, so its trace_t is
 * larger there than in the native engine build where qboolean is bool. Trace
 * syscalls for QVMs must marshal into the legacy layout explicitly.
 *
 * Native engine trace_t also carries BSP30 plane provenance fields that retail
 * QVMs and third-party native game DLLs (OpenArena, etc.) do not have. Those
 * must never be written through G_TRACE / G_TRACECAPSULE — doing so overflows
 * the game module's stack buffer (stack-smashing abort in FinishSpawningItem).
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

/* Native-DLL retail layout: bool qbooleans, no BSP30 provenance (52 bytes). */
typedef struct {
	qboolean	allsolid;
	qboolean	startsolid;
	float		fraction;
	vec3_t		endpos;
	cplane_t	plane;
	int			surfaceFlags;
	int			contents;
	int			entityNum;
} native_compat_trace_t;

/* Provenance appended to engine trace_t: planeKind + 5 ints = 24 bytes. */
enum { SV_TRACE_PROVENANCE_BYTES = 24 };

/* +4: two legacy int qbooleans vs native bools. -provenance: engine-only fields. */
STATIC_ASSERT( sizeof( legacy_trace_t ) == sizeof( trace_t ) + 4 - SV_TRACE_PROVENANCE_BYTES,
		"legacy_trace_t must match retail qagame trace layout" );
STATIC_ASSERT( sizeof( native_compat_trace_t ) + SV_TRACE_PROVENANCE_BYTES == sizeof( trace_t ),
		"native_compat_trace_t must be engine trace_t without provenance" );
STATIC_ASSERT( sizeof( native_compat_trace_t ) == 52,
		"native_compat_trace_t must match retail native qagame trace size" );
STATIC_ASSERT( sizeof( surfTraceEx_t ) >= sizeof( native_compat_trace_t ) + 8,
		"surfTraceEx_t must carry versioned provenance beyond the retail base" );

static void SV_FillLegacyTrace( legacy_trace_t *out, const trace_t *in ) {
	out->allsolid = in->allsolid ? 1 : 0;
	out->startsolid = in->startsolid ? 1 : 0;
	out->fraction = in->fraction;
	VectorCopy( in->endpos, out->endpos );
	out->plane = in->plane;
	out->surfaceFlags = in->surfaceFlags;
	out->contents = in->contents;
	out->entityNum = SV_EngineEntityNumToGame( in->entityNum );
}

static void SV_FillNativeCompatTrace( native_compat_trace_t *out, const trace_t *in ) {
	out->allsolid = in->allsolid;
	out->startsolid = in->startsolid;
	out->fraction = in->fraction;
	VectorCopy( in->endpos, out->endpos );
	out->plane = in->plane;
	out->surfaceFlags = in->surfaceFlags;
	out->contents = in->contents;
	out->entityNum = SV_EngineEntityNumToGame( in->entityNum );
}

/*
 * Fill a versioned Surf extended trace. The destination is zeroed first so no
 * stack garbage crosses the engine/module boundary. Legacy G_TRACE paths are
 * unchanged and still write only native_compat_trace_t / legacy_trace_t.
 */
static void SV_FillSurfTraceEx( surfTraceEx_t *out, const trace_t *in ) {
	uint32_t callerVersion;
	uint32_t callerSize;

	callerVersion = out->version;
	callerSize = out->size;

	Com_Memset( out, 0, sizeof( *out ) );
	out->version = SURF_TRACE_EX_VERSION;
	out->size = sizeof( *out );

	out->allsolid = in->allsolid;
	out->startsolid = in->startsolid;
	out->fraction = in->fraction;
	VectorCopy( in->endpos, out->endpos );
	out->plane = in->plane;
	out->surfaceFlags = in->surfaceFlags;
	out->contents = in->contents;
	out->entityNum = SV_EngineEntityNumToGame( in->entityNum );

	out->planeKind = (int)in->planeKind;
	out->sourceClipnode = in->sourceClipnode;
	out->sourcePlane = in->sourcePlane;
	out->sourceHull = in->sourceHull;
	out->sourceBrush = in->sourceBrush;
	out->sourceFacet = -1;
	out->sourceSide = in->sourceSide;
	out->contentsSource = in->contents;

	if ( in->entityNum != ENTITYNUM_WORLD && in->entityNum != ENTITYNUM_NONE ) {
		out->collisionBackend = TRACE_BACKEND_TRANSFORMED_ENTITY;
	} else if ( in->sourceClipnode >= 0 || in->sourceHull >= 0 ) {
		out->collisionBackend = TRACE_BACKEND_BSP30;
	} else if ( in->sourceBrush >= 0 || in->fraction < 1.0f ) {
		out->collisionBackend = TRACE_BACKEND_Q3_BRUSH;
	} else {
		out->collisionBackend = TRACE_BACKEND_UNKNOWN;
	}

	/* Reject mismatched callers without leaving partial garbage. */
	if ( callerVersion != 0 && callerVersion != SURF_TRACE_EX_VERSION ) {
		out->planeKind = TRACE_PLANE_WORLD_FACE;
		out->sourceClipnode = -1;
		out->sourcePlane = -1;
		out->sourceHull = -1;
		out->sourceBrush = -1;
		out->sourceFacet = -1;
		out->sourceSide = -1;
		out->contentsSource = 0;
		out->collisionBackend = TRACE_BACKEND_UNKNOWN;
	}
	(void)callerSize;
}

/*
===============
SV_AllocEngineEntityNum

Engine sprites/decals must not bump sv.num_entities (qagame array cap from
G_LOCATE_GAME_DATA). Scan upward from MAX_CLIENTS for a free slot.
===============
*/
int SV_AllocEngineEntityNum( void ) {
	int cap;
	int num;

	if ( !sv.gentities || sv.gentitySize <= 0 ) {
		return -1;
	}

	cap = sv_qvmGameEntCap > 0 ? sv_qvmGameEntCap : sv.num_entities;
	if ( cap <= 0 ) {
		cap = RETAIL_QVM_MAX_GENTITIES;
	}
	if ( cap > MAX_GENTITIES - 2 ) {
		cap = MAX_GENTITIES - 2;
	}

	for ( num = cap - 3; num >= MAX_CLIENTS; num-- ) {
		sharedEntity_t *ent = SV_GentityNum( num );

		if ( !ent->r.linked ) {
			return num;
		}
	}

	Com_Printf( S_COLOR_YELLOW "WARNING: no free gentity slot for engine overlay ent (cap=%d)\n", cap );
	return -1;
}

/*
===============
SV_LocateGameData

===============
*/
static void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGameClient ) {

	if ( !gvm->dllHandle ) {
		if ( numGEntities > MAX_GENTITIES ) {
			Com_Error( ERR_DROP, "%s: bad entity count %i", __func__, numGEntities );
		} else {
			if ( (uint32_t) sizeofGEntity_t > gvm->exactDataLength / (uint32_t) numGEntities ) {
				Com_Error( ERR_DROP, "%s: bad entity size %i", __func__, sizeofGEntity_t );	
			} else if ( (byte*)gEnts + (numGEntities * sizeofGEntity_t) > (gvm->dataBase + gvm->exactDataLength) ) {
				Com_Error( ERR_DROP, "%s: entities located out of data segment", __func__ );
			}
		}

		if ( (uint32_t) sizeofGameClient > gvm->exactDataLength / MAX_CLIENTS ) {
			Com_Error( ERR_DROP, "%s: bad game client size %i", __func__, sizeofGameClient );	
		} else if ( (byte*)clients + (sizeofGameClient * MAX_CLIENTS) > gvm->dataBase + gvm->exactDataLength ) {
			Com_Error( ERR_DROP, "%s: clients located out of data segment", __func__ );
		}
	}

	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;

	if ( gvm && !gvm->dllHandle ) {
		if ( sv_qvmGameEntCap <= 0 ) {
			/* Retail qagame.qvm arrays are always 1024; some builds pass a live count. */
			sv_qvmGameEntCap = numGEntities >= RETAIL_QVM_MAX_GENTITIES
				? numGEntities : RETAIL_QVM_MAX_GENTITIES;
		}
		if ( SV_UseLegacyNativeEntityNums() && !sv_qvmEntityMappingLogged ) {
			Com_Printf( "[server] retail entity number mapping active (cap=%d, trap_numGEntities=%d, sizeofGEntity=%d)\n",
				sv_qvmGameEntCap, numGEntities, sizeofGEntity_t );
			sv_qvmEntityMappingLogged = qtrue;
		}
	}
}


/*
===============
SV_GetUsercmd
===============
*/
static void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( (unsigned) clientNum < (unsigned) sv.maxclients ) {
		*cmd = svs.clients[ clientNum ].lastUsercmd;
	} else {
		Com_Error( ERR_DROP, "%s(): bad clientNum: %i", __func__, clientNum );
	}
}

/*
 * Retail qagame/cgame QVMs are compiled with MAX_GENTITIES 1024 (ENTITYNUM_WORLD 1022,
 * ENTITYNUM_NONE 1023). Engine uses MAX_GENTITIES 8192 — translate trap trace/passEntity
 * constants for QVM modules (!dllHandle), not native game DLLs.
 */
static qboolean SV_UseLegacyNativeEntityNums( void ) {
	return gvm && !gvm->dllHandle;
}

static int SV_LegacyEntityNumNone( void ) {
	return RETAIL_QVM_MAX_GENTITIES - 1;
}

static int SV_LegacyEntityNumWorld( void ) {
	return RETAIL_QVM_MAX_GENTITIES - 2;
}

int SV_GameEntityNumToEngine( int entityNum ) {
	if ( !SV_UseLegacyNativeEntityNums() ) {
		return entityNum;
	}

	if ( entityNum == SV_LegacyEntityNumNone() ) {
		return ENTITYNUM_NONE;
	}

	if ( entityNum == SV_LegacyEntityNumWorld() ) {
		return ENTITYNUM_WORLD;
	}

	return entityNum;
}

int SV_EngineEntityNumToGame( int entityNum ) {
	if ( !SV_UseLegacyNativeEntityNums() ) {
		return entityNum;
	}

	if ( entityNum == ENTITYNUM_NONE ) {
		return SV_LegacyEntityNumNone();
	}

	if ( entityNum == ENTITYNUM_WORLD ) {
		return SV_LegacyEntityNumWorld();
	}

	if ( entityNum < 0 || entityNum >= RETAIL_QVM_MAX_GENTITIES ) {
		return SV_LegacyEntityNumNone();
	}

	return entityNum;
}


//==============================================

static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


/*
====================
VM_ArgPtr
====================
*/
static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || gvm == NULL )
		return NULL;

	if ( gvm->entryPoint )
		return (void *)(intValue);
	else
		return (void *)(gvm->dataBase + (intValue & gvm->dataMask));
}


/*
====================
GVM_ArgPtr

exported version
====================
*/
void *GVM_ArgPtr( intptr_t intValue ) 
{
	return VM_ArgPtr( intValue );
}


static qboolean SV_GetValue( char* value, int valueSize, const char* key )
{
	if ( !Q_stricmp( key, "SVF_SELF_PORTAL2_Q3E" ) )
	{
		Com_sprintf( value, valueSize, "%i", SVF_SELF_PORTAL2 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_CVAR_SETDESCRIPTION );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_EngineSpriteShaderIndex" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_SPRITE_SHADER_INDEX );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_EngineSpriteSpawn" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_SPRITE_SPAWN );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_EngineDecalShaderIndex" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DECAL_SHADER_INDEX );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_EngineDecalSpawn" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DECAL_SPAWN );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_TraceEx" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_TRACE_EX );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_Init" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_INIT );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_Shutdown" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_SHUTDOWN );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_RunTurn" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_RUN_TURN );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_GetTurnMsec" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_GET_TURN_MSEC );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_GetCurrentTurn" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_GET_CURRENT_TURN );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_GetPendingCommandCount" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_GET_PENDING_COMMAND_COUNT );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_GetExecutedCommandCount" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_GET_EXECUTED_COMMAND_COUNT );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_GetEntityCount" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_GET_ENTITY_COUNT );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_RTS_ComputeStateHash" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_RTS_COMPUTE_STATE_HASH );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Phys_CharacterCreate" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_CHARACTER_CREATE );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Phys_CharacterMove" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_CHARACTER_MOVE );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Phys_CreateBody" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_CREATEBODY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_DestroyBody" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_DESTROYBODY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_ApplyForce" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_APPLYFORCE );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_ApplyImpulse" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_APPLYIMPULSE );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_GetBodyTransform" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_GETBODYTRANSFORM );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_SetBodyTransform" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_SETBODYTRANSFORM );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_SetBodyVelocity" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_SETBODYVELOCITY );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_RayCast" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_RAYCAST );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_LoadBSPCollision" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_LOADBSPCOLLISION );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_Phys_PmoveCorrect" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_PMOVE_CORRECT );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Phys_CreateRagdoll" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_PHYS_CREATERAGDOLL );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Loc_Lookup" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_LOC_LOOKUP );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_EngineDB_Available" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DB_AVAILABLE );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineDB_Exec" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DB_EXEC );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineDB_QueryOne" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DB_QUERYONE );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineDB_ProfileSet" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DB_PROFILE_SET );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineDB_ProfileGet" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DB_PROFILE_GET );
		return qtrue;
	}
	if ( !Q_stricmp( key, "trap_EngineDB_ProfileDelete" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_ENGINE_DB_PROFILE_DELETE );
		return qtrue;
	}

	return qfalse;
}


/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
static intptr_t SV_GameSystemCalls( intptr_t *args ) {

	// detect infinite loops in QVM code by counting syscalls per VM_Call invocation
	// the stock id 1.32 qagame.qvm has a bug in ClientSpawn() where a do/while(1) loop
	// retrying spawn point selection can loop forever if all spawn points have FL_NO_BOTS
	// set, causing the server to hang at 100% CPU
	if ( gvm->syscallCount >= 1024 * 1024 ) {
		Com_Error( ERR_DROP, "game VM syscall overflow - Loss of control in VM" );
	}
	++gvm->syscallCount;
	com_activeVmLastSyscall = (int)args[0];

	switch( args[0] ) {
	case G_PRINT:
		Com_Printf( "%s", (const char*)VMA(1) );
		return 0;
	case G_ERROR:
		Com_Error( ERR_DROP, "%s", (const char*)VMA(1) );
		return 0;
	case G_MILLISECONDS:
		return Sys_Milliseconds();
	case G_CVAR_REGISTER:
		Cvar_Register( VMA(1), VMA(2), VMA(3), args[4], gvm->privateFlag ); 
		return 0;
	case G_CVAR_UPDATE:
		Cvar_Update( VMA(1), gvm->privateFlag );
		return 0;
	case G_CVAR_SET:
		Cvar_SetSafe( (const char *)VMA(1), (const char *)VMA(2) );
		return 0;
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		return Cvar_VariableIntegerValue( (const char *)VMA(1) );
	case G_CVAR_VARIABLE_STRING_BUFFER:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		Cvar_VariableStringBufferSafe( VMA(1), VMA(2), args[3], gvm->privateFlag );
		return 0;
	case G_ARGC:
		return Cmd_Argc();
	case G_ARGV:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		Cmd_ArgvBuffer( args[1], VMA(2), args[3] );
		return 0;
	case G_SEND_CONSOLE_COMMAND:
		Cbuf_ExecuteText( args[1], VMA(2) );
		return 0;

	case G_FS_FOPEN_FILE:
		return FS_VM_OpenFile( VMA(1), VMA(2), args[3], H_QAGAME );
	case G_FS_READ:
		if ( args[3] == 0 ) // UrT may pass this with args[2]=-1 and cause false bounds check error
			return 0;
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		return FS_VM_ReadFile( VMA(1), args[2], args[3], H_QAGAME );
	case G_FS_WRITE:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		FS_VM_WriteFile( VMA(1), args[2], args[3], H_QAGAME );
		return 0;
	case G_FS_FCLOSE_FILE:
		FS_VM_CloseFile( args[1], H_QAGAME );
		return 0;
	case G_FS_SEEK:
		return FS_VM_SeekFile( args[1], args[2], args[3], H_QAGAME );

	case G_FS_GETFILELIST:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return FS_GetFileList( VMA(1), VMA(2), VMA(3), args[4] );

	case G_LOCATE_GAME_DATA:
		SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );
		return 0;
	case G_DROP_CLIENT:
		SV_GameDropClient( args[1], VMA(2) );
		return 0;
	case G_SEND_SERVER_COMMAND:
		SV_GameSendServerCommand( args[1], VMA(2) );
		return 0;
	case G_LINKENTITY:
		SV_LinkEntity( VMA(1) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( VMA(1) );
		return 0;
	case G_ENTITIES_IN_BOX:
		{
			int count;
			int *list;
			int i;
			int out;

			VM_CHECKBOUNDS( gvm, args[3], args[4] * sizeof( int ) );
			count = SV_AreaEntities( VMA(1), VMA(2), VMA(3), args[4] );
			if ( !SV_UseLegacyNativeEntityNums() || count <= 0 ) {
				return count;
			}
			list = (int *)VMA(3);
			out = 0;
			for ( i = 0; i < count; i++ ) {
				if ( list[i] >= 0 && list[i] < RETAIL_QVM_MAX_GENTITIES ) {
					list[out++] = list[i];
				}
			}
			return out;
		}
	case G_ENTITY_CONTACT:
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), /*int capsule*/ qfalse );
	case G_ENTITY_CONTACTCAPSULE:
		return SV_EntityContact( VMA(1), VMA(2), VMA(3), /*int capsule*/ qtrue );
	case G_TRACE:
		{
			trace_t trace;

			SV_Trace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				SV_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qfalse );
			if ( SV_UseLegacyNativeEntityNums() ) {
				VM_CHECKBOUNDS( gvm, args[1], sizeof( legacy_trace_t ) );
				SV_FillLegacyTrace( (legacy_trace_t *)VMA(1), &trace );
			} else {
				/* Native DLLs (OA etc.) expect retail size without BSP30 fields. */
				VM_CHECKBOUNDS( gvm, args[1], sizeof( native_compat_trace_t ) );
				SV_FillNativeCompatTrace( (native_compat_trace_t *)VMA(1), &trace );
			}
		}
		return 0;
	case G_TRACECAPSULE:
		{
			trace_t trace;

			SV_Trace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				SV_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qtrue );
			if ( SV_UseLegacyNativeEntityNums() ) {
				VM_CHECKBOUNDS( gvm, args[1], sizeof( legacy_trace_t ) );
				SV_FillLegacyTrace( (legacy_trace_t *)VMA(1), &trace );
			} else {
				VM_CHECKBOUNDS( gvm, args[1], sizeof( native_compat_trace_t ) );
				SV_FillNativeCompatTrace( (native_compat_trace_t *)VMA(1), &trace );
			}
		}
		return 0;
	case G_TRACE_EX:
		{
			trace_t trace;
			surfTraceEx_t *out;

			VM_CHECKBOUNDS( gvm, args[1], sizeof( surfTraceEx_t ) );
			out = (surfTraceEx_t *)VMA(1);
			SV_Trace( &trace, VMA(2), VMA(3), VMA(4), VMA(5),
				SV_GameEntityNumToEngine( args[6] ), args[7], /*int capsule*/ qfalse );
			SV_FillSurfTraceEx( out, &trace );
		}
		return 0;
	case G_POINT_CONTENTS:
		return SV_PointContents( VMA(1), SV_GameEntityNumToEngine( args[2] ) );
	case G_SET_BRUSH_MODEL:
		SV_SetBrushModel( VMA(1), VMA(2) );
		return 0;
	case G_IN_PVS:
		return SV_inPVS( VMA(1), VMA(2) );
	case G_IN_PVS_IGNORE_PORTALS:
		return SV_inPVSIgnorePortals( VMA(1), VMA(2) );

	case G_SET_CONFIGSTRING:
		SV_SetConfigstring( args[1], VMA(2) );
		return 0;
	case G_GET_CONFIGSTRING:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetConfigstring( args[1], VMA(2), args[3] );
		return 0;
	case G_SET_USERINFO:
		SV_SetUserinfo( args[1], VMA(2) );
		return 0;
	case G_GET_USERINFO:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetUserinfo( args[1], VMA(2), args[3] );
		return 0;
	case G_GET_SERVERINFO:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		SV_GetServerinfo( VMA(1), args[2] );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( VMA(1), args[2] );
		return 0;
	case G_AREAS_CONNECTED:
		return CM_AreasConnected( args[1], args[2] );

	case G_BOT_ALLOCATE_CLIENT:
		return SV_BotAllocateClient();
	case G_BOT_FREE_CLIENT:
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( usercmd_t ) );
		SV_GetUsercmd( args[1], VMA(2) );
		return 0;
	case G_GET_ENTITY_TOKEN:
		{
			const char *s = COM_Parse( (const char **)&sv.entityParsePoint );
			VM_CHECKBOUNDS( gvm, args[1], args[2] );
			//Q_strncpyz( VMA(1), s, args[2] );
			// we can't use our optimized Q_strncpyz() function
			// because of uninitialized memory bug in defrag mod
			{
				char *dst = (char*)VMA(1);
				const int size = args[2]-1;
				if ( size >= 0 ) {
					Q_strncpy( dst, s, size );
					dst[size] = '\0';
				}
			}
			if ( !sv.entityParsePoint && s[0] == '\0' ) {
				return qfalse;
			} else {
				return qtrue;
			}
		}

	case G_DEBUG_POLYGON_CREATE:
		return BotImport_DebugPolygonCreate( args[1], args[2], VMA(3) );
	case G_DEBUG_POLYGON_DELETE:
		BotImport_DebugPolygonDelete( args[1] );
		return 0;
	case G_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case G_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

		//====================================

	case BOTLIB_SETUP:
		return SV_BotLibSetup();
	case BOTLIB_SHUTDOWN:
		return SV_BotLibShutdown();
	case BOTLIB_LIBVAR_SET:
		return botlib_export->BotLibVarSet( VMA(1), VMA(2) );
	case BOTLIB_LIBVAR_GET:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		return botlib_export->BotLibVarGet( VMA(1), VMA(2), args[3] );

	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
		return botlib_export->PC_AddGlobalDefine( VMA(1) );
	case BOTLIB_PC_LOAD_SOURCE:
		return botlib_export->PC_LoadSourceHandle( VMA(1) );
	case BOTLIB_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( pc_token_t ) );
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
		return botlib_export->PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case BOTLIB_START_FRAME:
		return botlib_export->BotLibStartFrame( VMF(1) );
	case BOTLIB_LOAD_MAP:
		return SV_BotLoadMap( VMA(1) );
	case BOTLIB_UPDATENTITY:
		return botlib_export->BotLibUpdateEntity( args[1], VMA(2) );
	case BOTLIB_TEST:
		return botlib_export->Test( args[1], VMA(2), VMA(3), VMA(4) );

	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		return SV_BotGetConsoleMessage( args[1], VMA(2), args[3] );
	case BOTLIB_USER_COMMAND:
		{
			unsigned clientNum = args[1];
			if ( clientNum < (unsigned) sv.maxclients )
			{
				SV_ClientThink( &svs.clients[ clientNum ], VMA(2) );
			}
		}
		return 0;

	case BOTLIB_AAS_BBOX_AREAS:
		return botlib_export->aas.AAS_BBoxAreas( VMA(1), VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_AREA_INFO:
		return botlib_export->aas.AAS_AreaInfo( args[1], VMA(2) );
	case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
		return botlib_export->aas.AAS_AlternativeRouteGoals( VMA(1), args[2], VMA(3), args[4], args[5], VMA(6), args[7], args[8] );
	case BOTLIB_AAS_ENTITY_INFO:
		botlib_export->aas.AAS_EntityInfo( args[1], VMA(2) );
		return 0;

	case BOTLIB_AAS_INITIALIZED:
		return botlib_export->aas.AAS_Initialized();
	case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
		botlib_export->aas.AAS_PresenceTypeBoundingBox( args[1], VMA(2), VMA(3) );
		return 0;
	case BOTLIB_AAS_TIME:
		return FloatAsInt( botlib_export->aas.AAS_Time() );

	case BOTLIB_AAS_POINT_AREA_NUM:
		return botlib_export->aas.AAS_PointAreaNum( VMA(1) );
	case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
		return botlib_export->aas.AAS_PointReachabilityAreaIndex( VMA(1) );
	case BOTLIB_AAS_TRACE_AREAS:
		return botlib_export->aas.AAS_TraceAreas( VMA(1), VMA(2), VMA(3), VMA(4), args[5] );

	case BOTLIB_AAS_POINT_CONTENTS:
		return botlib_export->aas.AAS_PointContents( VMA(1) );
	case BOTLIB_AAS_NEXT_BSP_ENTITY:
		return botlib_export->aas.AAS_NextBSPEntity( args[1] );
	case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return botlib_export->aas.AAS_ValueForBSPEpairKey( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_VectorForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_FloatForBSPEpairKey( args[1], VMA(2), VMA(3) );
	case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
		return botlib_export->aas.AAS_IntForBSPEpairKey( args[1], VMA(2), VMA(3) );

	case BOTLIB_AAS_AREA_REACHABILITY:
		return botlib_export->aas.AAS_AreaReachability( args[1] );

	case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
		return botlib_export->aas.AAS_AreaTravelTimeToGoalArea( args[1], VMA(2), args[3], args[4] );
	case BOTLIB_AAS_ENABLE_ROUTING_AREA:
		return botlib_export->aas.AAS_EnableRoutingArea( args[1], args[2] );
	case BOTLIB_AAS_PREDICT_ROUTE:
		return botlib_export->aas.AAS_PredictRoute( VMA(1), args[2], VMA(3), args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11] );

	case BOTLIB_AAS_SWIMMING:
		return botlib_export->aas.AAS_Swimming( VMA(1) );
	case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
		return botlib_export->aas.AAS_PredictClientMovement( VMA(1), args[2], VMA(3), args[4], args[5],
			VMA(6), VMA(7), args[8], args[9], VMF(10), args[11], args[12], args[13] );

	case BOTLIB_EA_SAY:
		botlib_export->ea.EA_Say( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_SAY_TEAM:
		botlib_export->ea.EA_SayTeam( args[1], VMA(2) );
		return 0;
	case BOTLIB_EA_COMMAND:
		botlib_export->ea.EA_Command( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_ACTION:
		botlib_export->ea.EA_Action( args[1], args[2] );
		return 0;
	case BOTLIB_EA_GESTURE:
		botlib_export->ea.EA_Gesture( args[1] );
		return 0;
	case BOTLIB_EA_TALK:
		botlib_export->ea.EA_Talk( args[1] );
		return 0;
	case BOTLIB_EA_ATTACK:
		botlib_export->ea.EA_Attack( args[1] );
		return 0;
	case BOTLIB_EA_USE:
		botlib_export->ea.EA_Use( args[1] );
		return 0;
	case BOTLIB_EA_RESPAWN:
		botlib_export->ea.EA_Respawn( args[1] );
		return 0;
	case BOTLIB_EA_CROUCH:
		botlib_export->ea.EA_Crouch( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_UP:
		botlib_export->ea.EA_MoveUp( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
		botlib_export->ea.EA_MoveDown( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
		botlib_export->ea.EA_MoveForward( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_BACK:
		botlib_export->ea.EA_MoveBack( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
		botlib_export->ea.EA_MoveLeft( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
		botlib_export->ea.EA_MoveRight( args[1] );
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
		botlib_export->ea.EA_SelectWeapon( args[1], args[2] );
		return 0;
	case BOTLIB_EA_JUMP:
		botlib_export->ea.EA_Jump( args[1] );
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
		botlib_export->ea.EA_DelayedJump( args[1] );
		return 0;
	case BOTLIB_EA_MOVE:
		botlib_export->ea.EA_Move( args[1], VMA(2), VMF(3) );
		return 0;
	case BOTLIB_EA_VIEW:
		botlib_export->ea.EA_View( args[1], VMA(2) );
		return 0;

	case BOTLIB_EA_END_REGULAR:
		botlib_export->ea.EA_EndRegular( args[1], VMF(2) );
		return 0;
	case BOTLIB_EA_GET_INPUT:
		botlib_export->ea.EA_GetInput( args[1], VMF(2), VMA(3) );
		return 0;
	case BOTLIB_EA_RESET_INPUT:
		botlib_export->ea.EA_ResetInput( args[1] );
		return 0;

	case BOTLIB_AI_LOAD_CHARACTER:
		return botlib_export->ai.BotLoadCharacter( VMA(1), VMF(2) );
	case BOTLIB_AI_FREE_CHARACTER:
		botlib_export->ai.BotFreeCharacter( args[1] );
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_Float( args[1], args[2] ) );
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_BFloat( args[1], args[2], VMF(3), VMF(4) ) );
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
		return botlib_export->ai.Characteristic_Integer( args[1], args[2] );
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		return botlib_export->ai.Characteristic_BInteger( args[1], args[2], args[3], args[4] );
	case BOTLIB_AI_CHARACTERISTIC_STRING:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		botlib_export->ai.Characteristic_String( args[1], args[2], VMA(3), args[4] );
		return 0;

	case BOTLIB_AI_ALLOC_CHAT_STATE:
		return botlib_export->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
		botlib_export->ai.BotFreeChatState( args[1] );
		return 0;
	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
		botlib_export->ai.BotQueueConsoleMessage( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		botlib_export->ai.BotRemoveConsoleMessage( args[1], args[2] );
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNextConsoleMessage( args[1], VMA(2) );
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNumConsoleMessages( args[1] );
	case BOTLIB_AI_INITIAL_CHAT:
		botlib_export->ai.BotInitialChat( args[1], VMA(2), args[3], VMA(4), VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11) );
		return 0;
	case BOTLIB_AI_NUM_INITIAL_CHATS:
		return botlib_export->ai.BotNumInitialChats( args[1], VMA(2) );
	case BOTLIB_AI_REPLY_CHAT:
		return botlib_export->ai.BotReplyChat( args[1], VMA(2), args[3], args[4], VMA(5), VMA(6), VMA(7), VMA(8), VMA(9), VMA(10), VMA(11), VMA(12) );
	case BOTLIB_AI_CHAT_LENGTH:
		return botlib_export->ai.BotChatLength( args[1] );
	case BOTLIB_AI_ENTER_CHAT:
		botlib_export->ai.BotEnterChat( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_GET_CHAT_MESSAGE:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		botlib_export->ai.BotGetChatMessage( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_STRING_CONTAINS:
		return botlib_export->ai.StringContains( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_FIND_MATCH:
		return botlib_export->ai.BotFindMatch( VMA(1), VMA(2), args[3] );
	case BOTLIB_AI_MATCH_VARIABLE:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		botlib_export->ai.BotMatchVariable( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_UNIFY_WHITE_SPACES:
		botlib_export->ai.UnifyWhiteSpaces( VMA(1) );
		return 0;
	case BOTLIB_AI_REPLACE_SYNONYMS:
		botlib_export->ai.BotReplaceSynonyms( VMA(1), VM_DATA_GUARD_SIZE, args[2] );
		return 0;
	case BOTLIB_AI_LOAD_CHAT_FILE:
		return botlib_export->ai.BotLoadChatFile( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_SET_CHAT_GENDER:
		botlib_export->ai.BotSetChatGender( args[1], args[2] );
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME:
		botlib_export->ai.BotSetChatName( args[1], VMA(2), args[3] );
		return 0;

	case BOTLIB_AI_RESET_GOAL_STATE:
		botlib_export->ai.BotResetGoalState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
		botlib_export->ai.BotResetAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		botlib_export->ai.BotRemoveFromAvoidGoals( args[1], args[2] );
		return 0;
	case BOTLIB_AI_PUSH_GOAL:
		botlib_export->ai.BotPushGoal( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_POP_GOAL:
		botlib_export->ai.BotPopGoal( args[1] );
		return 0;
	case BOTLIB_AI_EMPTY_GOAL_STACK:
		botlib_export->ai.BotEmptyGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
		botlib_export->ai.BotDumpAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
		botlib_export->ai.BotDumpGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_GOAL_NAME:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		botlib_export->ai.BotGoalName( args[1], VMA(2), args[3] );
		return 0;
	case BOTLIB_AI_GET_TOP_GOAL:
		return botlib_export->ai.BotGetTopGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_SECOND_GOAL:
		return botlib_export->ai.BotGetSecondGoal( args[1], VMA(2) );
	case BOTLIB_AI_CHOOSE_LTG_ITEM:
		return botlib_export->ai.BotChooseLTGItem( args[1], VMA(2), VMA(3), args[4] );
	case BOTLIB_AI_CHOOSE_NBG_ITEM:
		return botlib_export->ai.BotChooseNBGItem( args[1], VMA(2), VMA(3), args[4], VMA(5), VMF(6) );
	case BOTLIB_AI_TOUCHING_GOAL:
		return botlib_export->ai.BotTouchingGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
		return botlib_export->ai.BotItemGoalInVisButNotVisible( args[1], VMA(2), VMA(3), VMA(4) );
	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
		return botlib_export->ai.BotGetLevelItemGoal( args[1], VMA(2), VMA(3) );
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
		return botlib_export->ai.BotGetNextCampSpotGoal( args[1], VMA(2) );
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
		return botlib_export->ai.BotGetMapLocationGoal( VMA(1), VMA(2) );
	case BOTLIB_AI_AVOID_GOAL_TIME:
		return FloatAsInt( botlib_export->ai.BotAvoidGoalTime( args[1], args[2] ) );
	case BOTLIB_AI_SET_AVOID_GOAL_TIME:
		botlib_export->ai.BotSetAvoidGoalTime( args[1], args[2], VMF(3));
		return 0;
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
		botlib_export->ai.BotInitLevelItems();
		return 0;
	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		botlib_export->ai.BotUpdateEntityItems();
		return 0;
	case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
		return botlib_export->ai.BotLoadItemWeights( args[1], VMA(2) );
	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
		botlib_export->ai.BotFreeItemWeights( args[1] );
		return 0;
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotInterbreedGoalFuzzyLogic( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotSaveGoalFuzzyLogic( args[1], VMA(2) );
		return 0;
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotMutateGoalFuzzyLogic( args[1], VMF(2) );
		return 0;
	case BOTLIB_AI_ALLOC_GOAL_STATE:
		return botlib_export->ai.BotAllocGoalState( args[1] );
	case BOTLIB_AI_FREE_GOAL_STATE:
		botlib_export->ai.BotFreeGoalState( args[1] );
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
		botlib_export->ai.BotResetMoveState( args[1] );
		return 0;
	case BOTLIB_AI_ADD_AVOID_SPOT:
		botlib_export->ai.BotAddAvoidSpot( args[1], VMA(2), VMF(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_TO_GOAL:
		botlib_export->ai.BotMoveToGoal( VMA(1), args[2], VMA(3), args[4] );
		return 0;
	case BOTLIB_AI_MOVE_IN_DIRECTION:
		return botlib_export->ai.BotMoveInDirection( args[1], VMA(2), VMF(3), args[4] );
	case BOTLIB_AI_RESET_AVOID_REACH:
		botlib_export->ai.BotResetAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
		botlib_export->ai.BotResetLastAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA:
		return botlib_export->ai.BotReachabilityArea( VMA(1), args[2] );
	case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
		return botlib_export->ai.BotMovementViewTarget( args[1], VMA(2), args[3], VMF(4), VMA(5) );
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
		return botlib_export->ai.BotPredictVisiblePosition( VMA(1), args[2], VMA(3), args[4], VMA(5) );
	case BOTLIB_AI_ALLOC_MOVE_STATE:
		return botlib_export->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
		botlib_export->ai.BotFreeMoveState( args[1] );
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE:
		botlib_export->ai.BotInitMoveState( args[1], VMA(2) );
		return 0;

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		return botlib_export->ai.BotChooseBestFightWeapon( args[1], VMA(2) );
	case BOTLIB_AI_GET_WEAPON_INFO:
		botlib_export->ai.BotGetWeaponInfo( args[1], args[2], VMA(3) );
		return 0;
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
		return botlib_export->ai.BotLoadWeaponWeights( args[1], VMA(2) );
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
		return botlib_export->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
		botlib_export->ai.BotFreeWeaponState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
		botlib_export->ai.BotResetWeaponState( args[1] );
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
		return botlib_export->ai.GeneticParentsAndChildSelection(args[1], VMA(2), VMA(3), VMA(4), VMA(5));

	// shared syscalls

	case TRAP_MEMSET:
		VM_CHECKBOUNDS( gvm, args[1], args[3] );
		Com_Memset( VMA(1), args[2], args[3] );
		return args[1];

	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( gvm, args[1], args[2], args[3] );
		Com_Memcpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( gvm, args[1], args[3] );
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

	case G_MATRIXMULTIPLY:
		MatrixMultiply( VMA(1), VMA(2), VMA(3) );
		return 0;

	case G_ANGLEVECTORS:
		AngleVectors( VMA(1), VMA(2), VMA(3), VMA(4) );
		return 0;

	case G_PERPENDICULARVECTOR:
		PerpendicularVector( VMA(1), VMA(2) );
		return 0;

	case G_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );

	case G_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );

	case G_TESTPRINTINT:
		return Com_sprintf( VMA(1), MAX_STRING_CHARS, "%i", (int)args[2] );

	case G_TESTPRINTFLOAT:
		return Com_sprintf( VMA(1), MAX_STRING_CHARS, "%f", VMF(2) );

	case G_CVAR_SETDESCRIPTION:
		Cvar_SetDescription2( (const char*)VMA(1), (const char*)VMA(2) );
		return 0;

	case G_TRAP_GETVALUE:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		return SV_GetValue( VMA(1), args[2], VMA(3) );

	case G_ENGINE_SPRITE_SHADER_INDEX:
		return SV_EngineSpriteShaderIndex( (const char *)VMA( 1 ) );

	case G_ENGINE_SPRITE_SPAWN: {
		engineSpriteMapDef_t def;

		Com_Memset( &def, 0, sizeof( def ) );
		def.type = (engineSpriteType_t)args[1];
		Q_strncpyz( def.shader, (const char *)VMA( 2 ), sizeof( def.shader ) );
		def.origin[0] = VMF( 3 );
		def.origin[1] = VMF( 4 );
		def.origin[2] = VMF( 5 );
		def.radius = VMF( 6 );
		def.rotation = VMF( 7 );
		def.cols = args[8] > 0 ? (int)args[8] : 1;
		def.rows = args[9] > 0 ? (int)args[9] : 1;
		def.fps = VMF( 10 );
		if ( def.fps <= 0.0f ) {
			def.fps = 8.0f;
		}
		return SV_EngineSprite_SpawnFromDef( &def );
	}

	case G_ENGINE_DECAL_SHADER_INDEX:
		return SV_EngineDecalShaderIndex( (const char *)VMA( 1 ) );

	case G_ENGINE_DECAL_SPAWN: {
		engineDecalMapDef_t def;

		Com_Memset( &def, 0, sizeof( def ) );
		Q_strncpyz( def.shader, (const char *)VMA( 1 ), sizeof( def.shader ) );
		def.origin[0] = VMF( 2 );
		def.origin[1] = VMF( 3 );
		def.origin[2] = VMF( 4 );
		def.radius = VMF( 5 );
		def.pitch = VMF( 6 );
		def.yaw = VMF( 7 );
		def.fadeSec = VMF( 8 );
		return SV_EngineDecal_SpawnFromDef( &def );
	}

	case G_PHYS_CHARACTER_CREATE:
		return Phys_CharacterCreate( VMF( 1 ), VMF( 2 ), VMF( 3 ) );

	case G_PHYS_CHARACTER_MOVE:
		return Phys_CharacterMove( (int)args[1], VMA( 2 ), VMF( 3 ), args[4] ? qtrue : qfalse );

	case G_PHYS_CREATEBODY:
		return Phys_CreateBody( (const physBodyDef_t *)VMA( 1 ) );

	case G_PHYS_DESTROYBODY:
		Phys_DestroyBody( (physBodyHandle_t)args[1] );
		return 0;

	case G_PHYS_APPLYFORCE:
		Phys_ApplyForce( (physBodyHandle_t)args[1], (const float *)VMA( 2 ), (const float *)VMA( 3 ) );
		return 0;

	case G_PHYS_APPLYIMPULSE:
		Phys_ApplyImpulse( (physBodyHandle_t)args[1], (const float *)VMA( 2 ), (const float *)VMA( 3 ) );
		return 0;

	case G_PHYS_GETBODYTRANSFORM:
		Phys_GetBodyTransform( (physBodyHandle_t)args[1], (physTransform_t *)VMA( 2 ) );
		return 0;

	case G_PHYS_SETBODYTRANSFORM:
		Phys_SetBodyTransform( (physBodyHandle_t)args[1], (const float *)VMA( 2 ), (const float *)VMA( 3 ) );
		return 0;

	case G_PHYS_SETBODYVELOCITY:
		Phys_SetBodyVelocity( (physBodyHandle_t)args[1], (const float *)VMA( 2 ), (const float *)VMA( 3 ) );
		return 0;

	case G_PHYS_RAYCAST:
		return Phys_RayCast( (const float *)VMA( 1 ), (const float *)VMA( 2 ), (physRayResult_t *)VMA( 3 ) );

	case G_PHYS_LOADBSPCOLLISION:
		return Phys_LoadBSPCollision();

	case G_PHYS_PMOVE_CORRECT:
		return Phys_PmoveCorrect( (float *)VMA( 1 ), (float *)VMA( 2 ), VMF( 3 ), VMF( 4 ), VMF( 5 ) );

	case G_PHYS_CREATERAGDOLL: {
		physBoundRagdoll_t bound;
		vec3_t origin;
		origin[0] = VMF( 1 );
		origin[1] = VMF( 2 );
		origin[2] = VMF( 3 );
		if ( !Phys_RagdollSpawnBound( (const char *)VMA( 4 ), origin, &bound ) ) {
			return -1;
		}
		return bound.ragdoll;
	}

	case G_LOC_LOOKUP:
		return Com_Loc_Lookup( (const char *)VMA( 1 ), VMA( 2 ), (int)args[3] );

	case G_ENGINE_DB_AVAILABLE:
		EngineDB_Init();
		return EngineDB_IsAvailable() ? 1 : 0;

	case G_ENGINE_DB_EXEC:
		return EngineDB_Exec( (const char *)VMA( 1 ) ) ? 1 : 0;

	case G_ENGINE_DB_QUERYONE:
		return EngineDB_QueryOne( (const char *)VMA( 1 ), VMA( 2 ), (int)args[3] ) ? 1 : 0;

	case G_ENGINE_DB_PROFILE_SET:
		return EngineDB_ProfileSet( (const char *)VMA( 1 ), (const char *)VMA( 2 ) ) ? 1 : 0;

	case G_ENGINE_DB_PROFILE_GET:
		return EngineDB_ProfileGet( (const char *)VMA( 1 ), VMA( 2 ), (int)args[3] ) ? 1 : 0;

	case G_ENGINE_DB_PROFILE_DELETE:
		return EngineDB_ProfileDelete( (const char *)VMA( 1 ) ) ? 1 : 0;

	case G_RTS_INIT:
		RTS_Init();
		return 0;

	case G_RTS_SHUTDOWN:
		RTS_Shutdown();
		return 0;

	case G_RTS_RUN_TURN:
		RTS_RunTurn( args[1] );
		return 0;

	case G_RTS_GET_TURN_MSEC:
		return RTS_GetTurnMsec();

	case G_RTS_GET_CURRENT_TURN:
		return RTS_GetCurrentTurn();

	case G_RTS_GET_PENDING_COMMAND_COUNT:
		return RTS_GetPendingCommandCount();

	case G_RTS_GET_EXECUTED_COMMAND_COUNT:
		return RTS_GetExecutedCommandCount();

	case G_RTS_GET_ENTITY_COUNT:
		return RTS_GetEntityCount();

	case G_RTS_COMPUTE_STATE_HASH:
		return (intptr_t)RTS_ComputeStateHash();

	default:
		Com_Error( ERR_DROP, "Bad game system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
SV_DllSyscall
====================
*/
static intptr_t QDECL SV_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t	args[14]; // max.count for qagame
	va_list	ap;
	int i;

	args[0] = arg;
	va_start( ap, arg );
	for (i = 1; (size_t) i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return SV_GameSystemCalls( args );
#else
	return SV_GameSystemCalls( &arg );
#endif
}


/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	sv_qvmGameEntCap = 0;
	sv_qvmEntityMappingLogged = qfalse;
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, 1, GAME_SHUTDOWN, qfalse );
	VM_Free( gvm );
	gvm = NULL;
	FS_VM_CloseFiles( H_QAGAME );
}


/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( qboolean restart ) {
	int		i;

	// start the entity parsing at the beginning
	sv.entityParsePoint = CM_EntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	// now done before GAME_INIT call
	for ( i = 0; i < sv.maxclients; i++ ) {
		svs.clients[i].gentity = NULL;
	}
	
	// use the current msec count for a random seed
	// init for this gamestate
	VM_Call( gvm, 3, GAME_INIT, sv.time, Com_Milliseconds(), restart );
}


/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, 1, GAME_SHUTDOWN, qtrue );

	// do a restart instead of a free
	gvm = VM_Restart( gvm );
	if ( !gvm ) {
		Com_Error( ERR_DROP, "VM_Restart on game failed" );
	}

	SV_InitGameVM( qtrue );

	SV_EnsureGameVersionConfigstring();

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
static qboolean SV_IsOpenArenaGame( void ) {
	const char *fs_game;
	const char *fs_basegame;

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

	return qfalse;
}

void SV_InitGameProgs( void ) {
	cvar_t	*var;
	/* Temporary: bots will run in VM. */
	extern int	bot_enable;
	int interpret;

	var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	}
	else {
		bot_enable = 0;
	}

	// load the dll or bytecode
	interpret = Cvar_VariableIntegerValue( "vm_game" );
	if ( SV_IsOpenArenaGame() ) {
		interpret = VMI_NATIVE;
	}
	gvm = VM_Create( VM_GAME, SV_GameSystemCalls, SV_DllSyscall, interpret );
	if ( !gvm ) {
		Com_Error( ERR_DROP, "VM_Create on game failed" );
	}

	SV_InitGameVM( qfalse );

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	const char *cmd;

	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	cmd = Cmd_Argv( 0 );

	// Intercept auth_ok to stash the canonical name on client_t before
	// the QVM sees the command. Argv layout matches Trinity's
	// trinity_auth_ok: slot, <opaque>, lockedName (argc >= 4).
	// The QVM may still process the command for announcements; the
	// engine owns the name-lock field.
	if ( !Q_stricmp( cmd, "auth_ok" ) && Cmd_Argc() >= 4 ) {
		int slot = atoi( Cmd_Argv( 1 ) );
		if ( slot >= 0 && slot < sv_maxclients->integer ) {
			Q_strncpyz( svs.clients[slot].lockedName,
			            Cmd_Argv( 3 ),
			            sizeof( svs.clients[slot].lockedName ) );
			SV_UserinfoChanged( &svs.clients[slot], qtrue, qfalse );
		}
	}

	// Clear the name lock on auth failure so the client can /name freely.
	if ( !Q_stricmp( cmd, "auth_fail" ) && Cmd_Argc() >= 2 ) {
		int slot = atoi( Cmd_Argv( 1 ) );
		if ( slot >= 0 && slot < sv_maxclients->integer ) {
			svs.clients[slot].lockedName[0] = '\0';
		}
	}

	return VM_Call( gvm, 0, GAME_CONSOLE_COMMAND );
}
