/*
===========================================================================
Lua Event Bus System

Central event dispatcher for decoupled communication between engine systems
and Lua scripts.
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

// Maximum waiting coroutines per event
#define MAX_EVENT_WAITERS 32

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

/*
=================
Lua_Events_SubscribeCallback
Subscribe a Lua function (by registry ref) to an event from C code.
Returns qtrue on success.
=================
*/
qboolean Lua_Events_SubscribeCallback(lua_State *L, const char *event_name, int callback_ref);

/*
=================
Lua_Events_UnsubscribeCallback
Unsubscribe a Lua function (by registry ref) from an event.
=================
*/
void Lua_Events_UnsubscribeCallback(lua_State *L, const char *event_name, int callback_ref);

/*
=================
Lua_Events_SubscribeOnce
Subscribe a Lua function to an event for one-time execution.
Returns qtrue on success.
=================
*/
qboolean Lua_Events_SubscribeOnce(lua_State *L, const char *event_name, int callback_ref);

/*
=================
Lua_Events_WaitFor
Lua coroutine support: Wait for an event and resume with event data.
Returns the number of arguments pushed to Lua stack.
=================
*/
int Lua_Events_WaitFor(lua_State *L, const char *event_name, int timeout_ms);

/*
=================
Lua_Events_Filter
Advanced event filtering support.
Returns qtrue if the event should be delivered to the subscriber.
=================
*/
typedef qboolean (*lua_event_filter_t)(lua_State *L, const char *event_name, int num_args, int *arg_refs);

/*
=================
Lua_Events_SetFilter
Set a filter function for an event subscription.
Returns qtrue on success.
=================
*/
qboolean Lua_Events_SetFilter(lua_State *L, const char *event_name, int callback_ref, lua_event_filter_t filter);

/*
=================
Lua_Events_HotReload
Handle hot-reload of scripts by cleaning up invalid event subscriptions.
Should be called when a script is reloaded.
=================
*/
void Lua_Events_HotReload(const char *script_name);

#endif // USE_LUA

#endif // __LUA_EVENTS_H__

