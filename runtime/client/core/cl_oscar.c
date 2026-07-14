/*
===========================================================================
Client OSCAR AIM shell: notify bridge + Engine.Oscar Lua bindings.
Uses the process-wide shared OSCAR session (hybrid local screen name).
===========================================================================
*/

#include "client.h"
#include "cl_oscar.h"
#include "net_oscar.h"

#ifdef USE_LUA
#include "lua_compat.h"
#endif

static cvar_t *cl_oscarNotify;
static cvar_t *cl_oscarChat;
static cvar_t *cl_oscarUi;
static unsigned int s_lastRosterGen;

#ifdef USE_LUA
static int CL_LuaOscar_IsAvailable( lua_State *L )
{
	lua_pushboolean( L, OSCAR_IsAvailable() );
	return 1;
}

static int CL_LuaOscar_GetState( lua_State *L )
{
	lua_pushstring( L, OSCAR_GetStatusString() );
	return 1;
}

static int CL_LuaOscar_GetStatus( lua_State *L )
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
	lua_pushinteger( L, OSCAR_BuddyCount() );
	lua_setfield( L, -2, "buddyCount" );
	lua_pushinteger( L, (lua_Integer)OSCAR_GetRosterGeneration() );
	lua_setfield( L, -2, "rosterGeneration" );
	return 1;
}

static int CL_LuaOscar_Connect( lua_State *L )
{
	lua_pushboolean( L, OSCAR_Connect() );
	return 1;
}

static int CL_LuaOscar_Disconnect( lua_State *L )
{
	OSCAR_Disconnect( luaL_optstring( L, 1, "lua disconnect" ) );
	return 0;
}

