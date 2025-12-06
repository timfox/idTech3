/*
=============================================================================
Encounter and Sequence Authoring System
Provides Lua-based encounter orchestration and world state management
=============================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"

// Encounter state
typedef enum {
	ENCOUNTER_STATE_INACTIVE,
	ENCOUNTER_STATE_PENDING,
	ENCOUNTER_STATE_ACTIVE,
	ENCOUNTER_STATE_COMPLETE,
	ENCOUNTER_STATE_FAILED
} encounter_state_t;

// Encounter trigger types
typedef enum {
	ENCOUNTER_TRIGGER_PLAYER_ENTER,
	ENCOUNTER_TRIGGER_PLAYER_PROXIMITY,
	ENCOUNTER_TRIGGER_SCRIPT,
	ENCOUNTER_TRIGGER_TIME,
	ENCOUNTER_TRIGGER_EVENT
} encounter_trigger_type_t;

// Encounter structure
typedef struct {
	char name[MAX_QPATH];
	encounter_state_t state;
	encounter_trigger_type_t triggerType;
	
	// Trigger parameters
	vec3_t triggerPosition;
	float triggerRadius;
	int32_t triggerEntity;
	
	// Encounter data
	uint32_t enemyCount;
	int32_t *enemyEntities;
	uint32_t waveCount;
	uint32_t currentWave;
	
	// Scripting
	char scriptName[MAX_QPATH];
	void *luaState;
	
	// Timing
	float startTime;
	float duration;
	float nextWaveTime;
	
	// World state modifiers
	char worldStateKey[MAX_QPATH];
	float worldStateValue;
	
	// Flags
	uint32_t flags;
} encounter_t;

// Sequence structure
typedef struct {
	char name[MAX_QPATH];
	uint32_t encounterCount;
	encounter_t *encounters;
	uint32_t currentEncounter;
	qboolean active;
	float startTime;
} sequence_t;

// World state entry
typedef struct {
	char key[MAX_QPATH];
	float value;
	float targetValue;
	float transitionTime;
	float transitionDuration;
} world_state_entry_t;

// External API
void G_EncounterSystem_Init( void );
void G_EncounterSystem_Shutdown( void );
void G_EncounterSystem_Update( void );

// Encounter management
encounter_t *G_Encounter_Create( const char *name, const char *scriptName );
void G_Encounter_Start( encounter_t *encounter );
void G_Encounter_Stop( encounter_t *encounter, qboolean success );
void G_Encounter_SetTrigger( encounter_t *encounter, encounter_trigger_type_t type, const vec3_t position, float radius );

// Sequence management
sequence_t *G_Sequence_Create( const char *name );
void G_Sequence_AddEncounter( sequence_t *sequence, encounter_t *encounter );
void G_Sequence_Start( sequence_t *sequence );
void G_Sequence_Stop( sequence_t *sequence );

// World state management
void G_WorldState_Set( const char *key, float value, float transitionTime );
float G_WorldState_Get( const char *key );
void G_WorldState_Update( void );

// Enemy spawning
int G_Encounter_SpawnEnemy( encounter_t *encounter, const char *classname, const vec3_t origin, const vec3_t angles );

// Lua integration
void G_EncounterSystem_RegisterLuaFunctions( void *luaState );

// CVars
extern cvar_t *g_encounterSystem;
extern cvar_t *g_debugEncounters;

