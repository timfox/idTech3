/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Server-side App CRDT publish + broadcast (Wyns et al. star topology).
idtech3backend integration: auto-bootstrap + Engine.AppCrdt on dedicated server.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "net_p2p.h"
#include "net_oscar.h"
#include "server.h"
#include "app_crdt.h"
#include "lua_debug.h"
#include "sv_app_crdt.h"

#ifdef USE_LUA
#include "lua_compat.h"

static appCrdtVersion_t s_authoritativeVersion;
static appCrdtSpec_t s_authoritativeSpec;
static qboolean s_backendBootstrapped;

static void SV_AppCrdt_FormatLocalVersion( char *buf, int buflen )
{
	AppCrdt_FormatVersion( AppCrdt_GetLocalVersion(), buf, buflen );
}

static void SV_AppCrdt_FormatAuthoritativeVersion( char *buf, int buflen )
{
	AppCrdt_FormatVersion( &s_authoritativeVersion, buf, buflen );
}

static const char *SV_AppCrdt_ClientStateName( clientState_t state )
{
	switch ( state ) {
	case CS_FREE: return "free";
	case CS_ZOMBIE: return "zombie";
	case CS_CONNECTED: return "connected";
	case CS_PRIMED: return "primed";
	case CS_ACTIVE: return "active";
	default: return "unknown";
	}
}

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

static int SV_AppCrdt_LuaGetStatus( lua_State *L )
{
	char localVer[32];
	char authoritativeVer[32];

	SV_AppCrdt_FormatLocalVersion( localVer, sizeof( localVer ) );
	SV_AppCrdt_FormatAuthoritativeVersion( authoritativeVer, sizeof( authoritativeVer ) );

	lua_newtable( L );
	lua_pushboolean( L, AppCrdt_IsEnabled() );
	lua_setfield( L, -2, "enabled" );
	lua_pushstring( L, localVer );
	lua_setfield( L, -2, "localVersion" );
	lua_pushstring( L, authoritativeVer );
	lua_setfield( L, -2, "authoritativeVersion" );
	lua_pushinteger( L, AppCrdt_GetQueueMax() );
	lua_setfield( L, -2, "queueMax" );
	lua_pushboolean( L, AppCrdt_BackendAvailable() );
	lua_setfield( L, -2, "backendAvailable" );
	lua_pushstring( L, AppCrdt_GetBackendRoot() ? AppCrdt_GetBackendRoot() : "" );
	lua_setfield( L, -2, "backendRoot" );
	lua_pushboolean( L, s_backendBootstrapped );
	lua_setfield( L, -2, "backendBootstrapped" );
	lua_pushstring( L, s_authoritativeSpec.manifestPath );
	lua_setfield( L, -2, "manifestPath" );
	lua_pushinteger( L, s_authoritativeSpec.scriptCount );
	lua_setfield( L, -2, "scriptCount" );
	return 1;
}

static int SV_LuaCvars_GetString( lua_State *L )
{
	char value[MAX_CVAR_VALUE_STRING];

	Cvar_VariableStringBuffer( luaL_checkstring( L, 1 ), value, sizeof( value ) );
	lua_pushstring( L, value );
	return 1;
}

