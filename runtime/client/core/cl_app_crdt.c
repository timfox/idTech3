/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side App CRDT: intercept server commands, apply publish, event queue.
===========================================================================
*/

#include "client.h"
#include "cl_app_crdt.h"
#include "../../qcommon/app_crdt.h"
#include "../../qcommon/lua_debug.h"
#include "../../qcommon/script_emit.h"

#ifdef USE_LUA
#include "lua_compat.h"

static appCrdtQueue_t s_eventQueue;

static void CL_AppCrdt_DeliverEvent( int msgMajor, const char *payload, void *userData )
{
	(void)userData;
	LuaDebug_CallAppCrdtMessage( msgMajor, payload );
}

static void CL_AppCrdt_AdaptEvent( int msgMajor, const char *payload, void *userData )
{
	(void)userData;
	LuaDebug_CallAppCrdtMessage( msgMajor, payload );
}

static void CL_AppCrdt_OnPublish( const char *verText, const char *manifestPath )
{
	appCrdtVersion_t ver;
	appCrdtSpec_t spec;

	if ( !AppCrdt_ParseVersion( verText, &ver ) ) {
		Com_Printf( S_COLOR_YELLOW "[AppCRDT] invalid publish version: %s\n", verText );
		return;
	}

	{
		const appCrdtVersion_t *local = AppCrdt_GetLocalVersion();
		if ( AppCrdt_CompareVersion( &ver, local ) <= 0 ) {
			Com_Printf( "[AppCRDT] publish ignored (local already at %s)\n", verText );
			return;
		}
	}

	if ( manifestPath && manifestPath[0] ) {
		if ( !AppCrdt_LoadManifest( manifestPath, &spec ) ) {
			return;
		}
		if ( AppCrdt_CompareVersion( &spec.version, &ver ) != 0 ) {
			spec.version = ver;
		}
	} else {
		Com_Memset( &spec, 0, sizeof( spec ) );
		spec.version = ver;
	}

	if ( spec.scriptCount > 0 ) {
		if ( !AppCrdt_ApplyPublish( &spec ) ) {
			return;
		}
	} else {
		AppCrdt_SetLocalVersion( &ver );
		Com_Printf( "[AppCRDT] synced version %s (no manifest scripts)\n", verText );
	}

	Com_ScriptEmitEvent( "app_crdt_publish", verText,
		manifestPath ? manifestPath : "", spec.scriptCount, AppCrdt_GetLocalMajor() );

	AppCrdt_QueueFlushUpToMajor( &s_eventQueue, AppCrdt_GetLocalMajor() );
}

static void CL_AppCrdt_OnEvent( int msgMajor, const char *payload )
{
	if ( !payload ) {
		return;
	}
	Com_ScriptEmitEvent( "app_crdt_event", payload, NULL, msgMajor, AppCrdt_GetLocalMajor() );
	AppCrdt_QueueDispatch( &s_eventQueue, AppCrdt_GetLocalMajor(), msgMajor, payload );
}

static void CL_AppCrdt_Flush_f( void )
{
	int n = AppCrdt_QueueFlushUpToMajor( &s_eventQueue, AppCrdt_GetLocalMajor() );
	Com_Printf( "[AppCRDT] flushed %d queued event(s)\n", n );
	Com_ScriptEmitEvent( "app_crdt_flush", NULL, NULL, n, AppCrdt_GetLocalMajor() );
}

static int CL_AppCrdt_LuaEmit( lua_State *L )
{
	char cmd[MAX_STRING_CHARS];
	const char *payload;

	if ( !AppCrdt_IsEnabled() ) {
		return luaL_error( L, "App CRDT disabled (com_app_crdt 0)" );
	}

	payload = luaL_checkstring( L, 1 );
	Com_sprintf( cmd, sizeof( cmd ), "appcrdt event %d %s",
		AppCrdt_GetLocalMajor(), payload );
	CL_AddReliableCommand( cmd, qfalse );
	return 0;
}

static int CL_AppCrdt_LuaGetVersion( lua_State *L )
{
	char verBuf[32];

	AppCrdt_FormatVersion( AppCrdt_GetLocalVersion(), verBuf, sizeof( verBuf ) );
	lua_pushstring( L, verBuf );
	return 1;
}

