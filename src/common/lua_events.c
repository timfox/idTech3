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
	qboolean once;     // One-time subscription
	lua_event_filter_t filter;  // Optional filter function
	char script_name[64]; // For hot-reload tracking
} event_subscriber_t;

// Waiting coroutine structure
typedef struct event_waiter_s {
	lua_State *L;
	int coroutine_ref;  // Lua coroutine reference
	int timeout_frame;  // Frame when timeout expires (0 = no timeout)
	qboolean active;
} event_waiter_t;

// Event name to subscribers mapping
typedef struct event_entry_s {
	char event_name[64];
	event_subscriber_t subscribers[MAX_EVENT_SUBSCRIBERS];
	int num_subscribers;
	event_waiter_t waiters[MAX_EVENT_WAITERS];
	int num_waiters;
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

static void Lua_Events_RemoveSubscriber(event_entry_t *entry, int index)
{
	event_subscriber_t *sub;

	if (!entry || index < 0 || index >= entry->num_subscribers) {
		return;
	}

	sub = &entry->subscribers[index];
	if (sub->callback_ref != LUA_NOREF && sub->L) {
		luaL_unref(sub->L, LUA_REGISTRYINDEX, sub->callback_ref);
	}

	memmove(sub, sub + 1, sizeof(event_subscriber_t) * (entry->num_subscribers - index - 1));
	entry->num_subscribers--;
}

static void Lua_Events_RemoveWaiter(event_entry_t *entry, int index)
{
	event_waiter_t *waiter;

	if (!entry || index < 0 || index >= entry->num_waiters) {
		return;
	}

	waiter = &entry->waiters[index];
	if (waiter->coroutine_ref != LUA_NOREF && waiter->L) {
		luaL_unref(waiter->L, LUA_REGISTRYINDEX, waiter->coroutine_ref);
	}

	memmove(waiter, waiter + 1, sizeof(event_waiter_t) * (entry->num_waiters - index - 1));
	entry->num_waiters--;
}

static void Lua_Events_StoreScriptName(lua_State *L, event_subscriber_t *sub)
{
	lua_Debug dbg;
	const char *source = NULL;
	const char *base = NULL;

	if (!sub) {
		return;
	}

	sub->script_name[0] = '\0';
	if (!L) {
		return;
	}

	if (!lua_getstack(L, 1, &dbg) || !lua_getinfo(L, "S", &dbg) || !dbg.source) {
		return;
	}

	source = dbg.source;
	if (source[0] == '@') {
		source++;
	}

	base = strrchr(source, '/');
	if (!base) {
		base = strrchr(source, '\\');
	}
	if (base) {
		source = base + 1;
	}

	Q_strncpyz(sub->script_name, source, sizeof(sub->script_name));
}

static lua_State *Lua_Events_GetWaiterThread(event_waiter_t *waiter)
{
	lua_State *co = NULL;

	if (!waiter || !waiter->L || waiter->coroutine_ref == LUA_NOREF) {
		return NULL;
	}

	lua_rawgeti(waiter->L, LUA_REGISTRYINDEX, waiter->coroutine_ref);
	if (lua_isthread(waiter->L, -1)) {
		co = lua_tothread(waiter->L, -1);
	}
	lua_pop(waiter->L, 1);

	return co;
}

static qboolean Lua_Events_ResumeWaiter(event_waiter_t *waiter, const queued_event_t *evt, qboolean timed_out)
{
	lua_State *co;
	int num_args = 0;
	int result;

	co = Lua_Events_GetWaiterThread(waiter);
	if (!co) {
		return qtrue;
	}

	if (timed_out) {
		lua_pushnil(co);
		lua_pushstring(co, "timeout");
		num_args = 2;
	} else {
		lua_pushstring(co, evt->event_name);
		num_args = 1;
		if (evt->from_lua && evt->source_L == waiter->L) {
			for (int j = 0; j < evt->num_args; j++) {
				lua_rawgeti(waiter->L, LUA_REGISTRYINDEX, evt->arg_refs[j]);
				lua_xmove(waiter->L, co, 1);
				num_args++;
			}
		}
	}

	result = lua_resume(co, NULL, num_args, NULL);
	if (result == LUA_OK) {
		return qtrue;
	}
	if (result == LUA_YIELD) {
		return qtrue;
	}

	{
		const char *error = lua_tostring(co, -1);
		Com_Printf("Lua_Events: Error resuming waiter: %s\n", error ? error : "Unknown error");
		lua_pop(co, 1);
	}

	return qtrue;
}

static void Lua_Events_ProcessTimeouts(int now_ms)
{
	for (int i = 0; i < s_event_map_size; i++) {
		event_entry_t *entry = &s_event_map[i];
		for (int j = 0; j < entry->num_waiters; j++) {
			event_waiter_t *waiter = &entry->waiters[j];
			if (!waiter->active || !waiter->L) {
				Lua_Events_RemoveWaiter(entry, j);
				j--;
				continue;
			}
			if (waiter->timeout_frame > 0 && now_ms >= waiter->timeout_frame) {
				Lua_Events_ResumeWaiter(waiter, NULL, qtrue);
				Lua_Events_RemoveWaiter(entry, j);
				j--;
			}
		}
	}
}

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
		for (j = 0; j < s_event_map[i].num_waiters; j++) {
			event_waiter_t *waiter = &s_event_map[i].waiters[j];
			if (waiter->coroutine_ref != LUA_NOREF && waiter->L) {
				luaL_unref(waiter->L, LUA_REGISTRYINDEX, waiter->coroutine_ref);
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
Lua_Events_GrowMap
Grow the event map capacity
=================
*/
static qboolean Lua_Events_GrowMap(void)
{
	int new_capacity = s_event_map_capacity * 2;
	event_entry_t *new_map = (event_entry_t *)Z_Malloc(sizeof(event_entry_t) * new_capacity);

	if (!new_map) {
		return qfalse;
	}

	Com_Memcpy(new_map, s_event_map, sizeof(event_entry_t) * s_event_map_size);
	memset(new_map + s_event_map_size, 0,
		sizeof(event_entry_t) * (new_capacity - s_event_map_size));
	Z_Free(s_event_map);
	s_event_map = new_map;
	s_event_map_capacity = new_capacity;

	return qtrue;
}

/*
=================
Lua_Events_Subscribe
Subscribe a Lua function to an event (internal)
=================
*/
static qboolean Lua_Events_Subscribe(lua_State *L, const char *event_name, int callback_ref, qboolean once)
{
	int i;
	event_entry_t *entry = NULL;

	if (!s_initialized || !L || !event_name || callback_ref == LUA_NOREF) {
		return qfalse;
	}

	// Find or create event entry
	for (i = 0; i < s_event_map_size; i++) {
		if (Q_stricmp(s_event_map[i].event_name, event_name) == 0) {
			entry = &s_event_map[i];
			break;
		}
	}

	if (i >= s_event_map_size) {
		// Event doesn't exist yet, create it
		if (s_event_map_size >= s_event_map_capacity) {
			if (!Lua_Events_GrowMap()) {
				return qfalse;
			}
		}
		entry = &s_event_map[s_event_map_size];
		Q_strncpyz(entry->event_name, event_name, sizeof(entry->event_name));
		entry->num_subscribers = 0;
		entry->num_waiters = 0;
		s_event_map_size++;
	}

	// Check if we have space for another subscriber
	if (entry->num_subscribers >= MAX_EVENT_SUBSCRIBERS) {
		return qfalse;
	}

	// Add subscriber
	event_subscriber_t *sub = &entry->subscribers[entry->num_subscribers];
	sub->L = L;
	sub->callback_ref = callback_ref;
	sub->active = qtrue;
	sub->once = once;
	sub->filter = NULL;
	Lua_Events_StoreScriptName(L, sub);
	entry->num_subscribers++;
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
	return Lua_Events_Subscribe(L, event_name, callback_ref, qfalse);
}

/*
=================
Lua_Events_SubscribeOnce
Subscribe a Lua callback (registry ref) to an event for one-time execution
=================
*/
qboolean Lua_Events_SubscribeOnce(lua_State *L, const char *event_name, int callback_ref)
{
	return Lua_Events_Subscribe(L, event_name, callback_ref, qtrue);
}

/*
=================
Lua_Events_WaitFor
Lua coroutine support: Wait for an event and resume with event data.
Yields the current coroutine and resumes with (event_name, ...args)
or (nil, "timeout") if a timeout is set.
=================
*/
int Lua_Events_WaitFor(lua_State *L, const char *event_name, int timeout_ms)
{
	int i;
	event_entry_t *entry = NULL;
	int coroutine_ref;
	int is_main_thread;

	if (!s_initialized || !L || !event_name) {
		lua_pushboolean(L, 0);
		return 1;
	}

	// Find or create event entry
	for (i = 0; i < s_event_map_size; i++) {
		if (Q_stricmp(s_event_map[i].event_name, event_name) == 0) {
			entry = &s_event_map[i];
			break;
		}
	}

	if (i >= s_event_map_size) {
		// Event doesn't exist yet, create it
		if (s_event_map_size >= s_event_map_capacity) {
			if (!Lua_Events_GrowMap()) {
				lua_pushboolean(L, 0);
				return 1;
			}
		}
		entry = &s_event_map[s_event_map_size];
		Q_strncpyz(entry->event_name, event_name, sizeof(entry->event_name));
		entry->num_subscribers = 0;
		entry->num_waiters = 0;
		s_event_map_size++;
	}

	// Check if we have space for another waiter
	if (entry->num_waiters >= MAX_EVENT_WAITERS) {
		lua_pushboolean(L, 0);
		return 1;
	}

	is_main_thread = lua_pushthread(L);
	if (is_main_thread) {
		lua_pop(L, 1);
		lua_pushboolean(L, 0);
		return 1;
	}
	coroutine_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	// Add to waiters
	event_waiter_t *waiter = &entry->waiters[entry->num_waiters];
	waiter->L = L;
	waiter->coroutine_ref = coroutine_ref;
	waiter->timeout_frame = timeout_ms > 0 ? (Sys_Milliseconds() + timeout_ms) : 0;
	waiter->active = qtrue;
	entry->num_waiters++;

	return lua_yield(L, 0);
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
	int now_ms;

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
					Lua_Events_RemoveSubscriber(entry, i);
					i--;
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

				if (sub->once) {
					Lua_Events_RemoveSubscriber(entry, i);
					i--;
				}
			}

			// Resume waiters for this event
			for (i = 0; i < entry->num_waiters; i++) {
				event_waiter_t *waiter = &entry->waiters[i];
				if (!waiter->active || !waiter->L) {
					Lua_Events_RemoveWaiter(entry, i);
					i--;
					continue;
				}
				Lua_Events_ResumeWaiter(waiter, evt, qfalse);
				Lua_Events_RemoveWaiter(entry, i);
				i--;
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

	now_ms = Sys_Milliseconds();
	Lua_Events_ProcessTimeouts(now_ms);
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

	if (Lua_Events_Subscribe(L, event_name, callback_ref, qfalse)) {
		lua_pushboolean(L, 1);
	} else {
		luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
		lua_pushboolean(L, 0);
	}

	return 1;
}

/*
=================
Lua_Events_Once
Lua binding: Events.once(event_name, callback)
=================
*/
static int Lua_Events_Once(lua_State *L)
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

	if (Lua_Events_Subscribe(L, event_name, callback_ref, qtrue)) {
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
Lua_Events_Filter_Lua
Lua binding: Events.filter(event_name, callback, filter_func)
=================
*/
static int Lua_Events_Filter_Lua(lua_State *L)
{
	// This is a placeholder - full implementation would need complex filter logic
	// For now, just return false (not implemented)
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
Lua_Events_WaitFor_Lua
Lua binding: Events.wait_for(event_name, timeout_ms)
=================
*/
static int Lua_Events_WaitFor_Lua(lua_State *L)
{
	const char *event_name;
	int timeout_ms = 0;

	if (lua_gettop(L) < 1) {
		lua_pushboolean(L, 0);
		return 1;
	}

	event_name = lua_tostring(L, 1);
	if (!event_name) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (lua_gettop(L) >= 2) {
		timeout_ms = (int)lua_tointeger(L, 2);
	}

	return Lua_Events_WaitFor(L, event_name, timeout_ms);
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
	Lua_RegisterFunction(L, "once", Lua_Events_Once);
	Lua_RegisterFunction(L, "off", Lua_Events_Off);
	Lua_RegisterFunction(L, "emit", Lua_Events_Emit_Lua);
	Lua_RegisterFunction(L, "wait_for", Lua_Events_WaitFor_Lua);
	Lua_RegisterFunction(L, "filter", Lua_Events_Filter_Lua);

	// Set as global
	lua_setglobal(L, "Events");
}

/*
=================
Lua_Events_HotReload
Handle hot-reload of scripts by cleaning up invalid event subscriptions.
Should be called when a script is reloaded.
=================
*/
void Lua_Events_HotReload(const char *script_name)
{
	int i, j;

	if (!s_initialized || !script_name) {
		return;
	}

	for (i = 0; i < s_event_map_size; i++) {
		event_entry_t *entry = &s_event_map[i];

		// Clean up subscribers from this script
		for (j = 0; j < entry->num_subscribers; j++) {
			event_subscriber_t *sub = &entry->subscribers[j];
			if (sub->active && Q_stricmp(sub->script_name, script_name) == 0) {
				// Unsubscribe this handler
				if (sub->callback_ref != LUA_NOREF && sub->L) {
					luaL_unref(sub->L, LUA_REGISTRYINDEX, sub->callback_ref);
				}
				sub->active = qfalse;
				sub->callback_ref = LUA_NOREF;
			}
		}

		// Clean up waiters from this script
		for (j = 0; j < entry->num_waiters; j++) {
			event_waiter_t *waiter = &entry->waiters[j];
			if (waiter->active && waiter->L) {
				// Check if this coroutine belongs to the reloaded script
				// This is a simplified check - in practice you'd need better script tracking
				lua_rawgeti(waiter->L, LUA_REGISTRYINDEX, waiter->coroutine_ref);
				if (lua_isthread(waiter->L, -1)) {
					// Resume with error to prevent hanging
					lua_State *co = lua_tothread(waiter->L, -1);
					lua_pushstring(co, "script reloaded");
					lua_error(co);
				}
				lua_pop(waiter->L, 1);

				luaL_unref(waiter->L, LUA_REGISTRYINDEX, waiter->coroutine_ref);
				waiter->active = qfalse;
				waiter->coroutine_ref = LUA_NOREF;
			}
		}
	}
}

#endif // USE_LUA