static int CL_LuaOscar_SendIM( lua_State *L )
{
	lua_pushboolean( L, OSCAR_SendIM( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}

static int CL_LuaOscar_JoinRoom( lua_State *L )
{
	lua_pushboolean( L, OSCAR_JoinRoom( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int CL_LuaOscar_LeaveRoom( lua_State *L )
{
	lua_pushboolean( L, OSCAR_LeaveRoom( luaL_optstring( L, 1, NULL ) ) );
	return 1;
}

static int CL_LuaOscar_SendRoomMessage( lua_State *L )
{
	lua_pushboolean( L, OSCAR_SendRoomMessage( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}

static int CL_LuaOscar_SetPresence( lua_State *L )
{
	lua_pushboolean( L, OSCAR_SetPresence( luaL_checkstring( L, 1 ), luaL_optstring( L, 2, "" ) ) );
	return 1;
}

static int CL_LuaOscar_AddBuddy( lua_State *L )
{
	lua_pushboolean( L, OSCAR_AddBuddy( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int CL_LuaOscar_RemoveBuddy( lua_State *L )
{
	lua_pushboolean( L, OSCAR_RemoveBuddy( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int CL_LuaOscar_BuddyCount( lua_State *L )
{
	lua_pushinteger( L, OSCAR_BuddyCount() );
	return 1;
}

static int CL_LuaOscar_GetBuddy( lua_State *L )
{
	oscarBuddy_t buddy;
	int index = (int)luaL_checkinteger( L, 1 );

	if ( !OSCAR_BuddyGet( index, &buddy ) ) {
		return 0;
	}
	lua_newtable( L );
	lua_pushstring( L, buddy.screenName );
	lua_setfield( L, -2, "screenName" );
	lua_pushstring( L, buddy.status );
	lua_setfield( L, -2, "status" );
	lua_pushstring( L, buddy.awayMessage );
	lua_setfield( L, -2, "awayMessage" );
	lua_pushboolean( L, buddy.online );
	lua_setfield( L, -2, "online" );
	return 1;
}

static int CL_LuaOscar_PollEvent( lua_State *L )
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

void CL_Oscar_RegisterLua( lua_State *L )
{
	if ( !L ) {
		return;
	}

	lua_newtable( L );
	lua_pushcfunction( L, CL_LuaOscar_IsAvailable );
	lua_setfield( L, -2, "IsAvailable" );
	lua_pushcfunction( L, CL_LuaOscar_GetState );
	lua_setfield( L, -2, "GetState" );
	lua_pushcfunction( L, CL_LuaOscar_GetStatus );
	lua_setfield( L, -2, "GetStatus" );
	lua_pushcfunction( L, CL_LuaOscar_Connect );
	lua_setfield( L, -2, "Connect" );
	lua_pushcfunction( L, CL_LuaOscar_Disconnect );
	lua_setfield( L, -2, "Disconnect" );
	lua_pushcfunction( L, CL_LuaOscar_SendIM );
	lua_setfield( L, -2, "SendIM" );
	lua_pushcfunction( L, CL_LuaOscar_JoinRoom );
	lua_setfield( L, -2, "JoinRoom" );
	lua_pushcfunction( L, CL_LuaOscar_LeaveRoom );
	lua_setfield( L, -2, "LeaveRoom" );
	lua_pushcfunction( L, CL_LuaOscar_SendRoomMessage );
	lua_setfield( L, -2, "SendRoomMessage" );
	lua_pushcfunction( L, CL_LuaOscar_SetPresence );
	lua_setfield( L, -2, "SetPresence" );
	lua_pushcfunction( L, CL_LuaOscar_AddBuddy );
	lua_setfield( L, -2, "AddBuddy" );
	lua_pushcfunction( L, CL_LuaOscar_RemoveBuddy );
	lua_setfield( L, -2, "RemoveBuddy" );
	lua_pushcfunction( L, CL_LuaOscar_BuddyCount );
	lua_setfield( L, -2, "BuddyCount" );
	lua_pushcfunction( L, CL_LuaOscar_GetBuddy );
	lua_setfield( L, -2, "GetBuddy" );
	lua_pushcfunction( L, CL_LuaOscar_PollEvent );
	lua_setfield( L, -2, "PollEvent" );

	lua_getglobal( L, "Engine" );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		lua_newtable( L );
		lua_setglobal( L, "Engine" );
		lua_getglobal( L, "Engine" );
	}
	lua_pushvalue( L, -2 );
	lua_setfield( L, -2, "Oscar" );
	lua_pop( L, 2 );
}
#else
void CL_Oscar_RegisterLua( struct lua_State *L )
{
	(void)L;
}
#endif

void CL_Oscar_Init( void )
{
	cl_oscarNotify = Cvar_Get( "cl_oscarNotify", "1", CVAR_ARCHIVE );
	cl_oscarChat = Cvar_Get( "cl_oscarChat", "1", CVAR_ARCHIVE );
	cl_oscarUi = Cvar_Get( "cl_oscarUi", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_oscarNotify, "When 1, mirror OSCAR IM/room/presence into console notify when oscar_notify is on." );
	Cvar_SetDescription( cl_oscarChat, "When 1, print OSCAR chat-oriented messages with AIM-style coloring." );
	Cvar_SetDescription( cl_oscarUi, "When 1, show OSCAR buddy panel in the ImGui inspector (requires r_imgui)." );
	s_lastRosterGen = OSCAR_GetRosterGeneration();
	Com_Printf( "OSCAR client shell: notify=%d chat=%d ui=%d\n",
		cl_oscarNotify->integer, cl_oscarChat->integer, cl_oscarUi->integer );
}

void CL_Oscar_Shutdown( void )
{
}

/*
===============
CL_Oscar_Frame

Lightweight client-side roster dirty log (events already printf via oscar_notify).
===============
*/
void CL_Oscar_Frame( void )
{
	unsigned int gen;

	if ( !cl_oscarNotify || !cl_oscarNotify->integer ) {
		return;
	}
	if ( !OSCAR_IsAvailable() ) {
		return;
	}

	gen = OSCAR_GetRosterGeneration();
	if ( gen != s_lastRosterGen ) {
		s_lastRosterGen = gen;
		/* Presence/IM printf is handled in net_oscar; keep this quiet. */
		(void)cl_oscarChat;
	}
}
