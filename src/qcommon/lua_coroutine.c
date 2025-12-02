/*
===========================================================================
Lua Coroutine Scheduler Implementation

Manages Lua coroutines for async scripting.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA

#include "lua_coroutine.h"
#include "lua_events.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

// Coroutine structure
typedef struct coroutine_s {
	lua_State *L;
	int coroutine_ref;  // Reference to Lua coroutine
	coroutine_state_t state;
	float wait_until_time;  // For time-based waiting
	char wait_event[64];  // For event-based waiting
	int wait_event_callback_ref;  // Callback for event waiting
	qboolean active;
} coroutine_t;

// Global coroutine state
static coroutine_t s_coroutines[MAX_COROUTINES];
static int s_num_coroutines = 0;
static qboolean s_initialized = qfalse;

/*
=================
Lua_Coroutine_Init
Initialize the coroutine scheduler
=================
*/
void Lua_Coroutine_Init(void)
{
	if (s_initialized) {
		return;
	}

	memset(s_coroutines, 0, sizeof(s_coroutines));
	s_num_coroutines = 0;
	s_initialized = qtrue;
}

/*
=================
Lua_Coroutine_Shutdown
Shutdown the coroutine scheduler
=================
*/
void Lua_Coroutine_Shutdown(void)
{
	int i;

	if (!s_initialized) {
		return;
	}

	// Clean up all coroutines
	for (i = 0; i < s_num_coroutines; i++) {
		coroutine_t *co = &s_coroutines[i];
		if (co->active && co->L) {
			if (co->coroutine_ref != LUA_NOREF) {
				luaL_unref(co->L, LUA_REGISTRYINDEX, co->coroutine_ref);
			}
			if (co->wait_event_callback_ref != LUA_NOREF) {
				luaL_unref(co->L, LUA_REGISTRYINDEX, co->wait_event_callback_ref);
			}
		}
	}

	memset(s_coroutines, 0, sizeof(s_coroutines));
	s_num_coroutines = 0;
	s_initialized = qfalse;
}

/*
=================
FindFreeCoroutineSlot
Find a free slot in the coroutine array
=================
*/
static int FindFreeCoroutineSlot(void)
{
	int i;

	for (i = 0; i < MAX_COROUTINES; i++) {
		if (!s_coroutines[i].active) {
			return i;
		}
	}

	return -1;
}

/*
=================
Lua_Coroutine_Create
Create a new coroutine from a Lua function
=================
*/
int Lua_Coroutine_Create(lua_State *L, int func_ref)
{
	int slot;
	coroutine_t *co;
	lua_State *co_L;

	if (!s_initialized || !L || func_ref == LUA_NOREF) {
		return -1;
	}

	slot = FindFreeCoroutineSlot();
	if (slot < 0) {
		Com_Printf("Lua_Coroutine_Create: Maximum coroutines reached\n");
		return -1;
	}

	co = &s_coroutines[slot];

	// Get the function
	lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		return -1;
	}

	// Create coroutine
	co_L = lua_newthread(L);
	if (!co_L) {
		lua_pop(L, 1);  // Pop function
		return -1;
	}

	// Store coroutine reference in main state (before moving function)
	co->coroutine_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	// Move function to coroutine
	lua_rawgeti(L, LUA_REGISTRYINDEX, func_ref);
	lua_xmove(L, co_L, 1);

	co->L = co_L;
	co->state = COROUTINE_RUNNING;
	co->wait_until_time = 0.0f;
	co->wait_event[0] = '\0';
	co->wait_event_callback_ref = LUA_NOREF;
	co->active = qtrue;

	if (slot >= s_num_coroutines) {
		s_num_coroutines = slot + 1;
	}

	return slot;
}

