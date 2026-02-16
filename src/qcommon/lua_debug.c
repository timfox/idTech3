#include "q_shared.h"
#include "qcommon.h"
#include "lua_debug.h"

#ifdef USE_LUA
#include "lua_compat.h"

#define MAX_LUA_TRACKED_SCRIPTS 64

static lua_State *s_luaState;
static int s_luaTrackedCount;
static char s_luaTrackedScripts[MAX_LUA_TRACKED_SCRIPTS][MAX_OSPATH];

static qboolean LuaDebug_IsAllowedPath( const char *scriptPath ) {
	if ( !scriptPath || !scriptPath[0] ) {
		return qfalse;
	}
	return ( !Q_strncmp( scriptPath, "gameplay/", 9 ) ||
		!Q_strncmp( scriptPath, "server/", 7 ) ||
		!Q_strncmp( scriptPath, "vm/game/", 8 ) ||
		!Q_strncmp( scriptPath, "scripts/lua/", 12 ) );
}

static void LuaDebug_ClearTrackedScripts( void ) {
	s_luaTrackedCount = 0;
}

static void LuaDebug_CloseState( void ) {
	if ( s_luaState ) {
		lua_close( s_luaState );
		s_luaState = NULL;
	}
}

static qboolean LuaDebug_OpenState( void ) {
	if ( s_luaState ) {
		return qtrue;
	}

	s_luaState = luaL_newstate();
	if ( !s_luaState ) {
		Com_Printf( S_COLOR_RED "Lua: failed to initialize VM\n" );
		return qfalse;
	}

	luaL_openlibs( s_luaState );
	return qtrue;
}

static void LuaDebug_PrintLuaError( const char *prefix ) {
	const char *msg = lua_tostring( s_luaState, -1 );
	Com_Printf( S_COLOR_RED "Lua: %s: %s\n", prefix, msg ? msg : "(unknown error)" );
	lua_pop( s_luaState, 1 );
}

static qboolean LuaDebug_LoadScript( const char *scriptPath ) {
	if ( !LuaDebug_OpenState() ) {
		return qfalse;
	}
	if ( !LuaDebug_IsAllowedPath( scriptPath ) ) {
		Com_Printf( S_COLOR_RED "Lua: denied script path '%s' (allowed: gameplay/, server/, vm/game/, scripts/lua/)\n", scriptPath );
		return qfalse;
	}

	if ( luaL_loadfile( s_luaState, scriptPath ) != LUA_OK ) {
		LuaDebug_PrintLuaError( scriptPath );
		return qfalse;
	}

	if ( lua_pcall( s_luaState, 0, LUA_MULTRET, 0 ) != LUA_OK ) {
		LuaDebug_PrintLuaError( scriptPath );
		return qfalse;
	}

	return qtrue;
}

static void LuaDebug_TrackScript( const char *scriptPath ) {
	int i;

	for ( i = 0; i < s_luaTrackedCount; i++ ) {
		if ( !Q_stricmp( s_luaTrackedScripts[i], scriptPath ) ) {
			return;
		}
	}

	if ( s_luaTrackedCount >= MAX_LUA_TRACKED_SCRIPTS ) {
		Com_Printf( S_COLOR_YELLOW "Lua: tracked script limit reached (%d)\n", MAX_LUA_TRACKED_SCRIPTS );
		return;
	}

	Q_strncpyz( s_luaTrackedScripts[s_luaTrackedCount], scriptPath, sizeof( s_luaTrackedScripts[0] ) );
	s_luaTrackedCount++;
}

