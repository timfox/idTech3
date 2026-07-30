/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side RTS render demo bridge.
===========================================================================
*/

#include "client.h"
#include "cl_ref.h"
#include "../../../modules/rts/rts_public.h"

#define CL_RTS_DEMO_MAX_ENTITIES 32

static cvar_t *cl_rtsDemo;
static cvar_t *cl_rtsDemoModel;
static cvar_t *cl_rtsDemoCount;
static cvar_t *cl_rtsDemoScale;
static cvar_t *cl_rtsDemoSpacing;
static cvar_t *cl_rtsDemoZ;
static cvar_t *cl_rtsDemoMove;

static qboolean s_rtsDemoStarted = qfalse;
static qhandle_t s_rtsDemoModel = 0;
static char s_rtsDemoModelPath[MAX_QPATH];
static int s_rtsDemoLastStepTime = 0;

static int CL_RTSDemo_EntityCount( void )
{
	int count = cl_rtsDemoCount ? cl_rtsDemoCount->integer : 6;

	if ( count < 1 ) {
		count = 1;
	}
	if ( count > CL_RTS_DEMO_MAX_ENTITIES ) {
		count = CL_RTS_DEMO_MAX_ENTITIES;
	}
	return count;
}

static qhandle_t CL_RTSDemo_RegisterModel( void )
{
	const char *path = cl_rtsDemoModel ? cl_rtsDemoModel->string : "models/rts/0ad_jav2.dae";

	if ( !re.RegisterModel ) {
		return 0;
	}
	if ( s_rtsDemoModel && !Q_stricmp( s_rtsDemoModelPath, path ) ) {
		return s_rtsDemoModel;
	}

	s_rtsDemoModel = re.RegisterModel( path );
	Q_strncpyz( s_rtsDemoModelPath, path, sizeof( s_rtsDemoModelPath ) );
	if ( !s_rtsDemoModel ) {
		Com_Printf( S_COLOR_YELLOW "rts_demo: failed to register model '%s'\n", path );
	}
	return s_rtsDemoModel;
}

static void CL_RTSDemo_RefreshEntityModels( qhandle_t model )
{
	const char *path = cl_rtsDemoModel ? cl_rtsDemoModel->string : "models/rts/0ad_jav2.dae";
	int count;
	int i;

	if ( !model ) {
		return;
	}

	count = RTS_GetEntityCount();
	if ( count > CL_RTS_DEMO_MAX_ENTITIES ) {
		count = CL_RTS_DEMO_MAX_ENTITIES;
	}
	for ( i = 0; i < count; ++i ) {
		rtsEntityId_t entityId = (rtsEntityId_t)( i + 1 );

		if ( !RTS_GetEntityModelHandle( entityId ) ) {
			RTS_SetEntityModel( entityId, path, model );
		}
	}
}

static void CL_RTSDemo_PostCreate( int entityIndex, int x, int y )
{
	rtsCommand_t cmd;

	Com_Memset( &cmd, 0, sizeof( cmd ) );
	cmd.type = RTS_COMMAND_CREATE_ENTITY;
	cmd.turn = RTS_GetCurrentTurn() + 1;
	cmd.playerId = RTS_OWNER_PLAYER1;
	cmd.sequence = entityIndex + 1;
	cmd.targetX = x;
	cmd.targetY = y;
	RTS_PostCommand( &cmd );
}

static void CL_RTSDemo_PostMove( rtsEntityId_t entityId, int sequence, int x, int y )
{
	rtsCommand_t cmd;

	Com_Memset( &cmd, 0, sizeof( cmd ) );
	cmd.type = RTS_COMMAND_SET_POSITION;
	cmd.turn = RTS_GetCurrentTurn() + 1;
	cmd.playerId = RTS_OWNER_PLAYER1;
	cmd.sequence = sequence;
	cmd.entityId = entityId;
	cmd.targetX = x;
	cmd.targetY = y;
	RTS_PostCommand( &cmd );
}

