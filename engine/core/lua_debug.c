#include "q_shared.h"
#include "qcommon.h"
#include "lua_debug.h"

#ifdef USE_LUA
#include "lua_compat.h"

#define MAX_LUA_TRACKED_SCRIPTS 64
#define LUA_HOTLOAD_STATE_KEY "idtech3_hotload_state"

static lua_State *s_luaState;
static int s_luaTrackedCount;
static char s_luaTrackedScripts[MAX_LUA_TRACKED_SCRIPTS][MAX_OSPATH];
static fileTime_t s_luaScriptMtimes[MAX_LUA_TRACKED_SCRIPTS];
static LuaDebug_EngineRegisterFn s_luaEngineRegisterFn;
static cvar_t *com_scriptWatch;
static cvar_t *com_scriptWatchMs;
static int s_scriptWatchNextMs;
static char s_scriptFallbackRoot[MAX_OSPATH];

static void LuaDebug_PrintLuaError( const char *prefix );
static void LuaDebug_CloseState( void );
static qboolean LuaDebug_OpenState( void );
static qboolean LuaDebug_LoadScript( const char *scriptPath );

void LuaDebug_SetEngineRegisterCallback( LuaDebug_EngineRegisterFn fn ) {
	s_luaEngineRegisterFn = fn;
}

void LuaDebug_SetScriptFallbackRoot( const char *root ) {
	if ( root && root[0] ) {
		Q_strncpyz( s_scriptFallbackRoot, root, sizeof( s_scriptFallbackRoot ) );
	} else {
		s_scriptFallbackRoot[0] = '\0';
	}
}

void LuaDebug_InitCvars( void ) {
	com_scriptWatch = Cvar_Get( "com_scriptWatch", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_scriptWatch, "Auto script_reload when tracked Lua files change on disk (loose files only)." );
	com_scriptWatchMs = Cvar_Get( "com_scriptWatchMs", "500", CVAR_ARCHIVE );
	Cvar_SetDescription( com_scriptWatchMs, "Poll interval for com_scriptWatch (milliseconds)." );
}

static qboolean LuaDebug_StatScript( const char *scriptPath, fileTime_t *mtimeOut ) {
	char *ospath;
	fileOffset_t size;
	fileTime_t mtime, ctime;

	if ( !mtimeOut || !scriptPath || !scriptPath[0] ) {
		return qfalse;
	}

	ospath = FS_BuildOSPath( FS_GetHomePath(), FS_GetCurrentGameDir(), scriptPath );
	if ( Sys_GetFileStats( ospath, &size, &mtime, &ctime ) ) {
		*mtimeOut = mtime;
		return qtrue;
	}

	{
		cvar_t *basepath = Cvar_Get( "fs_basepath", "", CVAR_LATCH );
		if ( basepath && basepath->string[0] ) {
			ospath = FS_BuildOSPath( basepath->string, FS_GetCurrentGameDir(), scriptPath );
			if ( Sys_GetFileStats( ospath, &size, &mtime, &ctime ) ) {
				*mtimeOut = mtime;
				return qtrue;
			}
		}
	}

	return qfalse;
}

static void LuaDebug_CallHotloadDestroy( void ) {
	if ( !s_luaState ) {
		return;
	}

	lua_getglobal( s_luaState, "on_hotload_destroy" );
	if ( !lua_isfunction( s_luaState, -1 ) ) {
		lua_pop( s_luaState, 1 );
		return;
	}

	if ( lua_pcall( s_luaState, 0, 1, 0 ) != LUA_OK ) {
		LuaDebug_PrintLuaError( "on_hotload_destroy" );
		return;
	}

	lua_pushvalue( s_luaState, -1 );
	lua_setfield( s_luaState, LUA_REGISTRYINDEX, LUA_HOTLOAD_STATE_KEY );
	lua_pop( s_luaState, 1 );
	Com_Printf( "Lua: on_hotload_destroy OK\n" );
}

static void LuaDebug_CallHotloadCreate( void ) {
	if ( !s_luaState ) {
		return;
	}

	lua_getglobal( s_luaState, "on_hotload_create" );
	if ( !lua_isfunction( s_luaState, -1 ) ) {
		lua_pop( s_luaState, 1 );
		return;
	}

	lua_getfield( s_luaState, LUA_REGISTRYINDEX, LUA_HOTLOAD_STATE_KEY );
	if ( lua_pcall( s_luaState, 1, 0, 0 ) != LUA_OK ) {
		LuaDebug_PrintLuaError( "on_hotload_create" );
	}
	lua_pushnil( s_luaState );
	lua_setfield( s_luaState, LUA_REGISTRYINDEX, LUA_HOTLOAD_STATE_KEY );
	Com_Printf( "Lua: on_hotload_create OK\n" );
}

static void LuaDebug_ReloadTracked( void )
{
	int i;
	int successCount = 0;
	int failureCount = 0;

	if ( s_luaTrackedCount <= 0 ) {
		return;
	}

	LuaDebug_CallHotloadDestroy();
	LuaDebug_CloseState();
	if ( !LuaDebug_OpenState() ) {
		return;
	}

	for ( i = 0; i < s_luaTrackedCount; i++ ) {
		if ( LuaDebug_LoadScript( s_luaTrackedScripts[i] ) ) {
			if ( !LuaDebug_StatScript( s_luaTrackedScripts[i], &s_luaScriptMtimes[i] ) ) {
				s_luaScriptMtimes[i] = 0;
			}
			successCount++;
		} else {
			failureCount++;
		}
	}

	LuaDebug_CallHotloadCreate();
	Com_Printf( "Lua: reloaded %d tracked script(s), %d failure(s)\n", successCount, failureCount );
}

