/*
=============================================================================
id Tech 3 - Single Player Enhancements Implementation

Single player features inspired by EntityPlus mod.
=============================================================================
*/

#include "g_singleplayer.h"
#include "g_public.h"
#include "../common/qcommon.h"

// Forward declarations - level structure may not be available
// level.time is typically available from game module
#ifndef level
typedef struct {
    float time;
    // Add other level fields as needed
} level_t;
extern level_t level;
#endif

// Helper to get current time if level.time is not available
static float GetLevelTime(void)
{
    #ifdef level
    return level.time;
    #else
    // Fallback - would need to get from game system
    return 0.0f;
    #endif
}

// Global single player state
static sp_level_state_t g_spLevelState;
static qboolean g_spInitialized = qfalse;

/*
===============
G_SP_Init

Initialize single player system
===============
*/
void G_SP_Init(void)
{
    if (g_spInitialized) {
        return;
    }
    
    memset(&g_spLevelState, 0, sizeof(sp_level_state_t));
    g_spLevelState.gameMode = SP_MODE_NONE;
    g_spLevelState.difficulty = 1; // Normal by default
    
    g_spInitialized = qtrue;
    Com_Printf("Single Player system initialized\n");
}

/*
===============
G_SP_Shutdown

Shutdown single player system
===============
*/
void G_SP_Shutdown(void)
{
    if (!g_spInitialized) {
        return;
    }
    
    G_SP_UnloadLevel();
    g_spInitialized = qfalse;
    
    Com_Printf("Single Player system shutdown\n");
}

/*
===============
G_SP_Update

Update single player systems
===============
*/
void G_SP_Update(void)
{
    if (!g_spInitialized || !g_spLevelState.levelStarted) {
        return;
    }
    
    // Update level time
    float currentTime = GetLevelTime();
    g_spLevelState.levelTime = currentTime - g_spLevelState.levelStartTime;
    
    // Update spawners
    G_SP_UpdateSpawners();
    
    // Check objective time limits
    int i;
    for (i = 0; i < g_spLevelState.objectiveCount; i++) {
        sp_objective_t* obj = &g_spLevelState.objectives[i];
        
        if (obj->status == SP_OBJECTIVE_ACTIVE && obj->timeLimit > 0.0f) {
            float currentTime = GetLevelTime();
            float elapsed = currentTime - obj->startTime;
            if (elapsed >= obj->timeLimit) {
                G_SP_FailObjective(obj->name);
            }
        }
    }
}

/*
===============
G_SP_LoadLevel

Load a single player level
===============
*/
void G_SP_LoadLevel(const char* mapname)
{
    if (!g_spInitialized) {
        G_SP_Init();
    }
    
    // Reset level state
    memset(&g_spLevelState, 0, sizeof(sp_level_state_t));
    g_spLevelState.gameMode = SP_MODE_STORY; // Default to story mode
    g_spLevelState.difficulty = 1;
    
    // Level will be loaded by normal map loading system
    // This function sets up SP-specific state
    
    Com_Printf("Single Player level loaded: %s\n", mapname);
}

/*
===============
G_SP_UnloadLevel

Unload current single player level
===============
*/
void G_SP_UnloadLevel(void)
{
    g_spLevelState.levelStarted = qfalse;
    g_spLevelState.levelCompleted = qfalse;
    g_spLevelState.objectiveCount = 0;
    g_spLevelState.checkpointCount = 0;
    g_spLevelState.spawnerCount = 0;
}

/*
===============
G_SP_GetLevelState

Get current level state
===============
*/
sp_level_state_t* G_SP_GetLevelState(void)
{
    return &g_spLevelState;
}

/*
===============
G_SP_CreateObjective

Create a new objective
===============
*/
sp_objective_t* G_SP_CreateObjective(const char* name, sp_objective_type_t type)
{
    if (g_spLevelState.objectiveCount >= 32) {
        Com_Printf("WARNING: Maximum objectives reached\n");
        return NULL;
    }
    
    sp_objective_t* obj = &g_spLevelState.objectives[g_spLevelState.objectiveCount];
    memset(obj, 0, sizeof(sp_objective_t));
    
    Q_strncpyz(obj->name, name, sizeof(obj->name));
    obj->type = type;
    obj->status = SP_OBJECTIVE_INACTIVE;
    obj->required = qtrue;
    obj->targetEntity = -1;
    
    g_spLevelState.objectiveCount++;
    
    return obj;
}