void Cmd_ScriptReload_f( void ) {
	int argc = Cmd_Argc();
	int i;
	int successCount = 0;
	int failureCount = 0;

	if ( argc <= 1 ) {
		if ( s_luaTrackedCount <= 0 ) {
			LuaDebug_CloseState();
			if ( LuaDebug_OpenState() ) {
				Com_Printf( "Lua: runtime initialized (%s, %d)\n", LUA_VERSION, LUA_VERSION_NUM );
			}
			return;
		}

		LuaDebug_CloseState();
		if ( !LuaDebug_OpenState() ) {
			return;
		}

		for ( i = 0; i < s_luaTrackedCount; i++ ) {
			if ( LuaDebug_LoadScript( s_luaTrackedScripts[i] ) ) {
				successCount++;
			} else {
				failureCount++;
			}
		}

		Com_Printf( "Lua: reloaded %d tracked script(s), %d failure(s)\n", successCount, failureCount );
		return;
	}

	LuaDebug_CloseState();
	LuaDebug_ClearTrackedScripts();

	if ( !LuaDebug_OpenState() ) {
		return;
	}

	for ( i = 1; i < argc; i++ ) {
		const char *scriptPath = Cmd_Argv( i );

		if ( !scriptPath || !scriptPath[0] ) {
			continue;
		}

		if ( LuaDebug_LoadScript( scriptPath ) ) {
			LuaDebug_TrackScript( scriptPath );
			successCount++;
		} else {
			failureCount++;
		}
	}

	Com_Printf( "Lua: loaded %d script(s), %d failure(s)\n", successCount, failureCount );
}

void Cmd_ScriptList_f( void ) {
	int i;

	Com_Printf( "Lua: compile-time API %s (LUA_VERSION_NUM=%d)\n", LUA_VERSION, LUA_VERSION_NUM );
	Com_Printf( "Lua: script path policy gameplay/, server/, vm/game/, scripts/lua/\n" );

	if ( !s_luaState ) {
		Com_Printf( "Lua: runtime not initialized. Run script_reload first.\n" );
		return;
	}

	Com_Printf( "Lua: runtime initialized\n" );
	Com_Printf( "Lua: tracked scripts (%d)\n", s_luaTrackedCount );

	for ( i = 0; i < s_luaTrackedCount; i++ ) {
		Com_Printf( "  %2d: %s\n", i + 1, s_luaTrackedScripts[i] );
	}
}

void Cmd_ScriptDump_f( void ) {
	int maxEntries = 128;
	int printed = 0;
	qboolean truncated = qfalse;

	if ( Cmd_Argc() > 1 ) {
		const int requested = atoi( Cmd_Argv( 1 ) );
		if ( requested > 0 ) {
			maxEntries = requested;
		}
	}

	if ( !s_luaState ) {
		Com_Printf( "Lua: runtime not initialized. Run script_reload first.\n" );
		return;
	}

	ID3_LUA_PUSH_GLOBAL_TABLE( s_luaState );
	lua_pushnil( s_luaState );

	Com_Printf( "Lua: globals (limit %d)\n", maxEntries );
	while ( lua_next( s_luaState, -2 ) != 0 ) {
		const char *keyName = lua_tostring( s_luaState, -2 );
		const char *valueType = lua_typename( s_luaState, lua_type( s_luaState, -1 ) );

		if ( keyName ) {
			Com_Printf( "  %s : %s\n", keyName, valueType );
		} else {
			const char *keyType = lua_typename( s_luaState, lua_type( s_luaState, -2 ) );
			Com_Printf( "  [%s] : %s\n", keyType, valueType );
		}

		lua_pop( s_luaState, 1 );
		printed++;
		if ( printed >= maxEntries ) {
			Com_Printf( "  ... output truncated ...\n" );
			truncated = qtrue;
			break;
		}
	}

	lua_pop( s_luaState, truncated ? 2 : 1 );
	Com_Printf( "Lua: dumped %d global entr%s\n", printed, printed == 1 ? "y" : "ies" );
}

#else

void Cmd_ScriptReload_f( void ) {
	Com_Printf( "Lua support is disabled in this build. Configure with -DUSE_LUA=ON.\n" );
}

void Cmd_ScriptList_f( void ) {
	Com_Printf( "Lua support is disabled in this build. Configure with -DUSE_LUA=ON.\n" );
}

void Cmd_ScriptDump_f( void ) {
	Com_Printf( "Lua support is disabled in this build. Configure with -DUSE_LUA=ON.\n" );
}

#endif
