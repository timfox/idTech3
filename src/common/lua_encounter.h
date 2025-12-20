/*
===========================================================================
Lua Encounter System

State machine for combat encounters with wave spawning and triggers.
===========================================================================
*/

#ifndef __LUA_ENCOUNTER_H__
#define __LUA_ENCOUNTER_H__

#include "q_shared.h"

#ifdef USE_LUA

#include <lua.h>

// Maximum number of active encounters
#define MAX_ENCOUNTERS 64

/*
=================
Lua_Encounter_Init
Initialize the encounter system
=================
*/
void Lua_Encounter_Init(void);

/*
=================
Lua_Encounter_Shutdown
Shutdown the encounter system
=================
*/
void Lua_Encounter_Shutdown(void);

/*
=================
Lua_Encounter_Update
Update all active encounters
Should be called once per frame
=================
*/
void Lua_Encounter_Update(void);

/*
=================
Lua_Encounter_RegisterBindings
Register Lua bindings for encounter system
L: Lua state
=================
*/
void Lua_Encounter_RegisterBindings(lua_State *L);

/*
=================
Lua_Encounter_StartByName
Invoke the Lua on_start hook for a named encounter (if defined)
=================
*/
void Lua_Encounter_StartByName(const char *name);

/*
=================
Lua_Encounter_OnWaveSpawn
Invoke the Lua on_wave_spawn hook for a named encounter (if defined)
=================
*/
void Lua_Encounter_OnWaveSpawn(const char *name, int wave_num);

/*
=================
Lua_Encounter_OnComplete
Invoke the Lua on_complete/on_fail hooks for a named encounter
success: qtrue for completion, qfalse for failure
=================
*/
void Lua_Encounter_OnComplete(const char *name, qboolean success);

#endif // USE_LUA

#endif // __LUA_ENCOUNTER_H__

