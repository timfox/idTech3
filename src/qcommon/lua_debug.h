#ifndef LUA_DEBUG_H
#define LUA_DEBUG_H

/* Client may register Engine.* bindings (g_lua_bindings.c is client-only). */
typedef void (*LuaDebug_EngineRegisterFn)( void *luaState );
void LuaDebug_SetEngineRegisterCallback( LuaDebug_EngineRegisterFn fn );

void Cmd_ScriptReload_f( void );
void Cmd_ScriptList_f( void );
void Cmd_ScriptDump_f( void );

#endif