/*
===============
G_SP_UpdateObjective

Update objective progress
===============
*/
void G_SP_UpdateObjective(const char* name, int value)
{
    int i;
    for (i = 0; i < g_spLevelState.objectiveCount; i++) {
        sp_objective_t* obj = &g_spLevelState.objectives[i];
        
        if (!Q_stricmp(obj->name, name) && obj->status == SP_OBJECTIVE_ACTIVE) {
            obj->currentValue = value;
            
            if (obj->currentValue >= obj->targetValue) {
                G_SP_CompleteObjective(name);
            }
            break;
        }
    }
}

/*
===============
G_SP_CompleteObjective

Mark objective as completed
===============
*/
void G_SP_CompleteObjective(const char* name)
{
    int i;
    for (i = 0; i < g_spLevelState.objectiveCount; i++) {
        sp_objective_t* obj = &g_spLevelState.objectives[i];
        
        if (!Q_stricmp(obj->name, name)) {
            if (obj->status == SP_OBJECTIVE_ACTIVE) {
                obj->status = SP_OBJECTIVE_COMPLETED;
                obj->currentValue = obj->targetValue;
                g_spLevelState.completedObjectives++;
                
                Com_Printf("Objective completed: %s\n", name);
                
                // Check if all required objectives are complete
                if (G_SP_AllRequiredObjectivesComplete()) {
                    g_spLevelState.levelCompleted = qtrue;
                    Com_Printf("All objectives completed! Level finished.\n");
                }
            }
            break;
        }
    }
}

/*
===============
G_SP_FailObjective

Mark objective as failed
===============
*/
void G_SP_FailObjective(const char* name)
{
    int i;
    for (i = 0; i < g_spLevelState.objectiveCount; i++) {
        sp_objective_t* obj = &g_spLevelState.objectives[i];
        
        if (!Q_stricmp(obj->name, name)) {
            obj->status = SP_OBJECTIVE_FAILED;
            Com_Printf("Objective failed: %s\n", name);
            break;
        }
    }
}

/*
===============
G_SP_AllRequiredObjectivesComplete

Check if all required objectives are complete
===============
*/
qboolean G_SP_AllRequiredObjectivesComplete(void)
{
    int i;
    for (i = 0; i < g_spLevelState.objectiveCount; i++) {
        sp_objective_t* obj = &g_spLevelState.objectives[i];
        
        if (obj->required && obj->status != SP_OBJECTIVE_COMPLETED) {
            return qfalse;
        }
    }
    
    return qtrue;
}

/*
===============
G_SP_CreateCheckpoint

Create a checkpoint
===============
*/
sp_checkpoint_t* G_SP_CreateCheckpoint(const char* name, vec3_t position, vec3_t angles)
{
    if (g_spLevelState.checkpointCount >= 16) {
        Com_Printf("WARNING: Maximum checkpoints reached\n");
        return NULL;
    }
    
    sp_checkpoint_t* cp = &g_spLevelState.checkpoints[g_spLevelState.checkpointCount];
    memset(cp, 0, sizeof(sp_checkpoint_t));
    
    Q_strncpyz(cp->name, name, sizeof(cp->name));
    VectorCopy(position, cp->position);
    VectorCopy(angles, cp->angles);
    
    g_spLevelState.checkpointCount++;
    
    return cp;
}

/*
===============
G_SP_ActivateCheckpoint

Activate a checkpoint
===============
*/
void G_SP_ActivateCheckpoint(const char* name)
{
    int i;
    for (i = 0; i < g_spLevelState.checkpointCount; i++) {
        sp_checkpoint_t* cp = &g_spLevelState.checkpoints[i];
        
        if (!Q_stricmp(cp->name, name)) {
            cp->activated = qtrue;
            cp->activationTime = level.time;
            g_spLevelState.currentCheckpoint = i;
            
            // Save player state
            G_SP_SaveToCheckpoint(name);
            
            Com_Printf("Checkpoint activated: %s\n", name);
            break;
        }
    }
}

/*
===============
G_SP_SaveToCheckpoint

Save player state to checkpoint
===============
*/
void G_SP_SaveToCheckpoint(const char* name)
{
    // TODO: Implement player state saving
    // This would save health, armor, ammo, position, etc.
    (void)name;
}