/*
=================
Lua_Coroutine_Update
Update all coroutines (resume ready ones)
=================
*/
void Lua_Coroutine_Update(float deltaTime)
{
	static float current_time = 0.0f;
	int i;
	int resumed = 0;
	const int max_per_frame = 32;  // Limit resumes per frame

	if (!s_initialized) {
		return;
	}

	current_time += deltaTime;

	for (i = 0; i < s_num_coroutines && resumed < max_per_frame; i++) {
		coroutine_t *co = &s_coroutines[i];
		if (!co->active || !co->L) {
			continue;
		}

		// Check if coroutine should resume
		qboolean should_resume = qfalse;

		if (co->state == COROUTINE_RUNNING) {
			should_resume = qtrue;
		} else if (co->state == COROUTINE_WAITING_TIME) {
			if (current_time >= co->wait_until_time) {
				should_resume = qtrue;
				co->state = COROUTINE_RUNNING;
			}
		} else if (co->state == COROUTINE_WAITING_EVENT) {
			// Event-based waiting is handled by event callback
			// The callback will resume the coroutine
			continue;
		}

		if (should_resume) {
			// Resume coroutine
			int result = lua_resume(co->L, NULL, 0);
			resumed++;

			if (result == LUA_OK) {
				// Coroutine finished
				co->state = COROUTINE_FINISHED;
				co->active = qfalse;
				// Note: coroutine_ref cleanup happens in Shutdown
			} else if (result == LUA_YIELD) {
				// Coroutine yielded - check what it's waiting for
				int top = lua_gettop(co->L);
				if (top >= 1) {
					if (lua_isstring(co->L, -1)) {
						const char *wait_type = lua_tostring(co->L, -1);
						if (Q_stricmp(wait_type, "time") == 0 && top >= 2) {
							// Waiting for time
							float wait_time = (float)lua_tonumber(co->L, -2);
							co->wait_until_time = current_time + wait_time;
							co->state = COROUTINE_WAITING_TIME;
							lua_pop(co->L, 2);
						} else if (Q_stricmp(wait_type, "event") == 0 && top >= 2) {
							// Waiting for event
							const char *event_name = lua_tostring(co->L, -2);
							if (event_name) {
								Q_strncpyz(co->wait_event, event_name, sizeof(co->wait_event));
								co->state = COROUTINE_WAITING_EVENT;
								
								// Create callback for event
								lua_pushvalue(co->L, -3);  // Filter function if present
								co->wait_event_callback_ref = luaL_ref(co->L, LUA_REGISTRYINDEX);
								
								// Subscribe to event
								// We'll handle this via a special event callback
							}
							lua_pop(co->L, top);
						}
					}
				}
			} else {
				// Error in coroutine
				const char *error = lua_tostring(co->L, -1);
				Com_Printf("Lua_Coroutine_Update: Error in coroutine %d: %s\n",
					i, error ? error : "Unknown error");
				lua_pop(co->L, 1);
				co->active = qfalse;
			}
		}
	}
}

// =================
// Lua Bindings
// =================

/*
=================
Lua_Coroutine_Wait
Lua binding: wait(seconds)
=================
*/
static int Lua_Coroutine_Wait(lua_State *L)
{
	float seconds;

	if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
		return luaL_error(L, "wait(seconds) expects a number");
	}

	seconds = (float)lua_tonumber(L, 1);
	if (seconds < 0.0f) {
		seconds = 0.0f;
	}

	// Yield with wait type
	lua_pushstring(L, "time");
	lua_pushnumber(L, seconds);
	return lua_yield(L, 2);
}

/*
=================
Lua_Coroutine_WaitForEvent
Lua binding: wait_for_event(event_name, filter_function)
=================
*/
static int Lua_Coroutine_WaitForEvent(lua_State *L)
{
	const char *event_name;

	if (lua_gettop(L) < 1) {
		return luaL_error(L, "wait_for_event(event_name, filter?) expects at least event name");
	}

	event_name = lua_tostring(L, 1);
	if (!event_name) {
		return luaL_error(L, "wait_for_event: event_name must be a string");
	}

	// Yield with event wait type
	lua_pushstring(L, "event");
	lua_pushstring(L, event_name);
	if (lua_gettop(L) >= 2 && lua_isfunction(L, 2)) {
		lua_pushvalue(L, 2);  // Filter function
	} else {
		lua_pushnil(L);  // No filter
	}
	return lua_yield(L, 3);
}

/*
=================
Lua_Coroutine_Start
Lua binding: start_coroutine(function)
=================
*/
static int Lua_Coroutine_Start(lua_State *L)
{
	int func_ref;
	int co_id;

	if (lua_gettop(L) < 1 || !lua_isfunction(L, 1)) {
		lua_pushinteger(L, -1);
		return 1;
	}

	// Create reference to function
	lua_pushvalue(L, 1);
	func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	co_id = Lua_Coroutine_Create(L, func_ref);
	
	// Release function reference (coroutine has its own)
	luaL_unref(L, LUA_REGISTRYINDEX, func_ref);

	lua_pushinteger(L, co_id);
	return 1;
}

/*
=================
Lua_Coroutine_RegisterBindings
Register Lua bindings for coroutine system
=================
*/
void Lua_Coroutine_RegisterBindings(lua_State *L)
{
	if (!L) {
		return;
	}

	// Register functions
	Lua_RegisterFunction(L, "wait", Lua_Coroutine_Wait);
	Lua_RegisterFunction(L, "wait_for_event", Lua_Coroutine_WaitForEvent);
	Lua_RegisterFunction(L, "start_coroutine", Lua_Coroutine_Start);
}

#endif // USE_LUA

