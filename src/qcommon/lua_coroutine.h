/*
===========================================================================
Lua Coroutine Scheduler

Manages Lua coroutines for async scripting. Enables wait() and
wait_for_event() functionality for non-blocking script execution.
===========================================================================
*/

#ifndef __LUA_COROUTINE_H__
#define __LUA_COROUTINE_H__

#include "q_shared.h"

#ifdef USE_LUA

#include <lua.h>

// Maximum number of active coroutines
#define MAX_COROUTINES 512

// Coroutine state
typedef enum {
	COROUTINE_RUNNING,
	COROUTINE_WAITING_TIME,
	COROUTINE_WAITING_EVENT,
	COROUTINE_FINISHED
} coroutine_state_t;

/*
=================
Lua_Coroutine_Init
Initialize the coroutine scheduler
=================
*/
void Lua_Coroutine_Init(void);

/*
=================
Lua_Coroutine_Shutdown
Shutdown the coroutine scheduler
=================
*/
void Lua_Coroutine_Shutdown(void);

/*
=================
Lua_Coroutine_Update
Update all coroutines (resume ready ones)
Should be called once per frame
deltaTime: Time since last frame in seconds
=================
*/
void Lua_Coroutine_Update(float deltaTime);

/*
=================
Lua_Coroutine_Create
Create a new coroutine from a Lua function
L: Lua state
func_ref: Lua function reference
Returns coroutine ID or -1 on failure
=================
*/
int Lua_Coroutine_Create(lua_State *L, int func_ref);

/*
=================
Lua_Coroutine_RegisterBindings
Register Lua bindings for coroutine system
L: Lua state
=================
*/
void Lua_Coroutine_RegisterBindings(lua_State *L);

#endif // USE_LUA

#endif // __LUA_COROUTINE_H__

