#ifndef LUA_DEBUG_H
#define LUA_DEBUG_H

/* Client may register Engine.* bindings (g_lua_bindings.c is client-only). */
typedef void (*LuaDebug_EngineRegisterFn)( void *luaState );
void LuaDebug_SetEngineRegisterCallback( LuaDebug_EngineRegisterFn fn );

void LuaDebug_InitCvars( void );
void LuaDebug_WatchTick( int nowMs );

qboolean LuaDebug_ReloadScriptPath( const char *scriptPath );
qboolean LuaDebug_BeginHotloadReload( void );
void LuaDebug_FinishHotloadReload( void );
void LuaDebug_SetScriptFallbackRoot( const char *root );
void LuaDebug_CallAppCrdtMessage( int msgMajor, const char *payload );
void LuaDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 );

void Cmd_ScriptReload_f( void );
void Cmd_ScriptList_f( void );
void Cmd_ScriptDump_f( void );

#endif
