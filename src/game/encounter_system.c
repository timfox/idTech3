/*
=============================================================================
Encounter and Sequence Authoring System Implementation
=============================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/q_common.h"
#include "g_local.h"
#include "encounter_system.h"

#define MAX_ENCOUNTERS 64
#define MAX_SEQUENCES 16
#define MAX_WORLD_STATE_ENTRIES 256
#define MAX_ENEMIES_PER_WAVE 32

// CVars
cvar_t *g_encounterSystem;
cvar_t *g_debugEncounters;

static encounter_t encounters[MAX_ENCOUNTERS];
static uint32_t encounterCount = 0;

static sequence_t sequences[MAX_SEQUENCES];
static uint32_t sequenceCount = 0;

static world_state_entry_t worldStateEntries[MAX_WORLD_STATE_ENTRIES];
static uint32_t worldStateCount = 0;

/*
=============================================================================
Encounter System Initialization
=============================================================================
*/
void G_EncounterSystem_Init( void )
{
	Com_Memset( encounters, 0, sizeof( encounters ) );
	Com_Memset( sequences, 0, sizeof( sequences ) );
	Com_Memset( worldStateEntries, 0, sizeof( worldStateEntries ) );
	
	encounterCount = 0;
	sequenceCount = 0;
	worldStateCount = 0;
	
	Com_Printf( "Encounter system initialized\n" );
}

/*
=============================================================================
Encounter System Shutdown
=============================================================================
*/
void G_EncounterSystem_Shutdown( void )
{
	// Stop all active encounters
	for ( uint32_t i = 0; i < encounterCount; i++ ) {
		if ( encounters[i].state == ENCOUNTER_STATE_ACTIVE ) {
			G_Encounter_Stop( &encounters[i] );
		}
		if ( encounters[i].enemyEntities ) {
			ri.Free( encounters[i].enemyEntities );
			encounters[i].enemyEntities = NULL;
		}
	}
	
	encounterCount = 0;
	sequenceCount = 0;
	worldStateCount = 0;
	
	Com_Printf( "Encounter system shutdown\n" );
}

/*
=============================================================================
Encounter System Update
=============================================================================
*/
void G_EncounterSystem_Update( void )
{
	if ( !g_encounterSystem || !g_encounterSystem->integer ) {
		return;
	}
	
	float currentTime = level.time * 0.001f;
	
	// Update world state transitions
	G_WorldState_Update();
	
	// Update encounters
	for ( uint32_t i = 0; i < encounterCount; i++ ) {
		encounter_t *encounter = &encounters[i];
		
		if ( encounter->state == ENCOUNTER_STATE_INACTIVE ) {
			// Check triggers
			if ( encounter->triggerType == ENCOUNTER_TRIGGER_PLAYER_PROXIMITY ) {
				// Check player proximity to trigger position
				// Note: This requires access to player entities, which should be provided
				// by the game module. For now, we use a placeholder that checks if
				// any player is within the trigger radius.
				qboolean playerNearby = qfalse;
				
				// TODO: Integrate with game entity system to find player entities
				// Example implementation (requires g_entities and level access):
				// for (int i = 0; i < level.maxclients; i++) {
				//     gentity_t *player = &g_entities[i];
				//     if (!player->inuse || !player->client) continue;
				//     float dist = Distance(encounter->triggerPosition, player->r.currentOrigin);
				//     if (dist <= encounter->triggerRadius) {
				//         playerNearby = qtrue;
				//         break;
				//     }
				// }
				
				if ( playerNearby ) {
					G_Encounter_Start( encounter );
				}
			}
			continue;
		}
		
		if ( encounter->state == ENCOUNTER_STATE_ACTIVE ) {
			// Check wave completion and spawn next wave
			qboolean waveComplete = qfalse;
			uint32_t aliveEnemyCount = 0;
			
			// Count alive enemies
			if ( encounter->enemyEntities && encounter->enemyCount > 0 ) {
				for ( uint32_t j = 0; j < encounter->enemyCount; j++ ) {
					int entityNum = encounter->enemyEntities[j];
					if ( entityNum >= 0 && entityNum < MAX_GENTITIES ) {
						gentity_t *enemy = &g_entities[entityNum];
						if ( enemy->inuse && enemy->health > 0 ) {
							aliveEnemyCount++;
						}
					}
				}
			}
			
			// Check if current wave is complete (all enemies defeated)
			if ( aliveEnemyCount == 0 && encounter->enemyCount > 0 ) {
				waveComplete = qtrue;
			}
			
			// Spawn next wave if current wave is complete
			if ( waveComplete && encounter->currentWave < encounter->waveCount - 1 ) {
				encounter->currentWave++;
				encounter->enemyCount = 0; // Reset for next wave
				// Note: Actual enemy spawning should be handled by Lua scripts via encounter callbacks
				// or by extending the encounter structure to include wave definitions
				// For now, this is a placeholder that can be extended
			}
			
			// Check completion conditions
			if ( encounter->currentWave >= encounter->waveCount - 1 && aliveEnemyCount == 0 ) {
				// All waves complete, all enemies defeated
				G_Encounter_Stop( encounter );
			} else if ( encounter->duration > 0.0f && 
			           ( currentTime - encounter->startTime ) >= encounter->duration ) {
				// Time expired
				G_Encounter_Stop( encounter );
			}
		}
	}
	
	// Update sequences
	for ( uint32_t i = 0; i < sequenceCount; i++ ) {
		sequence_t *sequence = &sequences[i];
		
		if ( !sequence->active ) {
			continue;
		}
		
		// Check if current encounter is complete
		if ( sequence->currentEncounter < sequence->encounterCount ) {
			encounter_t *currentEncounter = &sequence->encounters[sequence->currentEncounter];
			
			if ( currentEncounter->state == ENCOUNTER_STATE_COMPLETE ) {
				sequence->currentEncounter++;
				
				if ( sequence->currentEncounter < sequence->encounterCount ) {
					// Start next encounter
					G_Encounter_Start( &sequence->encounters[sequence->currentEncounter] );
				} else {
					// Sequence complete
					sequence->active = qfalse;
				}
			}
		}
	}
}