static int CL_AppCrdt_LuaIsEnabled( lua_State *L )
{
	lua_pushboolean( L, AppCrdt_IsEnabled() );
	return 1;
}

static int CL_AppCrdt_LuaGetStatus( lua_State *L )
{
	char verBuf[32];

	AppCrdt_FormatVersion( AppCrdt_GetLocalVersion(), verBuf, sizeof( verBuf ) );
	lua_newtable( L );
	lua_pushboolean( L, AppCrdt_IsEnabled() );
	lua_setfield( L, -2, "enabled" );
	lua_pushstring( L, verBuf );
	lua_setfield( L, -2, "localVersion" );
	lua_pushinteger( L, AppCrdt_GetLocalMajor() );
	lua_setfield( L, -2, "localMajor" );
	lua_pushinteger( L, AppCrdt_GetQueueMax() );
	lua_setfield( L, -2, "queueMax" );
	lua_pushinteger( L, s_eventQueue.count );
	lua_setfield( L, -2, "queuedEvents" );
	lua_pushboolean( L, AppCrdt_BackendAvailable() );
	lua_setfield( L, -2, "backendAvailable" );
	lua_pushstring( L, AppCrdt_GetBackendRoot() ? AppCrdt_GetBackendRoot() : "" );
	lua_setfield( L, -2, "backendRoot" );
	return 1;
}

void CL_AppCrdt_RegisterLua( lua_State *L )
{
	lua_newtable( L );
	lua_pushcfunction( L, CL_AppCrdt_LuaEmit );
	lua_setfield( L, -2, "emit" );
	lua_pushcfunction( L, CL_AppCrdt_LuaGetVersion );
	lua_setfield( L, -2, "getVersion" );
	lua_pushcfunction( L, CL_AppCrdt_LuaIsEnabled );
	lua_setfield( L, -2, "isEnabled" );
	lua_pushcfunction( L, CL_AppCrdt_LuaGetStatus );
	lua_setfield( L, -2, "getStatus" );
	lua_setglobal( L, "AppCrdt" );

	lua_getglobal( L, "Engine" );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		lua_newtable( L );
		lua_setglobal( L, "Engine" );
		lua_getglobal( L, "Engine" );
	}
	lua_getglobal( L, "AppCrdt" );
	lua_setfield( L, -2, "AppCrdt" );
	lua_pop( L, 1 );
}

void CL_AppCrdt_Init( void )
{
	int cap = AppCrdt_GetQueueMax();

	AppCrdt_RefreshBackendRoot();
	AppCrdt_QueueInit( &s_eventQueue, cap, CL_AppCrdt_DeliverEvent, CL_AppCrdt_AdaptEvent, NULL );
	Cmd_AddCommand( "app_crdt_flush", CL_AppCrdt_Flush_f );
}

void CL_AppCrdt_Frame( void )
{
	if ( !AppCrdt_IsEnabled() ) {
		return;
	}
	/* queue drain happens on publish; optional periodic flush for same-major backlog */
}

qboolean CL_AppCrdt_TryServerCommand( const char *s )
{
	const char *sub;
	const char *verText;
	const char *manifest;

	if ( !AppCrdt_IsEnabled() || !s ) {
		return qfalse;
	}

	Cmd_TokenizeString( s );
	if ( Q_stricmp( Cmd_Argv( 0 ), "appcrdt" ) ) {
		return qfalse;
	}

	sub = Cmd_Argv( 1 );
	if ( !Q_stricmp( sub, "publish" ) ) {
		verText = Cmd_Argv( 2 );
		manifest = Cmd_Argv( 3 );
		CL_AppCrdt_OnPublish( verText, manifest );
		return qtrue;
	}
	if ( !Q_stricmp( sub, "event" ) ) {
		int msgMajor = atoi( Cmd_Argv( 2 ) );
		const char *payload = Cmd_ArgsFrom( 3 );
		if ( payload[0] ) {
			CL_AppCrdt_OnEvent( msgMajor, payload );
		}
		return qtrue;
	}

	return qtrue;
}

#else

void CL_AppCrdt_Init( void ) {}
void CL_AppCrdt_Frame( void ) {}
qboolean CL_AppCrdt_TryServerCommand( const char *s ) { (void)s; return qfalse; }
void CL_AppCrdt_RegisterLua( struct lua_State *L ) { (void)L; }

#endif
