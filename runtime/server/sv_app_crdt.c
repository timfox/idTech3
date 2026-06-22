/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Server-side App CRDT publish + broadcast (Wyns et al. star topology).
idtech3backend integration: auto-bootstrap + Engine.AppCrdt on dedicated server.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "server.h"
#include "../qcommon/app_crdt.h"
#include "../qcommon/lua_debug.h"
#include "sv_app_crdt.h"

#ifdef USE_LUA
#include "../qcommon/lua_compat.h"

static appCrdtVersion_t s_authoritativeVersion;
static appCrdtSpec_t s_authoritativeSpec;
static qboolean s_backendBootstrapped;

static void SV_AppCrdt_SetAuthoritativeVersion( const appCrdtVersion_t *ver )
{
	char verBuf[32];

	if ( !ver ) {
		return;
	}
	s_authoritativeVersion = *ver;
	AppCrdt_FormatVersion( ver, verBuf, sizeof( verBuf ) );
	Cvar_Set( "com_app_crdt_version", verBuf );
	AppCrdt_SetLocalVersion( ver );
}

static void SV_AppCrdt_Broadcast( const char *cmd )
{
	int i;

	if ( !cmd || !cmd[0] ) {
		return;
	}
	for ( i = 0; i < sv.maxclients; i++ ) {
		client_t *cl = &svs.clients[i];
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		SV_AddServerCommand( cl, cmd );
	}
}

static qboolean SV_AppCrdt_DoPublish( const char *verText, const char *manifestPath )
{
	appCrdtVersion_t ver;
	appCrdtSpec_t spec;
	char verBuf[32];
	char cmd[MAX_STRING_CHARS];

	if ( !AppCrdt_IsEnabled() ) {
		return qfalse;
	}

	if ( !AppCrdt_ParseVersion( verText, &ver ) ) {
		Com_Printf( S_COLOR_YELLOW "[AppCRDT] invalid semver: %s\n", verText );
		return qfalse;
	}

	if ( !AppCrdt_MergeLWW( &s_authoritativeVersion, &ver ) ) {
		Com_Printf( "[AppCRDT] publish ignored (not newer than %d.%d.%d)\n",
			s_authoritativeVersion.major, s_authoritativeVersion.minor, s_authoritativeVersion.patch );
		return qfalse;
	}

	Com_Memset( &spec, 0, sizeof( spec ) );
	spec.version = s_authoritativeVersion;

	if ( manifestPath && manifestPath[0] ) {
		if ( !AppCrdt_LoadManifest( manifestPath, &spec ) ) {
			return qfalse;
		}
		s_authoritativeSpec = spec;
		if ( !AppCrdt_ApplyPublish( &spec ) ) {
			return qfalse;
		}
	} else {
		Com_Memset( &s_authoritativeSpec, 0, sizeof( s_authoritativeSpec ) );
		s_authoritativeSpec.version = s_authoritativeVersion;
	}

	SV_AppCrdt_SetAuthoritativeVersion( &s_authoritativeVersion );
	AppCrdt_FormatVersion( &s_authoritativeVersion, verBuf, sizeof( verBuf ) );

	if ( s_authoritativeSpec.manifestPath[0] ) {
		Com_sprintf( cmd, sizeof( cmd ), "appcrdt publish %s %s\n",
			verBuf, s_authoritativeSpec.manifestPath );
	} else {
		Com_sprintf( cmd, sizeof( cmd ), "appcrdt publish %s\n", verBuf );
	}
	SV_AppCrdt_Broadcast( cmd );
	Com_Printf( "[AppCRDT] published %s to all clients\n", verBuf );
	return qtrue;
}

static void SV_AppCrdt_EmitPayload( const char *payload )
{
	char cmd[MAX_STRING_CHARS];

	if ( !payload || !payload[0] ) {
		return;
	}

	Com_sprintf( cmd, sizeof( cmd ), "appcrdt event %d %s\n",
		s_authoritativeVersion.major, payload );
	SV_AppCrdt_Broadcast( cmd );
	Com_Printf( "[AppCRDT] emitted event major=%d\n", s_authoritativeVersion.major );
}

