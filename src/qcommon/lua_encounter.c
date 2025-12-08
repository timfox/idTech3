/*
===========================================================================
Lua Encounter System Implementation

State machine for combat encounters.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA

#include "lua_encounter.h"
#include "lua_events.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdarg.h>

// Encounter state
typedef enum {
	ENCOUNTER_IDLE,
	ENCOUNTER_ACTIVE,
	ENCOUNTER_COMPLETE,
	ENCOUNTER_FAILED
} encounter_state_t;

// Encounter structure
typedef struct encounter_s {
	char name[64];
	int script_ref;  // Lua reference to encounter definition table
	encounter_state_t state;
	int current_wave;
	int enemies_remaining;
	qboolean active;
} encounter_t;

// Global encounter state
static encounter_t s_encounters[MAX_ENCOUNTERS];
static int s_num_encounters = 0;
static qboolean s_initialized = qfalse;

static encounter_t *Lua_Encounter_Find(const char *name)
{
	int i;
	if (!name) {
		return NULL;
	}
	for (i = 0; i < s_num_encounters; i++) {
		if (s_encounters[i].active && Q_stricmp(s_encounters[i].name, name) == 0) {
			return &s_encounters[i];
		}
	}
	return NULL;
}

static void Lua_Encounter_CallHook(encounter_t *enc, const char *hook_name, int num_args, ...)
{
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	int i;
	va_list args;

	if (!L || !enc || !hook_name || enc->script_ref < 0) {
		return;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, enc->script_ref);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	lua_getfield(L, -1, hook_name);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return;
	}

	va_start(args, num_args);
	for (i = 0; i < num_args; i++) {
		double arg = va_arg(args, double);
		lua_pushnumber(L, arg);
	}
	va_end(args);

	if (lua_pcall(L, num_args, 0, 0) != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		Com_Printf("Lua_Encounter: Error calling hook %s for %s: %s\n",
			hook_name, enc->name, error ? error : "Unknown error");
		lua_pop(L, 1);
	}

	// Pop encounter table
	lua_pop(L, 1);
}

/*
=================
Lua_Encounter_Init
Initialize the encounter system
=================
*/
void Lua_Encounter_Init(void)
{
	if (s_initialized) {
		return;
	}

	memset(s_encounters, 0, sizeof(s_encounters));
	s_num_encounters = 0;
	s_initialized = qtrue;
}

/*
=================
Lua_Encounter_Shutdown
Shutdown the encounter system
=================
*/
void Lua_Encounter_Shutdown(void)
{
	int i;

	if (!s_initialized) {
		return;
	}

	// Clean up Lua references
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	if (L) {
		for (i = 0; i < s_num_encounters; i++) {
			if (s_encounters[i].script_ref >= 0) {
				luaL_unref(L, LUA_REGISTRYINDEX, s_encounters[i].script_ref);
			}
		}
	}

	memset(s_encounters, 0, sizeof(s_encounters));
	s_num_encounters = 0;
	s_initialized = qfalse;
}

/*
=================
Lua_Encounter_Update
Update all active encounters
=================
*/
void Lua_Encounter_Update(void)
{
	// Encounters are primarily event-driven
	// This function can be used for time-based updates if needed
}

// =================
// Lua Bindings
// =================

/*
=================
Lua_Encounter_Define
Lua binding: Encounter.define(name, definition_table)
=================
*/
static int Lua_Encounter_Define(lua_State *L)
{
	const char *name;
	int def_ref;
	int i, slot = -1;

	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}

	name = lua_tostring(L, 1);
	if (!name) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (!lua_istable(L, 2)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	// Find existing encounter or free slot
	for (i = 0; i < s_num_encounters; i++) {
		if (Q_stricmp(s_encounters[i].name, name) == 0) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		// Find free slot
		for (i = 0; i < MAX_ENCOUNTERS; i++) {
			if (!s_encounters[i].active) {
				slot = i;
				break;
			}
		}
	}

	if (slot < 0) {
		Com_Printf("Lua_Encounter_Define: Maximum encounters reached\n");
		lua_pushboolean(L, 0);
		return 1;
	}

	// Create reference to definition table
	lua_pushvalue(L, 2);
	def_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	encounter_t *enc = &s_encounters[slot];
	Q_strncpyz(enc->name, name, sizeof(enc->name));
	enc->script_ref = def_ref;
	enc->state = ENCOUNTER_IDLE;
	enc->current_wave = 0;
	enc->enemies_remaining = 0;
	enc->active = qtrue;

	if (slot >= s_num_encounters) {
		s_num_encounters = slot + 1;
	}

	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_Encounter_Start
Lua binding: Encounter.start(name)
=================
*/
static int Lua_Encounter_Start(lua_State *L)
{
	const char *name;

	if (lua_gettop(L) < 1) {
		return 0;
	}

	name = lua_tostring(L, 1);
	if (!name) {
		return 0;
	}

	Lua_Encounter_StartByName(name);

	return 0;
}

/*
=================
Lua_Encounter_RegisterBindings
Register Lua bindings for encounter system
=================
*/
void Lua_Encounter_RegisterBindings(lua_State *L)
{
	if (!L) {
		return;
	}

	// Create Encounter table
	lua_newtable(L);

	// Register functions
	lua_pushcfunction(L, Lua_Encounter_Define);
	lua_setfield(L, -2, "define");
	lua_pushcfunction(L, Lua_Encounter_Start);
	lua_setfield(L, -2, "start");

	// Set as global
	lua_setglobal(L, "Encounter");
}

/*
=================
Lua_Encounter_StartByName
Called by engine code to start a named encounter
=================
*/
void Lua_Encounter_StartByName(const char *name)
{
	encounter_t *enc;

	if (!s_initialized || !name) {
		return;
	}

	enc = Lua_Encounter_Find(name);
	if (!enc) {
		return;
	}

	enc->state = ENCOUNTER_ACTIVE;
	enc->current_wave = 0;

	Lua_Encounter_CallHook(enc, "on_start", 0);
}

/*
=================
Lua_Encounter_OnWaveSpawn
Called by engine code when a new wave should be spawned
=================
*/
void Lua_Encounter_OnWaveSpawn(const char *name, int wave_num)
{
	encounter_t *enc;

	if (!s_initialized || !name) {
		return;
	}

	enc = Lua_Encounter_Find(name);
	if (!enc || enc->state != ENCOUNTER_ACTIVE) {
		return;
	}

	Lua_Encounter_CallHook(enc, "on_wave_spawn", 1, (double)wave_num);
}

/*
=================
Lua_Encounter_OnComplete
Called by engine code when an encounter ends
=================
*/
void Lua_Encounter_OnComplete(const char *name, qboolean success)
{
	encounter_t *enc;

	if (!s_initialized || !name) {
		return;
	}

	enc = Lua_Encounter_Find(name);
	if (!enc || enc->state == ENCOUNTER_COMPLETE || enc->state == ENCOUNTER_FAILED) {
		return;
	}

	enc->state = success ? ENCOUNTER_COMPLETE : ENCOUNTER_FAILED;

	if (success) {
		Lua_Encounter_CallHook(enc, "on_complete", 0);
	} else {
		Lua_Encounter_CallHook(enc, "on_fail", 0);
	}
}

#endif // USE_LUA