void LuaDebug_WatchTick( int nowMs ) {
	int i;

	if ( !com_scriptWatch || !com_scriptWatch->integer || s_luaTrackedCount <= 0 ) {
		return;
	}

	if ( nowMs < s_scriptWatchNextMs ) {
		return;
	}

	s_scriptWatchNextMs = nowMs + ( com_scriptWatchMs ? com_scriptWatchMs->integer : 500 );
	if ( s_scriptWatchNextMs <= nowMs ) {
		s_scriptWatchNextMs = nowMs + 500;
	}

	for ( i = 0; i < s_luaTrackedCount; i++ ) {
		fileTime_t mtime;

		if ( !LuaDebug_StatScript( s_luaTrackedScripts[i], &mtime ) ) {
			continue;
		}
		if ( s_luaScriptMtimes[i] != 0 && mtime != s_luaScriptMtimes[i] ) {
			Com_Printf( "Lua: watch detected change in %s\n", s_luaTrackedScripts[i] );
			LuaDebug_ReloadTracked();
			return;
		}
		if ( s_luaScriptMtimes[i] == 0 ) {
			s_luaScriptMtimes[i] = mtime;
		}
	}
}

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
	if ( s_luaEngineRegisterFn ) {
		s_luaEngineRegisterFn( s_luaState );
		Com_Printf( "Lua: Engine.* API registered\n" );
	}
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
		void *buf = NULL;
		int blen;
		char osPath[MAX_OSPATH];
		qboolean loaded = qfalse;

		lua_pop( s_luaState, 1 );
		blen = FS_ReadFile( scriptPath, &buf );
		if ( blen > 0 && buf ) {
			if ( luaL_loadbuffer( s_luaState, (const char *)buf, (size_t)blen, scriptPath ) == LUA_OK ) {
				loaded = qtrue;
			} else {
				lua_pop( s_luaState, 1 );
			}
			FS_FreeFile( buf );
		}

		if ( !loaded && s_scriptFallbackRoot[0] ) {
			Com_sprintf( osPath, sizeof( osPath ), "%s/%s", s_scriptFallbackRoot, scriptPath );
			if ( luaL_loadfile( s_luaState, osPath ) != LUA_OK ) {
				LuaDebug_PrintLuaError( scriptPath );
				return qfalse;
			}
			loaded = qtrue;
		}

		if ( !loaded ) {
			LuaDebug_PrintLuaError( scriptPath );
			return qfalse;
		}
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
	if ( !LuaDebug_StatScript( scriptPath, &s_luaScriptMtimes[s_luaTrackedCount] ) ) {
		s_luaScriptMtimes[s_luaTrackedCount] = 0;
	}
	s_luaTrackedCount++;
}

qboolean LuaDebug_BeginHotloadReload( void )
{
	LuaDebug_CallHotloadDestroy();
	LuaDebug_CloseState();
	LuaDebug_ClearTrackedScripts();
	Com_Memset( s_luaScriptMtimes, 0, sizeof( s_luaScriptMtimes ) );
	return LuaDebug_OpenState();
}

qboolean LuaDebug_ReloadScriptPath( const char *scriptPath )
{
	if ( !scriptPath || !scriptPath[0] ) {
		return qfalse;
	}
	if ( !LuaDebug_OpenState() ) {
		return qfalse;
	}
	if ( LuaDebug_LoadScript( scriptPath ) ) {
		LuaDebug_TrackScript( scriptPath );
		return qtrue;
	}
	return qfalse;
}

void LuaDebug_FinishHotloadReload( void )
{
	LuaDebug_CallHotloadCreate();
}

void LuaDebug_CallAppCrdtMessage( int msgMajor, const char *payload )
{
	if ( !s_luaState || !payload ) {
		return;
	}

	lua_getglobal( s_luaState, "on_app_crdt_message" );
	if ( !lua_isfunction( s_luaState, -1 ) ) {
		lua_pop( s_luaState, 1 );
		return;
	}

	lua_pushinteger( s_luaState, msgMajor );
	lua_pushstring( s_luaState, payload );
	if ( lua_pcall( s_luaState, 2, 0, 0 ) != LUA_OK ) {
		LuaDebug_PrintLuaError( "on_app_crdt_message" );
	}
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

		LuaDebug_ReloadTracked();
		return;
	}

	if ( !LuaDebug_BeginHotloadReload() ) {
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

	LuaDebug_CallHotloadCreate();
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

void LuaDebug_InitCvars( void ) {
}

void LuaDebug_SetScriptFallbackRoot( const char *root ) {
	(void)root;
}

void LuaDebug_WatchTick( int nowMs ) {
	(void)nowMs;
}

qboolean LuaDebug_BeginHotloadReload( void ) { return qfalse; }
qboolean LuaDebug_ReloadScriptPath( const char *scriptPath ) { (void)scriptPath; return qfalse; }
void LuaDebug_FinishHotloadReload( void ) {}
void LuaDebug_CallAppCrdtMessage( int msgMajor, const char *payload )
{
	(void)msgMajor;
	(void)payload;
}

#endif
