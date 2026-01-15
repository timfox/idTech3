/*
=============================================================================
id Tech 3 - Single Player Enhancements

Single player features inspired by EntityPlus mod.
Provides mission objectives, checkpoints, enemy spawning, and more.
=============================================================================
*/

#pragma once

#include "../common/q_shared.h"
#include "g_public.h"

// Single Player Game Modes
typedef enum {
    SP_MODE_NONE,
    SP_MODE_ARENA,      // Arena-style combat
    SP_MODE_STORY,      // Story-driven campaign
    SP_MODE_SURVIVAL,   // Survival mode
    SP_MODE_TUTORIAL    // Tutorial mode
} sp_game_mode_t;

// Objective Types
typedef enum {
    SP_OBJECTIVE_KILL,          // Kill X enemies
    SP_OBJECTIVE_COLLECT,       // Collect X items
    SP_OBJECTIVE_REACH,          // Reach location
    SP_OBJECTIVE_PROTECT,        // Protect entity
    SP_OBJECTIVE_DESTROY,        // Destroy target
    SP_OBJECTIVE_INTERACT,       // Interact with object
    SP_OBJECTIVE_TIMED           // Complete within time limit
} sp_objective_type_t;

// Objective Status
typedef enum {
    SP_OBJECTIVE_INACTIVE,
    SP_OBJECTIVE_ACTIVE,
    SP_OBJECTIVE_COMPLETED,
    SP_OBJECTIVE_FAILED
} sp_objective_status_t;

// Single Player Objective
typedef struct {
    char name[MAX_QPATH];
    char description[MAX_QPATH];
    sp_objective_type_t type;
    sp_objective_status_t status;
    qboolean required;
    
    // Progress tracking
    int targetValue;
    int currentValue;
    
    // Location-based objectives
    vec3_t targetLocation;
    float targetRadius;
    
    // Entity-based objectives
    int targetEntity;
    
    // Timing
    float timeLimit;
    float startTime;
    
    // Rewards
    int experienceReward;
    int itemReward;
} sp_objective_t;

// Single Player Checkpoint
typedef struct {
    char name[MAX_QPATH];
    vec3_t position;
    vec3_t angles;
    qboolean activated;
    float activationTime;
    
    // Save state
    int playerHealth;
    int playerArmor;
    int playerAmmo[16];  // Max weapons (WP_NUM_WEAPONS if available)
} sp_checkpoint_t;

// Single Player Enemy Spawner
typedef struct {
    char enemyType[MAX_QPATH];
    vec3_t spawnPosition;
    vec3_t spawnAngles;
    int maxSpawns;
    int currentSpawns;
    float spawnDelay;
    float lastSpawnTime;
    qboolean respawnable;
    int respawnDelay;
    int spawnedEntities[MAX_GENTITIES];
    int spawnedCount;
} sp_enemy_spawner_t;

// Single Player Level State
typedef struct {
    sp_game_mode_t gameMode;
    
    // Objectives
    sp_objective_t objectives[32];
    int objectiveCount;
    int completedObjectives;
    
    // Checkpoints
    sp_checkpoint_t checkpoints[16];
    int checkpointCount;
    int currentCheckpoint;
    
    // Enemy spawners
    sp_enemy_spawner_t spawners[64];
    int spawnerCount;
    
    // Level state
    qboolean levelStarted;
    qboolean levelCompleted;
    float levelStartTime;
    float levelTime;
    
    // Player stats
    int kills;
    int deaths;
    int secretsFound;
    int totalSecrets;
    
    // Difficulty
    int difficulty;  // 0=easy, 1=normal, 2=hard, 3=nightmare
} sp_level_state_t;

// External API
void G_SP_Init(void);
void G_SP_Shutdown(void);
void G_SP_Update(void);

// Level management
void G_SP_LoadLevel(const char* mapname);
void G_SP_UnloadLevel(void);
sp_level_state_t* G_SP_GetLevelState(void);

// Objectives
sp_objective_t* G_SP_CreateObjective(const char* name, sp_objective_type_t type);
void G_SP_UpdateObjective(const char* name, int value);
void G_SP_CompleteObjective(const char* name);
void G_SP_FailObjective(const char* name);
qboolean G_SP_AllRequiredObjectivesComplete(void);

// Checkpoints
sp_checkpoint_t* G_SP_CreateCheckpoint(const char* name, vec3_t position, vec3_t angles);
void G_SP_ActivateCheckpoint(const char* name);
void G_SP_SaveToCheckpoint(const char* name);
void G_SP_LoadFromCheckpoint(const char* name);

// Enemy spawning
sp_enemy_spawner_t* G_SP_CreateEnemySpawner(const char* enemyType, vec3_t position, vec3_t angles);
void G_SP_UpdateSpawners(void);
void G_SP_SpawnEnemy(sp_enemy_spawner_t* spawner);

// Game mode
void G_SP_SetGameMode(sp_game_mode_t mode);
sp_game_mode_t G_SP_GetGameMode(void);

// Difficulty
void G_SP_SetDifficulty(int difficulty);
int G_SP_GetDifficulty(void);

// Statistics
void G_SP_AddKill(void);
void G_SP_AddDeath(void);
void G_SP_AddSecret(void);
int G_SP_GetKills(void);
int G_SP_GetDeaths(void);
int G_SP_GetSecretsFound(void);