/*
=============================================================================
Create Encounter
=============================================================================
*/
encounter_t *G_Encounter_Create( const char *name, const char *scriptName )
{
	if ( encounterCount >= MAX_ENCOUNTERS ) {
		Com_Printf( "WARNING: Encounter limit reached\n" );
		return NULL;
	}
	
	encounter_t *encounter = &encounters[encounterCount];
	Com_Memset( encounter, 0, sizeof( encounter_t ) );
	
	Q_strncpyz( encounter->name, name, sizeof( encounter->name ) );
	if ( scriptName ) {
		Q_strncpyz( encounter->scriptName, scriptName, sizeof( encounter->scriptName ) );
	}
	
	encounter->state = ENCOUNTER_STATE_INACTIVE;
	encounter->enemyCount = 0;
	encounter->enemyEntities = NULL;
	encounter->waveCount = 1;
	encounter->currentWave = 0;
	
	encounterCount++;
	
	return encounter;
}

/*
=============================================================================
Start Encounter
=============================================================================
*/
void G_Encounter_Start( encounter_t *encounter )
{
	if ( !encounter || encounter->state == ENCOUNTER_STATE_ACTIVE ) {
		return;
	}
	
	encounter->state = ENCOUNTER_STATE_ACTIVE;
	encounter->startTime = level.time * 0.001f;
	encounter->currentWave = 0;
	
	// Apply world state changes
	if ( encounter->worldStateKey[0] != '\0' ) {
		G_WorldState_Set( encounter->worldStateKey, encounter->worldStateValue, 0.5f );
	}
	
	// Execute Lua script if present
	// TODO: Integrate with Lua system to execute encounter script
	// Example: if (encounter->scriptName[0] != '\0' && encounter->luaState) {
	//     lua_getglobal(encounter->luaState, "onEncounterStart");
	//     lua_pushstring(encounter->luaState, encounter->name);
	//     lua_call(encounter->luaState, 1, 0);
	// }
	
	// Spawn initial wave of enemies
	encounter->currentWave = 0;
	encounter->enemyCount = 0;
	
	// Allocate enemy entity array if we have waves
	if ( encounter->waveCount > 0 ) {
		encounter->enemyEntities = (int32_t *)ri.Malloc( MAX_ENEMIES_PER_WAVE * sizeof( int32_t ) );
		// Note: Actual enemy spawning should be handled by Lua scripts via encounter callbacks
		// or by extending the encounter structure to include wave definitions
		// For now, this is a placeholder that can be extended
	}
	
	if ( g_debugEncounters && g_debugEncounters->integer ) {
		Com_Printf( "Encounter started: %s\n", encounter->name );
	}
}

