/*
===========================================================================
Lua Script Debugging Commands

Console commands for debugging Lua scripts.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA

#include <lua.h>
#include <lauxlib.h>
#include "lua_wrapper.h"
#include "lua_entity.h"
#include "lua_encounter.h"
#include "lua_sequence.h"
#include "lua_coroutine.h"

/*
=================
Cmd_ScriptReload_f
Reload all Lua scripts
=================
*/
void Cmd_ScriptReload_f(void)
{
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	
	if (!L) {
		Com_Printf("Script system not initialized\n");
		return;
	}
	
	Com_Printf("Reloading Lua scripts...\n");
	
	// Shutdown and reinit Lua systems
	extern void Lua_Shutdown(void);
	extern void Lua_Init(void);
	
	Lua_Shutdown();
	Lua_Init();
	
	Com_Printf("Scripts reloaded\n");
}

/*
=================
Cmd_ScriptList_f
List active scripts, coroutines, encounters, sequences
=================
*/
void Cmd_ScriptList_f(void)
{
	Com_Printf("=== Lua Script System Status ===\n");
	
	// List entity scripts
	// Note: This would require exposing game_get_entity_count from Lua
	// For now, just show basic status
	Com_Printf("Entities: %d\n", entity_count);
	
	// Note: Detailed listing would require exposing internal state
	// This is a basic implementation
	Com_Printf("Use 'script_dump' for detailed information\n");
}

/*
=================
Cmd_ScriptDump_f
Dump detailed script state
=================
*/
void Cmd_ScriptDump_f(void)
{
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	
	if (!L) {
		Com_Printf("Script system not initialized\n");
		return;
	}
	
	Com_Printf("=== Lua Script Dump ===\n");
	
	// Dump basic Lua state info
	Com_Printf("Lua state: %p\n", (void *)L);
	Com_Printf("Stack top: %d\n", lua_gettop(L));
	
	// Note: More detailed dumping would require exposing internal structures
	// This provides a basic framework that can be extended
	Com_Printf("Dump complete\n");
}

#endif // USE_LUA