/*
===============
G_SP_LoadFromCheckpoint

Load player state from checkpoint
===============
*/
void G_SP_LoadFromCheckpoint(const char* name)
{
    // TODO: Implement player state loading
    // This would restore health, armor, ammo, position, etc.
    (void)name;
}

/*
===============
G_SP_CreateEnemySpawner

Create an enemy spawner
===============
*/
sp_enemy_spawner_t* G_SP_CreateEnemySpawner(const char* enemyType, vec3_t position, vec3_t angles)
{
    if (g_spLevelState.spawnerCount >= 64) {
        Com_Printf("WARNING: Maximum spawners reached\n");
        return NULL;
    }
    
    sp_enemy_spawner_t* spawner = &g_spLevelState.spawners[g_spLevelState.spawnerCount];
    memset(spawner, 0, sizeof(sp_enemy_spawner_t));
    
    Q_strncpyz(spawner->enemyType, enemyType, sizeof(spawner->enemyType));
    VectorCopy(position, spawner->spawnPosition);
    VectorCopy(angles, spawner->spawnAngles);
    spawner->maxSpawns = 1;
    spawner->spawnDelay = 1.0f;
    spawner->respawnable = qfalse;
    
    g_spLevelState.spawnerCount++;
    
    return spawner;
}

/*
===============
G_SP_UpdateSpawners

Update all enemy spawners
===============
*/
void G_SP_UpdateSpawners(void)
{
    int i;
    for (i = 0; i < g_spLevelState.spawnerCount; i++) {
        sp_enemy_spawner_t* spawner = &g_spLevelState.spawners[i];
        
        // Check if we need to spawn
        if (spawner->currentSpawns < spawner->maxSpawns) {
            float currentTime = GetLevelTime();
            float timeSinceLastSpawn = currentTime - spawner->lastSpawnTime;
            
            if (timeSinceLastSpawn >= spawner->spawnDelay) {
                G_SP_SpawnEnemy(spawner);
            }
        }
    }
}

/*
===============
G_SP_SpawnEnemy

Spawn an enemy from a spawner
===============
*/
void G_SP_SpawnEnemy(sp_enemy_spawner_t* spawner)
{
    // TODO: Implement actual enemy spawning
    // This would create a gentity_t with AI component
    // For now, just update spawner state
    
    spawner->lastSpawnTime = GetLevelTime();
    spawner->currentSpawns++;
    
    Com_Printf("Spawning enemy: %s at (%f, %f, %f)\n",
               spawner->enemyType,
               spawner->spawnPosition[0],
               spawner->spawnPosition[1],
               spawner->spawnPosition[2]);
}

/*
===============
G_SP_SetGameMode

Set game mode
===============
*/
void G_SP_SetGameMode(sp_game_mode_t mode)
{
    g_spLevelState.gameMode = mode;
}

/*
===============
G_SP_GetGameMode

Get game mode
===============
*/
sp_game_mode_t G_SP_GetGameMode(void)
{
    return g_spLevelState.gameMode;
}

/*
===============
G_SP_SetDifficulty

Set difficulty level
===============
*/
void G_SP_SetDifficulty(int difficulty)
{
    g_spLevelState.difficulty = Com_Clamp(0, 3, difficulty);
}

/*
===============
G_SP_GetDifficulty

Get difficulty level
===============
*/
int G_SP_GetDifficulty(void)
{
    return g_spLevelState.difficulty;
}

/*
===============
G_SP_AddKill

Add a kill to statistics
===============
*/
void G_SP_AddKill(void)
{
    g_spLevelState.kills++;
}

/*
===============
G_SP_AddDeath

Add a death to statistics
===============
*/
void G_SP_AddDeath(void)
{
    g_spLevelState.deaths++;
}

/*
===============
G_SP_AddSecret

Add a secret found to statistics
===============
*/
void G_SP_AddSecret(void)
{
    g_spLevelState.secretsFound++;
}

/*
===============
G_SP_GetKills

Get kill count
===============
*/
int G_SP_GetKills(void)
{
    return g_spLevelState.kills;
}

/*
===============
G_SP_GetDeaths

Get death count
===============
*/
int G_SP_GetDeaths(void)
{
    return g_spLevelState.deaths;
}

/*
===============
G_SP_GetSecretsFound

Get secrets found count
===============
*/
int G_SP_GetSecretsFound(void)
{
    return g_spLevelState.secretsFound;
}
