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

#endif // USE_LUA

#endif // __LUA_ENCOUNTER_H__

