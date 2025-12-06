/*
===========================================================================
Lua Event Bus System Implementation

Central event dispatcher for decoupled communication.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA

#include "lua_events.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

// Event subscriber structure
typedef struct event_subscriber_s {
	lua_State *L;
	int callback_ref;  // Lua function reference
	qboolean active;
} event_subscriber_t;

// Event name to subscribers mapping
typedef struct event_entry_s {
	char event_name[64];
	event_subscriber_t subscribers[MAX_EVENT_SUBSCRIBERS];
	int num_subscribers;
} event_entry_t;

// Event queue entry
typedef struct queued_event_s {
	char event_name[64];
	lua_State *source_L;  // Lua state that emitted the event (if from Lua)
	int num_args;
	int arg_refs[MAX_EVENT_ARGS];  // Lua references for arguments
	qboolean from_lua;
} queued_event_t;

// Global event bus state
static event_entry_t *s_event_map = NULL;
static int s_event_map_size = 0;
static int s_event_map_capacity = 0;

static queued_event_t s_event_queue[MAX_EVENT_QUEUE];
static int s_event_queue_head = 0;
static int s_event_queue_tail = 0;
static int s_event_queue_count = 0;

static qboolean s_initialized = qfalse;

#define INITIAL_EVENT_MAP_SIZE 32

/*
=================
Lua_Events_Init
Initialize the event bus system
=================
*/
void Lua_Events_Init(void)
{
	if (s_initialized) {
		return;
	}

	s_event_map_capacity = INITIAL_EVENT_MAP_SIZE;
	s_event_map = (event_entry_t *)Z_Malloc(sizeof(event_entry_t) * s_event_map_capacity);
	memset(s_event_map, 0, sizeof(event_entry_t) * s_event_map_capacity);
	s_event_map_size = 0;

	memset(s_event_queue, 0, sizeof(s_event_queue));
	s_event_queue_head = 0;
	s_event_queue_tail = 0;
	s_event_queue_count = 0;

	s_initialized = qtrue;
}

/*
=================
Lua_Events_Shutdown
Shutdown the event bus system
=================
*/
void Lua_Events_Shutdown(void)
{
	int i, j;

	if (!s_initialized) {
		return;
	}

	// Clean up all Lua references
	for (i = 0; i < s_event_map_size; i++) {
		for (j = 0; j < s_event_map[i].num_subscribers; j++) {
			event_subscriber_t *sub = &s_event_map[i].subscribers[j];
			if (sub->callback_ref != LUA_NOREF && sub->L) {
				luaL_unref(sub->L, LUA_REGISTRYINDEX, sub->callback_ref);
			}
		}
	}

	// Clean up queued event references
	for (i = 0; i < s_event_queue_count; i++) {
		int idx = (s_event_queue_head + i) % MAX_EVENT_QUEUE;
		queued_event_t *evt = &s_event_queue[idx];
		if (evt->from_lua && evt->source_L) {
			for (int k = 0; k < evt->num_args; k++) {
				if (evt->arg_refs[k] != LUA_NOREF) {
					luaL_unref(evt->source_L, LUA_REGISTRYINDEX, evt->arg_refs[k]);
				}
			}
		}
	}

	if (s_event_map) {
		Z_Free(s_event_map);
		s_event_map = NULL;
	}

	s_event_map_size = 0;
	s_event_map_capacity = 0;
	s_event_queue_count = 0;
	s_initialized = qfalse;
}

/*
=================
FindOrCreateEventEntry
Find an event entry by name, or create a new one
=================
*/
static event_entry_t *FindOrCreateEventEntry(const char *event_name)
{
	int i;

	// Find existing entry
	for (i = 0; i < s_event_map_size; i++) {
		if (Q_stricmp(s_event_map[i].event_name, event_name) == 0) {
			return &s_event_map[i];
		}
	}

	// Create new entry if we have space
	if (s_event_map_size >= s_event_map_capacity) {
		// Grow the map
		int new_capacity = s_event_map_capacity * 2;
		event_entry_t *new_map = (event_entry_t *)Z_Malloc(
			sizeof(event_entry_t) * new_capacity);
		memcpy(new_map, s_event_map, sizeof(event_entry_t) * s_event_map_size);
		memset(new_map + s_event_map_size, 0,
			sizeof(event_entry_t) * (new_capacity - s_event_map_size));
		Z_Free(s_event_map);
		s_event_map = new_map;
		s_event_map_capacity = new_capacity;
	}

	// Create new entry
	Q_strncpyz(s_event_map[s_event_map_size].event_name, event_name,
		sizeof(s_event_map[s_event_map_size].event_name));
	s_event_map[s_event_map_size].num_subscribers = 0;
	return &s_event_map[s_event_map_size++];
}