/*
=============================================================================
Stop Encounter
=============================================================================
*/
void G_Encounter_Stop( encounter_t *encounter )
{
	if ( !encounter || encounter->state != ENCOUNTER_STATE_ACTIVE ) {
		return;
	}
	
	encounter->state = ENCOUNTER_STATE_COMPLETE;
	
	// Clean up enemies
	if ( encounter->enemyEntities && encounter->enemyCount > 0 ) {
		for ( uint32_t i = 0; i < encounter->enemyCount; i++ ) {
			int entityNum = encounter->enemyEntities[i];
			if ( entityNum >= 0 && entityNum < MAX_GENTITIES ) {
				gentity_t *enemy = &g_entities[entityNum];
				if ( enemy->inuse ) {
					// Remove enemy by freeing the entity
					G_FreeEntity( enemy );
				}
			}
		}
		ri.Free( encounter->enemyEntities );
		encounter->enemyEntities = NULL;
		encounter->enemyCount = 0;
	}
	
	// Reset world state if it was modified
	if ( encounter->worldStateKey[0] != '\0' ) {
		// Reset to default value (0.0) or remove the world state entry
		G_WorldState_Set( encounter->worldStateKey, 0.0f, 0.5f );
	}
	
	if ( g_debugEncounters && g_debugEncounters->integer ) {
		Com_Printf( "Encounter stopped: %s\n", encounter->name );
	}
}

/*
=============================================================================
Set Encounter Trigger
=============================================================================
*/
void G_Encounter_SetTrigger( encounter_t *encounter, encounter_trigger_type_t type, const vec3_t position, float radius )
{
	if ( !encounter ) {
		return;
	}
	
	encounter->triggerType = type;
	VectorCopy( position, encounter->triggerPosition );
	encounter->triggerRadius = radius;
}

/*
=============================================================================
Create Sequence
=============================================================================
*/
sequence_t *G_Sequence_Create( const char *name )
{
	if ( sequenceCount >= MAX_SEQUENCES ) {
		Com_Printf( "WARNING: Sequence limit reached\n" );
		return NULL;
	}
	
	sequence_t *sequence = &sequences[sequenceCount];
	Com_Memset( sequence, 0, sizeof( sequence_t ) );
	
	Q_strncpyz( sequence->name, name, sizeof( sequence->name ) );
	sequence->encounterCount = 0;
	sequence->encounters = NULL;
	sequence->currentEncounter = 0;
	sequence->active = qfalse;
	
	sequenceCount++;
	
	return sequence;
}

/*
=============================================================================
Add Encounter to Sequence
=============================================================================
*/
void G_Sequence_AddEncounter( sequence_t *sequence, encounter_t *encounter )
{
	if ( !sequence || !encounter ) {
		return;
	}
	
	// Reallocate encounter array
	encounter_t *newEncounters = (encounter_t *)ri.Malloc( ( sequence->encounterCount + 1 ) * sizeof( encounter_t ) );
	if ( sequence->encounters ) {
		Com_Memcpy( newEncounters, sequence->encounters, sequence->encounterCount * sizeof( encounter_t ) );
		ri.Free( sequence->encounters );
	}
	
	sequence->encounters = newEncounters;
	sequence->encounters[sequence->encounterCount] = *encounter;
	sequence->encounterCount++;
}

/*
=============================================================================
Start Sequence
=============================================================================
*/
void G_Sequence_Start( sequence_t *sequence )
{
	if ( !sequence || sequence->active ) {
		return;
	}
	
	sequence->active = qtrue;
	sequence->startTime = level.time * 0.001f;
	sequence->currentEncounter = 0;
	
	if ( sequence->encounterCount > 0 ) {
		G_Encounter_Start( &sequence->encounters[0] );
	}
}

/*
=============================================================================
Stop Sequence
=============================================================================
*/
void G_Sequence_Stop( sequence_t *sequence )
{
	if ( !sequence || !sequence->active ) {
		return;
	}
	
	sequence->active = qfalse;
	
	// Stop current encounter
	if ( sequence->currentEncounter < sequence->encounterCount ) {
		G_Encounter_Stop( &sequence->encounters[sequence->currentEncounter] );
	}
}

