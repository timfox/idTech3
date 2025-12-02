/*
===========================================================================
Lua Event Bus System

Central event dispatcher for decoupled communication between engine systems
and Lua scripts. Inspired by id Tech 7's dataflow architecture.
===========================================================================
*/

#ifndef __LUA_EVENTS_H__
#define __LUA_EVENTS_H__

#include "q_shared.h"

#ifdef USE_LUA

#include <lua.h>

// Maximum number of event subscribers per event
#define MAX_EVENT_SUBSCRIBERS 256

// Maximum event queue size
#define MAX_EVENT_QUEUE 1024

// Maximum arguments per event
#define MAX_EVENT_ARGS 8

/*
=================
Lua_Events_Init
Initialize the event bus system
=================
*/
void Lua_Events_Init(void);

/*
=================
Lua_Events_Shutdown
Shutdown the event bus system
=================
*/
void Lua_Events_Shutdown(void);

/*
=================
Lua_Events_Update
Process queued events and dispatch to subscribers
Should be called once per frame
=================
*/
void Lua_Events_Update(void);

/*
=================
Lua_Events_Emit
Emit an event from engine code
event_name: Name of the event
num_args: Number of arguments (up to MAX_EVENT_ARGS)
...: Variable arguments (must be valid Lua values or NULL)
=================
*/
void Lua_Events_Emit(const char *event_name, int num_args, ...);

/*
=================
Lua_Events_EmitFromLua
Emit an event from Lua code
L: Lua state
event_name: Name of the event
num_args: Number of arguments on Lua stack
=================
*/
void Lua_Events_EmitFromLua(lua_State *L, const char *event_name, int num_args);

/*
=================
Lua_Events_RegisterBindings
Register Lua bindings for event system
L: Lua state
=================
*/
void Lua_Events_RegisterBindings(lua_State *L);

#endif // USE_LUA

#endif // __LUA_EVENTS_H__