static void CL_RTSDemo_Start( void )
{
	int i;
	int count = CL_RTSDemo_EntityCount();
	float spacing = cl_rtsDemoSpacing ? cl_rtsDemoSpacing->value : 96.0f;
	qhandle_t model = CL_RTSDemo_RegisterModel();

	RTS_Init();
	RTS_SetDefaultModelForOwner( RTS_OWNER_PLAYER1,
		cl_rtsDemoModel ? cl_rtsDemoModel->string : "models/rts/0ad_jav2.dae", model );

	for ( i = 0; i < count; ++i ) {
		int col = i % 4;
		int row = i / 4;
		CL_RTSDemo_PostCreate( i, (int)( ( col - 1.5f ) * spacing ), (int)( row * spacing ) );
	}
	RTS_RunTurn( 50 );
	s_rtsDemoStarted = qtrue;
	s_rtsDemoLastStepTime = cls.realtime;
	Cvar_Set( "cl_rtsDemo", "1" );
	Com_Printf( "rts_demo: started %d entities model='%s' handle=%d\n",
		count, cl_rtsDemoModel ? cl_rtsDemoModel->string : "", model );
}

static void CL_RTSDemo_Stop( void )
{
	RTS_Shutdown();
	s_rtsDemoStarted = qfalse;
	s_rtsDemoLastStepTime = 0;
	Cvar_Set( "cl_rtsDemo", "0" );
	Com_Printf( "rts_demo: stopped\n" );
}

static void CL_RTSDemo_Start_f( void )
{
	if ( Cmd_Argc() > 1 ) {
		Cvar_Set( "cl_rtsDemoModel", Cmd_Argv( 1 ) );
		s_rtsDemoModel = 0;
		s_rtsDemoModelPath[0] = '\0';
	}
	CL_RTSDemo_Start();
}

static void CL_RTSDemo_Stop_f( void )
{
	CL_RTSDemo_Stop();
}

static void CL_RTSDemo_Status_f( void )
{
	Com_Printf( "rts_demo: enabled=%d started=%d entities=%d model='%s' handle=%d turn=%d pending=%d\n",
		cl_rtsDemo ? cl_rtsDemo->integer : 0,
		s_rtsDemoStarted,
		RTS_GetEntityCount(),
		cl_rtsDemoModel ? cl_rtsDemoModel->string : "",
		s_rtsDemoModel,
		RTS_GetCurrentTurn(),
		RTS_GetPendingCommandCount() );
}

static void CL_RTSDemo_UpdateSimulation( void )
{
	int i;
	int count;
	int elapsed;
	float spacing;

	if ( !cl_rtsDemo || !cl_rtsDemo->integer ) {
		if ( s_rtsDemoStarted ) {
			RTS_Shutdown();
			s_rtsDemoStarted = qfalse;
		}
		return;
	}
	if ( !s_rtsDemoStarted || RTS_GetEntityCount() <= 0 ) {
		CL_RTSDemo_Start();
	}
	if ( !cl_rtsDemoMove || !cl_rtsDemoMove->integer ) {
		return;
	}
	elapsed = cls.realtime - s_rtsDemoLastStepTime;
	if ( elapsed < 100 ) {
		return;
	}

	count = RTS_GetEntityCount();
	if ( count > CL_RTS_DEMO_MAX_ENTITIES ) {
		count = CL_RTS_DEMO_MAX_ENTITIES;
	}
	spacing = cl_rtsDemoSpacing ? cl_rtsDemoSpacing->value : 96.0f;
	for ( i = 0; i < count; ++i ) {
		float phase = (float)cls.realtime * 0.001f + (float)i * 0.75f;
		int col = i % 4;
		int row = i / 4;
		int x = (int)( ( col - 1.5f ) * spacing + sinf( phase ) * spacing * 0.35f );
		int y = (int)( row * spacing + cosf( phase * 0.8f ) * spacing * 0.35f );
		CL_RTSDemo_PostMove( i + 1, i + 1, x, y );
	}
	RTS_RunTurn( elapsed );
	s_rtsDemoLastStepTime = cls.realtime;
}

