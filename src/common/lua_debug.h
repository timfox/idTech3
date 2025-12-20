/*
===========================================================================
Lua Script Debugging Commands

Console commands for debugging Lua scripts.
===========================================================================
*/

#ifndef __LUA_DEBUG_H__
#define __LUA_DEBUG_H__

#include "q_shared.h"

#ifdef USE_LUA

/*
=================
Cmd_ScriptReload_f
Reload all Lua scripts
=================
*/
void Cmd_ScriptReload_f(void);

/*
=================
Cmd_ScriptList_f
List active scripts, coroutines, encounters, sequences
=================
*/
void Cmd_ScriptList_f(void);

/*
=================
Cmd_ScriptDump_f
Dump detailed script state
=================
*/
void Cmd_ScriptDump_f(void);

#endif // USE_LUA

#endif // __LUA_DEBUG_H__