/*
=================
Lua_Events_Subscribe
Subscribe a Lua function to an event (internal)
=================
*/
static qboolean Lua_Events_Subscribe(lua_State *L, const char *event_name, int callback_ref)
{
	event_entry_t *entry;
	event_subscriber_t *sub;

	if (!s_initialized || !L || !event_name || callback_ref == LUA_NOREF) {
		return qfalse;
	}

	entry = FindOrCreateEventEntry(event_name);

	if (entry->num_subscribers >= MAX_EVENT_SUBSCRIBERS) {
		Com_Printf("Lua_Events_Subscribe: Too many subscribers for event '%s'\n", event_name);
		return qfalse;
	}

	sub = &entry->subscribers[entry->num_subscribers++];
	sub->L = L;
	sub->callback_ref = callback_ref;
	sub->active = qtrue;

	return qtrue;
}

/*
=================
Lua_Events_Unsubscribe
Unsubscribe a Lua function from an event (internal)
=================
*/
static void Lua_Events_Unsubscribe(lua_State *L, const char *event_name, int callback_ref)
{
	int i;
	event_entry_t *entry;

	if (!s_initialized) {
		return;
	}

	for (i = 0; i < s_event_map_size; i++) {
		if (Q_stricmp(s_event_map[i].event_name, event_name) == 0) {
			entry = &s_event_map[i];
			for (int k = 0; k < entry->num_subscribers; k++) {
				event_subscriber_t *sub = &entry->subscribers[k];
				if (sub->L == L && sub->callback_ref == callback_ref) {
					// Remove reference
					if (sub->callback_ref != LUA_NOREF) {
						luaL_unref(L, LUA_REGISTRYINDEX, sub->callback_ref);
					}
					// Shift remaining subscribers
					memmove(sub, sub + 1,
						sizeof(event_subscriber_t) * (entry->num_subscribers - k - 1));
					entry->num_subscribers--;
					return;
				}
			}
			break;
		}
	}
}

/*
=================
Lua_Events_SubscribeCallback
Public helper for C code to subscribe a Lua callback (registry ref) to an event
=================
*/
qboolean Lua_Events_SubscribeCallback(lua_State *L, const char *event_name, int callback_ref)
{
	return Lua_Events_Subscribe(L, event_name, callback_ref);
}

/*
=================
Lua_Events_UnsubscribeCallback
Public helper for C code to unsubscribe a Lua callback from an event
=================
*/
void Lua_Events_UnsubscribeCallback(lua_State *L, const char *event_name, int callback_ref)
{
	Lua_Events_Unsubscribe(L, event_name, callback_ref);
}

/*
=================
Lua_Events_Update
Process queued events and dispatch to subscribers
=================
*/
void Lua_Events_Update(void)
{
	int processed = 0;
	const int max_per_frame = 64;  // Limit processing per frame

	if (!s_initialized) {
		return;
	}

	while (s_event_queue_count > 0 && processed < max_per_frame) {
		queued_event_t *evt = &s_event_queue[s_event_queue_head];
		event_entry_t *entry = NULL;
		int i;

		// Find event entry
		for (i = 0; i < s_event_map_size; i++) {
			if (Q_stricmp(s_event_map[i].event_name, evt->event_name) == 0) {
				entry = &s_event_map[i];
				break;
			}
		}

		if (entry) {
			// Dispatch to all subscribers
			for (i = 0; i < entry->num_subscribers; i++) {
				event_subscriber_t *sub = &entry->subscribers[i];
				if (!sub->active || !sub->L) {
					continue;
				}

				// Get callback function
				lua_rawgeti(sub->L, LUA_REGISTRYINDEX, sub->callback_ref);
				if (!lua_isfunction(sub->L, -1)) {
					lua_pop(sub->L, 1);
					continue;
				}

				// Push event name
				lua_pushstring(sub->L, evt->event_name);

				// Push arguments
				if (evt->from_lua && evt->source_L == sub->L) {
					// Arguments are already in the same Lua state
					int j;
					for (j = 0; j < evt->num_args; j++) {
						lua_rawgeti(sub->L, LUA_REGISTRYINDEX, evt->arg_refs[j]);
					}
				} else {
					// For now, we'll skip cross-state events
					// This could be enhanced to serialize/deserialize
				}

				// Call callback (event_name, ...args)
				int num_args = evt->from_lua && evt->source_L == sub->L ? evt->num_args + 1 : 1;
				if (lua_pcall(sub->L, num_args, 0, 0) != LUA_OK) {
					const char *error = lua_tostring(sub->L, -1);
					Com_Printf("Lua_Events_Update: Error in event callback for '%s': %s\n",
						evt->event_name, error ? error : "Unknown error");
					lua_pop(sub->L, 1);
				}
			}
		}

		// Clean up event references
		if (evt->from_lua && evt->source_L) {
			int j;
			for (j = 0; j < evt->num_args; j++) {
				if (evt->arg_refs[j] != LUA_NOREF) {
					luaL_unref(evt->source_L, LUA_REGISTRYINDEX, evt->arg_refs[j]);
				}
			}
		}

		// Advance queue
		s_event_queue_head = (s_event_queue_head + 1) % MAX_EVENT_QUEUE;
		s_event_queue_count--;
		processed++;
	}
}

