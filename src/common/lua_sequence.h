/*
===========================================================================
Lua Sequence System

Timeline-based sequences/cinematics.
===========================================================================
*/

#ifndef __LUA_SEQUENCE_H__
#define __LUA_SEQUENCE_H__

#include "q_shared.h"

#ifdef USE_LUA

#include <lua.h>

// Maximum number of active sequences
#define MAX_SEQUENCES 32

/*
=================
Lua_Sequence_Init
Initialize the sequence system
=================
*/
void Lua_Sequence_Init(void);

/*
=================
Lua_Sequence_Shutdown
Shutdown the sequence system
=================
*/
void Lua_Sequence_Shutdown(void);

/*
=================
Lua_Sequence_Update
Update all active sequences
Should be called once per frame
deltaTime: Time since last frame in seconds
=================
*/
void Lua_Sequence_Update(float deltaTime);

/*
=================
Lua_Sequence_RegisterBindings
Register Lua bindings for sequence system
L: Lua state
=================
*/
void Lua_Sequence_RegisterBindings(lua_State *L);

#endif // USE_LUA

#endif // __LUA_SEQUENCE_H__