static int SV_AppCrdt_LuaPublish( lua_State *L )
{
	const char *verText = luaL_checkstring( L, 1 );
	const char *manifest = luaL_optstring( L, 2, "" );

	if ( !AppCrdt_IsEnabled() ) {
		return luaL_error( L, "App CRDT disabled (com_app_crdt 0)" );
	}
	lua_pushboolean( L, SV_AppCrdt_DoPublish( verText, manifest ) );
	return 1;
}

static int SV_AppCrdt_LuaEmit( lua_State *L )
{
	const char *payload = luaL_checkstring( L, 1 );

	if ( !AppCrdt_IsEnabled() ) {
		return luaL_error( L, "App CRDT disabled (com_app_crdt 0)" );
	}
	SV_AppCrdt_EmitPayload( payload );
	return 0;
}

static int SV_AppCrdt_LuaGetVersion( lua_State *L )
{
	char verBuf[32];
	const appCrdtVersion_t *ver = AppCrdt_GetLocalVersion();

	AppCrdt_FormatVersion( ver, verBuf, sizeof( verBuf ) );
	lua_pushstring( L, verBuf );
	return 1;
}

static int SV_AppCrdt_LuaIsEnabled( lua_State *L )
{
	lua_pushboolean( L, AppCrdt_IsEnabled() );
	return 1;
}

static void SV_AppCrdt_RegisterServerLua( void *luaState )
{
	lua_State *L = (lua_State *)luaState;

	lua_getglobal( L, "Engine" );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		lua_newtable( L );
		lua_setglobal( L, "Engine" );
		lua_getglobal( L, "Engine" );
	}

	lua_newtable( L );
	lua_pushcfunction( L, SV_AppCrdt_LuaPublish );
	lua_setfield( L, -2, "publish" );
	lua_pushcfunction( L, SV_AppCrdt_LuaEmit );
	lua_setfield( L, -2, "emit" );
	lua_pushcfunction( L, SV_AppCrdt_LuaGetVersion );
	lua_setfield( L, -2, "getVersion" );
	lua_pushcfunction( L, SV_AppCrdt_LuaIsEnabled );
	lua_setfield( L, -2, "isEnabled" );
	lua_setfield( L, -2, "AppCrdt" );
	lua_pop( L, 1 );
}

void SV_AppCrdt_ClientEnterWorld( client_t *client )
{
	char verBuf[32];
	char cmd[MAX_STRING_CHARS];

	if ( !AppCrdt_IsEnabled() || !client ) {
		return;
	}
	if ( s_authoritativeVersion.major == 0 && s_authoritativeVersion.minor == 0 &&
		s_authoritativeVersion.patch == 0 ) {
		return;
	}

	AppCrdt_FormatVersion( &s_authoritativeVersion, verBuf, sizeof( verBuf ) );
	if ( s_authoritativeSpec.manifestPath[0] ) {
		Com_sprintf( cmd, sizeof( cmd ), "appcrdt publish %s %s\n",
			verBuf, s_authoritativeSpec.manifestPath );
	} else {
		Com_sprintf( cmd, sizeof( cmd ), "appcrdt publish %s\n", verBuf );
	}
	SV_AddServerCommand( client, cmd );
}

static qboolean SV_AppCrdt_HandleClientEvent( client_t *cl, const char *payload )
{
	if ( !AppCrdt_IsEnabled() || !cl || !payload ) {
		return qfalse;
	}

	SV_AppCrdt_EmitPayload( payload );
	Com_Printf( "[AppCRDT] relay client event from %s major=%d\n", cl->name, s_authoritativeVersion.major );
	return qtrue;
}

qboolean SV_AppCrdt_TryClientCommand( client_t *cl, const char *s )
{
	const char *sub;

	if ( !AppCrdt_IsEnabled() || !s ) {
		return qfalse;
	}

	Cmd_TokenizeString( s );
	if ( Q_stricmp( Cmd_Argv( 0 ), "appcrdt" ) ) {
		return qfalse;
	}

	sub = Cmd_Argv( 1 );
	if ( !sub[0] ) {
		return qtrue;
	}

	if ( !Q_stricmp( sub, "event" ) ) {
		const char *payload = Cmd_ArgsFrom( 2 );
		if ( !payload[0] ) {
			return qtrue;
		}
		return SV_AppCrdt_HandleClientEvent( cl, payload );
	}

	return qtrue;
}

