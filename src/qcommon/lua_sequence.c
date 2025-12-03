/*
===========================================================================
Lua Sequence System Implementation

Timeline-based sequences/cinematics.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA

#include "lua_sequence.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

// Sequence step
typedef struct sequence_step_s {
	float time;
	int action_ref;  // Lua function reference
	qboolean executed;
} sequence_step_t;

// Sequence structure
typedef struct sequence_s {
	char name[64];
	sequence_step_t *steps;
	int num_steps;
	float current_time;
	float total_time;
	qboolean active;
	qboolean playing;
} sequence_t;

// Global sequence state
static sequence_t s_sequences[MAX_SEQUENCES];
static int s_num_sequences = 0;
static qboolean s_initialized = qfalse;

/*
=================
Lua_Sequence_Init
Initialize the sequence system
=================
*/
void Lua_Sequence_Init(void)
{
	if (s_initialized) {
		return;
	}

	memset(s_sequences, 0, sizeof(s_sequences));
	s_num_sequences = 0;
	s_initialized = qtrue;
}

/*
=================
Lua_Sequence_Shutdown
Shutdown the sequence system
=================
*/
void Lua_Sequence_Shutdown(void)
{
	int i, j;

	if (!s_initialized) {
		return;
	}

	// Clean up Lua references
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();
	if (L) {
		for (i = 0; i < s_num_sequences; i++) {
			if (s_sequences[i].steps) {
				for (j = 0; j < s_sequences[i].num_steps; j++) {
					if (s_sequences[i].steps[j].action_ref >= 0) {
						luaL_unref(L, LUA_REGISTRYINDEX, s_sequences[i].steps[j].action_ref);
					}
				}
				Z_Free(s_sequences[i].steps);
				s_sequences[i].steps = NULL;
			}
		}
	}

	memset(s_sequences, 0, sizeof(s_sequences));
	s_num_sequences = 0;
	s_initialized = qfalse;
}

/*
=================
Lua_Sequence_Update
Update all active sequences
=================
*/
void Lua_Sequence_Update(float deltaTime)
{
	int i, j;
	extern lua_State *Lua_GetMainState(void);
	lua_State *L = Lua_GetMainState();

	if (!L || !s_initialized) {
		return;
	}

	for (i = 0; i < s_num_sequences; i++) {
		sequence_t *seq = &s_sequences[i];
		if (!seq->active || !seq->playing || !seq->steps) {
			continue;
		}

		seq->current_time += deltaTime;

		// Execute steps that should run at this time
		for (j = 0; j < seq->num_steps; j++) {
			sequence_step_t *step = &seq->steps[j];
			if (!step->executed && seq->current_time >= step->time) {
				// Execute action
				lua_rawgeti(L, LUA_REGISTRYINDEX, step->action_ref);
				if (lua_isfunction(L, -1)) {
					if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
						const char *error = lua_tostring(L, -1);
						Com_Printf("Lua_Sequence_Update: Error in sequence step: %s\n",
							error ? error : "Unknown error");
						lua_pop(L, 1);
					}
				} else {
					lua_pop(L, 1);
				}
				step->executed = qtrue;
			}
		}

		// Check if sequence is complete
		if (seq->current_time >= seq->total_time) {
			seq->playing = qfalse;
		}
	}
}

// =================
// Lua Bindings
// =================

/*
=================
Lua_Sequence_Define
Lua binding: Sequence.define(name, steps_table)
=================
*/
static int Lua_Sequence_Define(lua_State *L)
{
	const char *name;
	int i, slot = -1;
	int num_steps;

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

	// Find existing sequence or free slot
	for (i = 0; i < s_num_sequences; i++) {
		if (Q_stricmp(s_sequences[i].name, name) == 0) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		for (i = 0; i < MAX_SEQUENCES; i++) {
			if (!s_sequences[i].active) {
				slot = i;
				break;
			}
		}
	}

	if (slot < 0) {
		Com_Printf("Lua_Sequence_Define: Maximum sequences reached\n");
		lua_pushboolean(L, 0);
		return 1;
	}

	// Count steps
	num_steps = (int)luaL_len(L, 2);
	if (num_steps <= 0) {
		lua_pushboolean(L, 0);
		return 1;
	}

	sequence_t *seq = &s_sequences[slot];
	
	// Free old steps if any
	if (seq->steps) {
		for (i = 0; i < seq->num_steps; i++) {
			if (seq->steps[i].action_ref >= 0) {
				luaL_unref(L, LUA_REGISTRYINDEX, seq->steps[i].action_ref);
			}
		}
		Z_Free(seq->steps);
	}

	// Allocate steps
	seq->steps = (sequence_step_t *)Z_Malloc(sizeof(sequence_step_t) * num_steps);
	seq->num_steps = num_steps;
	seq->total_time = 0.0f;

	// Parse steps
	for (i = 0; i < num_steps; i++) {
		lua_rawgeti(L, 2, i + 1);  // Lua arrays are 1-indexed
		if (!lua_istable(L, -1)) {
			lua_pop(L, 1);
			continue;
		}

		sequence_step_t *step = &seq->steps[i];

		// Get time
		lua_getfield(L, -1, "time");
		step->time = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		// Get action
		lua_getfield(L, -1, "action");
		if (lua_isfunction(L, -1)) {
			step->action_ref = luaL_ref(L, LUA_REGISTRYINDEX);
		} else {
			lua_pop(L, 1);
			step->action_ref = -1;
		}

		step->executed = qfalse;

		if (step->time > seq->total_time) {
			seq->total_time = step->time;
		}

		lua_pop(L, 1);  // Pop step table
	}

	Q_strncpyz(seq->name, name, sizeof(seq->name));
	seq->current_time = 0.0f;
	seq->active = qtrue;
	seq->playing = qfalse;

	if (slot >= s_num_sequences) {
		s_num_sequences = slot + 1;
	}

	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_Sequence_Play
Lua binding: Sequence.play(name)
=================
*/
static int Lua_Sequence_Play(lua_State *L)
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

	for (i = 0; i < s_num_sequences; i++) {
		if (Q_stricmp(s_sequences[i].name, name) == 0 && s_sequences[i].active) {
			sequence_t *seq = &s_sequences[i];
			seq->current_time = 0.0f;
			seq->playing = qtrue;
			
			// Reset all steps
			int j;
			for (j = 0; j < seq->num_steps; j++) {
				seq->steps[j].executed = qfalse;
			}
			break;
		}
	}

	return 0;
}

/*
=================
Lua_Sequence_RegisterBindings
Register Lua bindings for sequence system
=================
*/
void Lua_Sequence_RegisterBindings(lua_State *L)
{
	if (!L) {
		return;
	}

	// Create Sequence table
	lua_newtable(L);

	// Register functions
	Lua_RegisterFunction(L, "define", Lua_Sequence_Define);
	Lua_RegisterFunction(L, "play", Lua_Sequence_Play);

	// Set as global
	lua_setglobal(L, "Sequence");
}

#endif // USE_LUA

