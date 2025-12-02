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
	int i;

	if (lua_gettop(L) < 1) {
		return 0;
	}

	name = lua_tostring(L, 1);
	if (!name) {
		return 0;
	}

	for (i = 0; i < s_num_encounters; i++) {
		if (Q_stricmp(s_encounters[i].name, name) == 0 && s_encounters[i].active) {
			encounter_t *enc = &s_encounters[i];
			enc->state = ENCOUNTER_ACTIVE;
			enc->current_wave = 0;

			// Call on_start hook if present
			lua_rawgeti(L, LUA_REGISTRYINDEX, enc->script_ref);
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "on_start");
				if (lua_isfunction(L, -1)) {
					if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
						const char *error = lua_tostring(L, -1);
						Com_Printf("Lua_Encounter_Start: Error in on_start: %s\n",
							error ? error : "Unknown error");
						lua_pop(L, 1);
					}
				} else {
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);

			break;
		}
	}

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
	Lua_RegisterFunction(L, "define", Lua_Encounter_Define);
	Lua_RegisterFunction(L, "start", Lua_Encounter_Start);

	// Set as global
	lua_setglobal(L, "Encounter");
}

#endif // USE_LUA