static void SV_AppCrdt_Publish_f( void )
{
	if ( !AppCrdt_IsEnabled() ) {
		Com_Printf( S_COLOR_YELLOW "[AppCRDT] disabled (com_app_crdt 0)\n" );
		return;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: app_crdt_publish <semver> [manifest.json]\n" );
		return;
	}

	SV_AppCrdt_DoPublish( Cmd_Argv( 1 ), Cmd_Argc() >= 3 ? Cmd_Argv( 2 ) : NULL );
}

static void SV_AppCrdt_Emit_f( void )
{
	const char *payload;

	if ( !AppCrdt_IsEnabled() ) {
		Com_Printf( S_COLOR_YELLOW "[AppCRDT] disabled (com_app_crdt 0)\n" );
		return;
	}

	payload = Cmd_ArgsFrom( 1 );
	if ( !payload[0] ) {
		Com_Printf( "Usage: app_crdt_emit <payload>\n" );
		return;
	}

	SV_AppCrdt_EmitPayload( payload );
}

void SV_AppCrdt_OnMapReady( void )
{
	char manifestPath[MAX_OSPATH];
	appCrdtSpec_t spec;
	char verBuf[32];
	cvar_t *autoCvar;

	if ( s_backendBootstrapped || !AppCrdt_IsEnabled() ) {
		return;
	}

	autoCvar = Cvar_Get( "com_app_crdt_auto", "1", CVAR_ARCHIVE );
	if ( !autoCvar || !autoCvar->integer ) {
		return;
	}

	AppCrdt_RefreshBackendRoot();
	if ( !AppCrdt_BackendAvailable() ) {
		return;
	}
	if ( !AppCrdt_GetDefaultBackendManifest( manifestPath, sizeof( manifestPath ) ) ) {
		return;
	}
	if ( !AppCrdt_LoadManifest( manifestPath, &spec ) ) {
		return;
	}

	AppCrdt_FormatVersion( &spec.version, verBuf, sizeof( verBuf ) );
	if ( SV_AppCrdt_DoPublish( verBuf, manifestPath ) ) {
		s_backendBootstrapped = qtrue;
		Com_Printf( "[AppCRDT] idtech3backend bootstrap publish %s\n", verBuf );
	}
}

void SV_AppCrdt_Init( void )
{
	const char *verStr;

	Com_Memset( &s_authoritativeVersion, 0, sizeof( s_authoritativeVersion ) );
	Com_Memset( &s_authoritativeSpec, 0, sizeof( s_authoritativeSpec ) );
	s_backendBootstrapped = qfalse;

	verStr = Cvar_VariableString( "com_app_crdt_version" );
	if ( verStr && verStr[0] ) {
		AppCrdt_ParseVersion( verStr, &s_authoritativeVersion );
	}

	LuaDebug_SetEngineRegisterCallback( SV_AppCrdt_RegisterServerLua );
	AppCrdt_RefreshBackendRoot();

	Cmd_AddCommand( "app_crdt_publish", SV_AppCrdt_Publish_f );
	Cmd_AddCommand( "app_crdt_emit", SV_AppCrdt_Emit_f );

	if ( AppCrdt_IsEnabled() && AppCrdt_BackendAvailable() ) {
		Com_Printf( "[AppCRDT] idtech3backend ready (auto=%s)\n",
			Cvar_VariableString( "com_app_crdt_auto" ) );
	}
}

#else

void SV_AppCrdt_Init( void ) {}
void SV_AppCrdt_ClientEnterWorld( client_t *client ) { (void)client; }
void SV_AppCrdt_OnMapReady( void ) {}
qboolean SV_AppCrdt_TryClientCommand( client_t *cl, const char *s ) { (void)cl; (void)s; return qfalse; }

#endif