/*
=============================================================================
Set World State
=============================================================================
*/
void G_WorldState_Set( const char *key, float value, float transitionTime )
{
	// Find existing entry
	for ( uint32_t i = 0; i < worldStateCount; i++ ) {
		if ( !Q_stricmp( worldStateEntries[i].key, key ) ) {
			world_state_entry_t *entry = &worldStateEntries[i];
			entry->targetValue = value;
			entry->transitionDuration = transitionTime;
			entry->transitionTime = 0.0f;
			return;
		}
	}
	
	// Create new entry
	if ( worldStateCount >= MAX_WORLD_STATE_ENTRIES ) {
		Com_Printf( "WARNING: World state limit reached\n" );
		return;
	}
	
	world_state_entry_t *entry = &worldStateEntries[worldStateCount];
	Q_strncpyz( entry->key, key, sizeof( entry->key ) );
	entry->value = value;
	entry->targetValue = value;
	entry->transitionTime = 0.0f;
	entry->transitionDuration = transitionTime;
	
	worldStateCount++;
}

/*
=============================================================================
Get World State
=============================================================================
*/
float G_WorldState_Get( const char *key )
{
	for ( uint32_t i = 0; i < worldStateCount; i++ ) {
		if ( !Q_stricmp( worldStateEntries[i].key, key ) ) {
			return worldStateEntries[i].value;
		}
	}
	
	return 0.0f;
}

/*
=============================================================================
Update World State
=============================================================================
*/
void G_WorldState_Update( void )
{
	float frameTime = level.frameTime * 0.001f;
	
	for ( uint32_t i = 0; i < worldStateCount; i++ ) {
		world_state_entry_t *entry = &worldStateEntries[i];
		
		if ( entry->transitionDuration > 0.0f ) {
			entry->transitionTime += frameTime;
			
			if ( entry->transitionTime >= entry->transitionDuration ) {
				entry->value = entry->targetValue;
				entry->transitionDuration = 0.0f;
			} else {
				float t = entry->transitionTime / entry->transitionDuration;
				entry->value = Lerp( entry->value, entry->targetValue, t );
			}
		}
	}
}

/*
=============================================================================
Spawn Enemy Helper
Helper function to spawn an enemy entity at a given position
=============================================================================
*/
int G_Encounter_SpawnEnemy( encounter_t *encounter, const char *classname, const vec3_t origin, const vec3_t angles )
{
	if ( !encounter || !classname || encounter->enemyCount >= MAX_ENEMIES_PER_WAVE ) {
		return -1;
	}
	
	gentity_t *enemy = G_Spawn();
	if ( !enemy ) {
		return -1;
	}
	
	// Set basic properties
	enemy->classname = classname;
	VectorCopy( origin, enemy->s.origin );
	VectorCopy( origin, enemy->s.pos.trBase );
	VectorCopy( origin, enemy->r.currentOrigin );
	if ( angles ) {
		VectorCopy( angles, enemy->s.angles );
	} else {
		VectorSet( enemy->s.angles, 0, 0, 0 );
	}
	
	// Set default health if not specified
	if ( enemy->health <= 0 ) {
		enemy->health = 100;
	}
	
	// Link entity to world
	trap_LinkEntity( enemy );
	
	// Add to encounter's enemy list
	if ( !encounter->enemyEntities ) {
		encounter->enemyEntities = (int32_t *)ri.Malloc( MAX_ENEMIES_PER_WAVE * sizeof( int32_t ) );
	}
	encounter->enemyEntities[encounter->enemyCount] = enemy - g_entities;
	encounter->enemyCount++;
	
	if ( g_debugEncounters && g_debugEncounters->integer ) {
		Com_Printf( "Encounter '%s': Spawned enemy '%s' at (%.1f, %.1f, %.1f)\n",
			encounter->name, classname, origin[0], origin[1], origin[2] );
	}
	
	return enemy - g_entities;
}

/*
=============================================================================
Register Lua Functions
=============================================================================
*/
void G_EncounterSystem_RegisterLuaFunctions( void *luaState )
{
	// TODO: Register Lua functions for encounter system
	// Example:
	// lua_register(luaState, "EncounterDefine", lua_encounter_define);
	// lua_register(luaState, "SequenceDefine", lua_sequence_define);
	// lua_register(luaState, "WorldStateSet", lua_world_state_set);
	// lua_register(luaState, "EncounterSpawnEnemy", lua_encounter_spawn_enemy);
}