/*
=================
Lua_Events_Emit
Emit an event from engine code
=================
*/
void Lua_Events_Emit(const char *event_name, int num_args, ...)
{
	(void)num_args;  // Suppress unused parameter warning
	// For now, engine events are simpler - we'll queue them without Lua refs
	// This can be enhanced later to support complex data types
	if (!s_initialized || !event_name || s_event_queue_count >= MAX_EVENT_QUEUE) {
		return;
	}

	queued_event_t *evt = &s_event_queue[s_event_queue_tail];
	Q_strncpyz(evt->event_name, event_name, sizeof(evt->event_name));
	evt->source_L = NULL;
	evt->num_args = 0;  // Engine events don't use Lua refs for now
	evt->from_lua = qfalse;
	memset(evt->arg_refs, 0, sizeof(evt->arg_refs));

	s_event_queue_tail = (s_event_queue_tail + 1) % MAX_EVENT_QUEUE;
	s_event_queue_count++;
}

/*
=================
Lua_Events_EmitFromLua
Emit an event from Lua code
=================
*/
void Lua_Events_EmitFromLua(lua_State *L, const char *event_name, int num_args)
{
	int i;

	if (!s_initialized || !L || !event_name || s_event_queue_count >= MAX_EVENT_QUEUE) {
		return;
	}

	if (num_args < 0 || num_args > MAX_EVENT_ARGS) {
		Com_Printf("Lua_Events_EmitFromLua: Invalid num_args %d\n", num_args);
		return;
	}

	queued_event_t *evt = &s_event_queue[s_event_queue_tail];
	Q_strncpyz(evt->event_name, event_name, sizeof(evt->event_name));
	evt->source_L = L;
	evt->num_args = num_args;
	evt->from_lua = qtrue;

	// Store arguments as Lua references
	for (i = 0; i < num_args; i++) {
		int stack_idx = -(num_args - i);  // Arguments are on stack
		lua_pushvalue(L, stack_idx);
		evt->arg_refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	s_event_queue_tail = (s_event_queue_tail + 1) % MAX_EVENT_QUEUE;
	s_event_queue_count++;
}

// =================
// Lua Bindings
// =================

/*
=================
Lua_Events_On
Lua binding: Events.on(event_name, callback)
=================
*/
static int Lua_Events_On(lua_State *L)
{
	const char *event_name;
	int callback_ref;

	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}

	event_name = lua_tostring(L, 1);
	if (!event_name) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (!lua_isfunction(L, 2)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	// Create reference to callback function
	lua_pushvalue(L, 2);
	callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	if (Lua_Events_Subscribe(L, event_name, callback_ref)) {
		lua_pushboolean(L, 1);
	} else {
		luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
		lua_pushboolean(L, 0);
	}

	return 1;
}

/*
=================
Lua_Events_Off
Lua binding: Events.off(event_name, callback)
=================
*/
static int Lua_Events_Off(lua_State *L)
{
	const char *event_name;
	int i;
	event_entry_t *entry;

	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}

	event_name = lua_tostring(L, 1);
	if (!event_name) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (!lua_isfunction(L, 2)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	// Find event entry
	for (i = 0; i < s_event_map_size; i++) {
		if (Q_stricmp(s_event_map[i].event_name, event_name) == 0) {
			entry = &s_event_map[i];
			// Compare functions by reference
			for (int k = 0; k < entry->num_subscribers; k++) {
				event_subscriber_t *sub = &entry->subscribers[k];
				if (sub->L == L && sub->callback_ref != LUA_NOREF) {
					// Check if this is the same function
					lua_rawgeti(L, LUA_REGISTRYINDEX, sub->callback_ref);
					if (lua_rawequal(L, -1, 2)) {
						lua_pop(L, 1);
						// Found it, remove
						if (sub->callback_ref != LUA_NOREF) {
							luaL_unref(L, LUA_REGISTRYINDEX, sub->callback_ref);
						}
						// Shift remaining subscribers
						memmove(sub, sub + 1,
							sizeof(event_subscriber_t) * (entry->num_subscribers - k - 1));
						entry->num_subscribers--;
						lua_pushboolean(L, 1);
						return 1;
					}
					lua_pop(L, 1);
				}
			}
			break;
		}
	}

	lua_pushboolean(L, 0);
	return 1;
}

/*
=================
Lua_Events_Emit_Lua
Lua binding: Events.emit(event_name, ...)
=================
*/
static int Lua_Events_Emit_Lua(lua_State *L)
{
	const char *event_name;
	int num_args;

	if (lua_gettop(L) < 1) {
		return 0;
	}

	event_name = lua_tostring(L, 1);
	if (!event_name) {
		return 0;
	}

	num_args = lua_gettop(L) - 1;  // All args except event_name
	Lua_Events_EmitFromLua(L, event_name, num_args);

	return 0;
}

/*
=================
Lua_Events_RegisterBindings
Register Lua bindings for event system
=================
*/
void Lua_Events_RegisterBindings(lua_State *L)
{
	if (!L) {
		return;
	}

	// Create Events table
	lua_newtable(L);

	// Register functions
	Lua_RegisterFunction(L, "on", Lua_Events_On);
	Lua_RegisterFunction(L, "off", Lua_Events_Off);
	Lua_RegisterFunction(L, "emit", Lua_Events_Emit_Lua);

	// Set as global
	lua_setglobal(L, "Events");
}

#endif // USE_LUA