static int SV_LuaCvars_GetNumber( lua_State *L )
{
	lua_pushnumber( L, Cvar_VariableValue( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int SV_LuaCvars_GetInteger( lua_State *L )
{
	lua_pushinteger( L, Cvar_VariableIntegerValue( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int SV_LuaCvars_Set( lua_State *L )
{
	Cvar_Set( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) );
	return 0;
}

static int SV_LuaCvars_SetNumber( lua_State *L )
{
	char value[64];

	Com_sprintf( value, sizeof( value ), "%g", luaL_checknumber( L, 2 ) );
	Cvar_Set( luaL_checkstring( L, 1 ), value );
	return 0;
}

static int SV_LuaCvars_SetInteger( lua_State *L )
{
	char value[32];

	Com_sprintf( value, sizeof( value ), "%d", (int)luaL_checkinteger( L, 2 ) );
	Cvar_Set( luaL_checkstring( L, 1 ), value );
	return 0;
}

static int SV_LuaCvars_SetBoolean( lua_State *L )
{
	Cvar_Set( luaL_checkstring( L, 1 ), lua_toboolean( L, 2 ) ? "1" : "0" );
	return 0;
}

static int SV_LuaConsole_Exec( lua_State *L )
{
	const char *text = luaL_checkstring( L, 1 );
	const char *mode = luaL_optstring( L, 2, "append" );
	cbufExec_t when = EXEC_APPEND;

	if ( !Q_stricmp( mode, "insert" ) ) {
		when = EXEC_INSERT;
	} else if ( !Q_stricmp( mode, "now" ) ) {
		when = EXEC_NOW;
	}

	Cbuf_ExecuteText( when, text );
	return 0;
}

static int SV_LuaConsole_AddText( lua_State *L )
{
	Cbuf_AddText( luaL_checkstring( L, 1 ) );
	return 0;
}

static int SV_LuaServer_GetInfo( lua_State *L )
{
	lua_newtable( L );
	lua_pushstring( L, sv_hostname ? sv_hostname->string : "" );
	lua_setfield( L, -2, "hostname" );
	lua_pushstring( L, sv_mapname ? sv_mapname->string : "" );
	lua_setfield( L, -2, "mapname" );
	lua_pushinteger( L, sv_gametype ? sv_gametype->integer : 0 );
	lua_setfield( L, -2, "gametype" );
	lua_pushinteger( L, sv.maxclients );
	lua_setfield( L, -2, "maxclients" );
	lua_pushinteger( L, sv_privateClients ? sv_privateClients->integer : 0 );
	lua_setfield( L, -2, "privateClients" );
	lua_pushinteger( L, sv.time );
	lua_setfield( L, -2, "time" );
	lua_pushinteger( L, com_dedicated ? com_dedicated->integer : 0 );
	lua_setfield( L, -2, "dedicated" );
	return 1;
}

static int SV_LuaServer_GetClientCount( lua_State *L )
{
	int count = 0;
	int activeOnly = lua_toboolean( L, 1 );
	int i;

	for ( i = 0; i < sv.maxclients; i++ ) {
		client_t *cl = &svs.clients[i];
		if ( activeOnly ) {
			if ( cl->state == CS_ACTIVE ) {
				count++;
			}
		} else if ( cl->state >= CS_CONNECTED ) {
			count++;
		}
	}

	lua_pushinteger( L, count );
	return 1;
}

static int SV_LuaServer_GetClientInfo( lua_State *L )
{
	int clientNum = (int)luaL_checkinteger( L, 1 );
	client_t *cl;

	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return luaL_error( L, "client index out of range" );
	}

	cl = &svs.clients[clientNum];
	if ( cl->state < CS_CONNECTED ) {
		lua_pushnil( L );
		return 1;
	}

	lua_newtable( L );
	lua_pushinteger( L, clientNum );
	lua_setfield( L, -2, "clientNum" );
	lua_pushstring( L, cl->name );
	lua_setfield( L, -2, "name" );
	lua_pushstring( L, SV_AppCrdt_ClientStateName( cl->state ) );
	lua_setfield( L, -2, "state" );
	lua_pushinteger( L, cl->ping );
	lua_setfield( L, -2, "ping" );
	lua_pushboolean( L, cl->netchan.remoteAddress.type == NA_BOT );
	lua_setfield( L, -2, "isBot" );
	lua_pushstring( L, NET_AdrToStringwPort( &cl->netchan.remoteAddress ) );
	lua_setfield( L, -2, "address" );
	lua_pushstring( L, cl->country ? cl->country : "" );
	lua_setfield( L, -2, "country" );
	lua_pushstring( L, cl->tld );
	lua_setfield( L, -2, "tld" );
	return 1;
}

static int SV_LuaServer_SendCommand( lua_State *L )
{
	int clientNum = (int)luaL_checkinteger( L, 1 );
	const char *cmd = luaL_checkstring( L, 2 );
	client_t *cl;

	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return luaL_error( L, "client index out of range" );
	}

	cl = &svs.clients[clientNum];
	if ( cl->state < CS_CONNECTED ) {
		return luaL_error( L, "client not connected" );
	}

	SV_AddServerCommand( cl, cmd );
	return 0;
}

static int SV_LuaServer_BroadcastCommand( lua_State *L )
{
	SV_AppCrdt_Broadcast( luaL_checkstring( L, 1 ) );
	return 0;
}

static int SV_LuaServer_DropClient( lua_State *L )
{
	int clientNum = (int)luaL_checkinteger( L, 1 );
	const char *reason = luaL_optstring( L, 2, "Disconnected" );
	client_t *cl;

	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return luaL_error( L, "client index out of range" );
	}

	cl = &svs.clients[clientNum];
	if ( cl->state < CS_CONNECTED ) {
		return luaL_error( L, "client not connected" );
	}

	SV_DropClient( cl, reason );
	return 0;
}

static int SV_LuaP2P_GetStatus( lua_State *L )
{
	char address[MAX_STRING_CHARS];
	char sessionId[MAX_STRING_CHARS];

	address[0] = '\0';
	sessionId[0] = '\0';

	if ( sv_p2pSessionId && sv_p2pSessionId->string[0] &&
		Q_stricmp( sv_p2pSessionId->string, "auto" ) != 0 ) {
		Q_strncpyz( sessionId, sv_p2pSessionId->string, sizeof( sessionId ) );
	}
	if ( !sessionId[0] ) {
		Com_sprintf( sessionId, sizeof( sessionId ), "%s-%i-%s",
			FS_GetCurrentGameDir(), sv.serverId, sv_mapname ? sv_mapname->string : "nomap" );
	}

	lua_newtable( L );
	lua_pushstring( L, NET_P2P_BackendName() );
	lua_setfield( L, -2, "backend" );
	lua_pushboolean( L, NET_P2P_IsSupported() );
	lua_setfield( L, -2, "supported" );
	lua_pushboolean( L, NET_P2P_IsEnabled() );
	lua_setfield( L, -2, "enabled" );
	lua_pushboolean( L, NET_P2P_IsReady() );
	lua_setfield( L, -2, "ready" );
	lua_pushstring( L, NET_P2P_GetLocalAddressString( address, sizeof( address ) ) ? address : "" );
	lua_setfield( L, -2, "address" );
	lua_pushboolean( L, sv_p2pHostMigration && sv_p2pHostMigration->integer );
	lua_setfield( L, -2, "hostMigration" );
	lua_pushinteger( L, sv_p2pReconnectWindow ? sv_p2pReconnectWindow->integer : 0 );
	lua_setfield( L, -2, "reconnectWindowSec" );
	lua_pushstring( L, ( sv_p2pFailover && sv_p2pFailover->string[0] ) ? sv_p2pFailover->string : "reconnect" );
	lua_setfield( L, -2, "failover" );
	lua_pushstring( L, sessionId );
	lua_setfield( L, -2, "sessionId" );
	return 1;
}

static int SV_LuaOscar_IsAvailable( lua_State *L )
{
	lua_pushboolean( L, OSCAR_IsAvailable() );
	return 1;
}

static int SV_LuaOscar_GetState( lua_State *L )
{
	lua_pushstring( L, OSCAR_GetStatusString() );
	return 1;
}

static int SV_LuaOscar_GetStatus( lua_State *L )
{
	lua_newtable( L );
	lua_pushboolean( L, OSCAR_IsAvailable() );
	lua_setfield( L, -2, "available" );
	lua_pushstring( L, OSCAR_GetStatusString() );
	lua_setfield( L, -2, "state" );
	lua_pushstring( L, OSCAR_GetCurrentRoom() );
	lua_setfield( L, -2, "room" );
	lua_pushstring( L, OSCAR_GetLastError() );
	lua_setfield( L, -2, "lastError" );
	lua_pushinteger( L, OSCAR_GetReconnectAttempt() );
	lua_setfield( L, -2, "reconnectAttempt" );
	return 1;
}

static int SV_LuaOscar_Connect( lua_State *L )
{
	(void)L;
	lua_pushboolean( L, OSCAR_Connect() );
	return 1;
}

static int SV_LuaOscar_Disconnect( lua_State *L )
{
	OSCAR_Disconnect( luaL_optstring( L, 1, "lua disconnect" ) );
	return 0;
}

static int SV_LuaOscar_SendIM( lua_State *L )
{
	lua_pushboolean( L, OSCAR_SendIM( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}

static int SV_LuaOscar_JoinRoom( lua_State *L )
{
	lua_pushboolean( L, OSCAR_JoinRoom( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int SV_LuaOscar_LeaveRoom( lua_State *L )
{
	lua_pushboolean( L, OSCAR_LeaveRoom( luaL_optstring( L, 1, NULL ) ) );
	return 1;
}

static int SV_LuaOscar_SendRoomMessage( lua_State *L )
{
	lua_pushboolean( L, OSCAR_SendRoomMessage( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}

static int SV_LuaOscar_SetPresence( lua_State *L )
{
	lua_pushboolean( L, OSCAR_SetPresence( luaL_checkstring( L, 1 ), luaL_optstring( L, 2, "" ) ) );
	return 1;
}

static int SV_LuaOscar_AddBuddy( lua_State *L )
{
	lua_pushboolean( L, OSCAR_AddBuddy( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int SV_LuaOscar_RemoveBuddy( lua_State *L )
{
	lua_pushboolean( L, OSCAR_RemoveBuddy( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int SV_LuaOscar_PollEvent( lua_State *L )
{
	oscarEvent_t ev;

	if ( !OSCAR_PollEvent( &ev ) ) {
		return 0;
	}

	lua_newtable( L );
	switch ( ev.type ) {
	case OSCAR_EVENT_CONNECTED: lua_pushstring( L, "connected" ); break;
	case OSCAR_EVENT_DISCONNECTED: lua_pushstring( L, "disconnected" ); break;
	case OSCAR_EVENT_INSTANT_MESSAGE: lua_pushstring( L, "instant_message" ); break;
	case OSCAR_EVENT_ROOM_MESSAGE: lua_pushstring( L, "room_message" ); break;
	case OSCAR_EVENT_PRESENCE_CHANGED: lua_pushstring( L, "presence_changed" ); break;
	case OSCAR_EVENT_REQUEST_COMPLETE: lua_pushstring( L, "request_complete" ); break;
	case OSCAR_EVENT_ERROR: lua_pushstring( L, "error" ); break;
	default: lua_pushstring( L, "unknown" ); break;
	}
	lua_setfield( L, -2, "type" );
	lua_pushinteger( L, ev.requestId );
	lua_setfield( L, -2, "requestId" );
	lua_pushboolean( L, ev.ok );
	lua_setfield( L, -2, "ok" );
	lua_pushstring( L, ev.room );
	lua_setfield( L, -2, "room" );
	lua_pushstring( L, ev.screenName );
	lua_setfield( L, -2, "screenName" );
	lua_pushstring( L, ev.status );
	lua_setfield( L, -2, "status" );
	lua_pushstring( L, ev.text );
	lua_setfield( L, -2, "text" );
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
	lua_pushcfunction( L, SV_AppCrdt_LuaGetStatus );
	lua_setfield( L, -2, "getStatus" );
	lua_setfield( L, -2, "AppCrdt" );

	lua_newtable( L );
	lua_pushcfunction( L, SV_LuaCvars_GetString );
	lua_setfield( L, -2, "getString" );
	lua_pushcfunction( L, SV_LuaCvars_GetNumber );
	lua_setfield( L, -2, "getNumber" );
	lua_pushcfunction( L, SV_LuaCvars_GetInteger );
	lua_setfield( L, -2, "getInteger" );
	lua_pushcfunction( L, SV_LuaCvars_Set );
	lua_setfield( L, -2, "set" );
	lua_pushcfunction( L, SV_LuaCvars_SetNumber );
	lua_setfield( L, -2, "setNumber" );
	lua_pushcfunction( L, SV_LuaCvars_SetInteger );
	lua_setfield( L, -2, "setInteger" );
	lua_pushcfunction( L, SV_LuaCvars_SetBoolean );
	lua_setfield( L, -2, "setBoolean" );
	lua_setfield( L, -2, "Cvars" );

	lua_newtable( L );
	lua_pushcfunction( L, SV_LuaConsole_Exec );
	lua_setfield( L, -2, "exec" );
	lua_pushcfunction( L, SV_LuaConsole_AddText );
	lua_setfield( L, -2, "addText" );
	lua_setfield( L, -2, "Console" );

	lua_newtable( L );
	lua_pushcfunction( L, SV_LuaServer_GetInfo );
	lua_setfield( L, -2, "getInfo" );
	lua_pushcfunction( L, SV_LuaServer_GetClientCount );
	lua_setfield( L, -2, "getClientCount" );
	lua_pushcfunction( L, SV_LuaServer_GetClientInfo );
	lua_setfield( L, -2, "getClientInfo" );
	lua_pushcfunction( L, SV_LuaServer_SendCommand );
	lua_setfield( L, -2, "sendCommand" );
	lua_pushcfunction( L, SV_LuaServer_BroadcastCommand );
	lua_setfield( L, -2, "broadcastCommand" );
	lua_pushcfunction( L, SV_LuaServer_DropClient );
	lua_setfield( L, -2, "dropClient" );
	lua_setfield( L, -2, "Server" );

	lua_newtable( L );
	lua_pushcfunction( L, SV_LuaP2P_GetStatus );
	lua_setfield( L, -2, "getStatus" );
	lua_setfield( L, -2, "P2P" );

	lua_newtable( L );
	lua_pushcfunction( L, SV_LuaOscar_IsAvailable );
	lua_setfield( L, -2, "IsAvailable" );
	lua_pushcfunction( L, SV_LuaOscar_GetState );
	lua_setfield( L, -2, "GetState" );
	lua_pushcfunction( L, SV_LuaOscar_GetStatus );
	lua_setfield( L, -2, "GetStatus" );
	lua_pushcfunction( L, SV_LuaOscar_Connect );
	lua_setfield( L, -2, "Connect" );
	lua_pushcfunction( L, SV_LuaOscar_Disconnect );
	lua_setfield( L, -2, "Disconnect" );
	lua_pushcfunction( L, SV_LuaOscar_SendIM );
	lua_setfield( L, -2, "SendIM" );
	lua_pushcfunction( L, SV_LuaOscar_JoinRoom );
	lua_setfield( L, -2, "JoinRoom" );
	lua_pushcfunction( L, SV_LuaOscar_LeaveRoom );
	lua_setfield( L, -2, "LeaveRoom" );
	lua_pushcfunction( L, SV_LuaOscar_SendRoomMessage );
	lua_setfield( L, -2, "SendRoomMessage" );
	lua_pushcfunction( L, SV_LuaOscar_SetPresence );
	lua_setfield( L, -2, "SetPresence" );
	lua_pushcfunction( L, SV_LuaOscar_AddBuddy );
	lua_setfield( L, -2, "AddBuddy" );
	lua_pushcfunction( L, SV_LuaOscar_RemoveBuddy );
	lua_setfield( L, -2, "RemoveBuddy" );
	lua_pushcfunction( L, SV_LuaOscar_PollEvent );
	lua_setfield( L, -2, "PollEvent" );
	lua_setfield( L, -2, "Oscar" );

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