void CL_RTSDemo_AddRefEntitiesToScene( void )
{
	rtsRenderEntity_t ents[CL_RTS_DEMO_MAX_ENTITIES];
	int count;
	int i;
	float zOrigin;
	float unitScale;
	qhandle_t model;

	CL_RTSDemo_UpdateSimulation();
	if ( !cl_rtsDemo || !cl_rtsDemo->integer || !s_rtsDemoStarted ) {
		return;
	}

	model = CL_RTSDemo_RegisterModel();
	if ( model ) {
		RTS_SetDefaultModelForOwner( RTS_OWNER_PLAYER1,
			cl_rtsDemoModel ? cl_rtsDemoModel->string : "models/rts/0ad_jav2.dae", model );
		CL_RTSDemo_RefreshEntityModels( model );
	}

	zOrigin = cl_rtsDemoZ ? cl_rtsDemoZ->value : 32.0f;
	unitScale = cl_rtsDemoScale ? cl_rtsDemoScale->value : 1.0f;
	count = RTS_BuildRenderEntities( ents, CL_RTS_DEMO_MAX_ENTITIES, zOrigin, unitScale );
	if ( count > CL_RTS_DEMO_MAX_ENTITIES ) {
		count = CL_RTS_DEMO_MAX_ENTITIES;
	}
	for ( i = 0; i < count; ++i ) {
		refEntity_t ref;
		vec3_t angles;

		if ( !ents[i].modelHandle ) {
			continue;
		}
		Com_Memset( &ref, 0, sizeof( ref ) );
		ref.reType = RT_MODEL;
		ref.renderfx = RF_MINLIGHT;
		ref.hModel = ents[i].modelHandle;
		VectorCopy( ents[i].origin, ref.origin );
		VectorCopy( ents[i].origin, ref.lightingOrigin );
		VectorSet( angles, 0.0f, ents[i].yawDegrees, 0.0f );
		AnglesToAxis( angles, ref.axis );
		if ( ents[i].scale > 0.0f && ents[i].scale != 1.0f ) {
			VectorScale( ref.axis[0], ents[i].scale, ref.axis[0] );
			VectorScale( ref.axis[1], ents[i].scale, ref.axis[1] );
			VectorScale( ref.axis[2], ents[i].scale, ref.axis[2] );
			ref.nonNormalizedAxes = qtrue;
		}
		ref.shader.rgba[0] = 255;
		ref.shader.rgba[1] = 255;
		ref.shader.rgba[2] = 255;
		ref.shader.rgba[3] = 255;
		re.AddRefEntityToScene( &ref, qfalse );
	}
}

void CL_RTSDemo_Init( void )
{
	cl_rtsDemo = Cvar_Get( "cl_rtsDemo", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_rtsDemo, "Draw the local RTS model demo through refEntity_t submissions." );
	cl_rtsDemoModel = Cvar_Get( "cl_rtsDemoModel", "models/rts/0ad_jav2.dae", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_rtsDemoModel, "Model qpath used by the local RTS demo." );
	cl_rtsDemoCount = Cvar_Get( "cl_rtsDemoCount", "6", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_rtsDemoCount, "1", va( "%d", CL_RTS_DEMO_MAX_ENTITIES ), CV_INTEGER );
	cl_rtsDemoScale = Cvar_Get( "cl_rtsDemoScale", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_rtsDemoScale, "0.01", "64", CV_FLOAT );
	cl_rtsDemoSpacing = Cvar_Get( "cl_rtsDemoSpacing", "96", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_rtsDemoSpacing, "16", "1024", CV_FLOAT );
	cl_rtsDemoZ = Cvar_Get( "cl_rtsDemoZ", "32", CVAR_ARCHIVE_ND );
	cl_rtsDemoMove = Cvar_Get( "cl_rtsDemoMove", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_rtsDemoMove, "0", "1", CV_INTEGER );

	Cmd_RemoveCommand( "rts_demo_start" );
	Cmd_RemoveCommand( "rts_demo_stop" );
	Cmd_RemoveCommand( "rts_demo_status" );
	Cmd_AddCommand( "rts_demo_start", CL_RTSDemo_Start_f );
	Cmd_AddCommand( "rts_demo_stop", CL_RTSDemo_Stop_f );
	Cmd_AddCommand( "rts_demo_status", CL_RTSDemo_Status_f );
}
