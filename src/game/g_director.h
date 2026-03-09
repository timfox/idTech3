/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

AI Director engine system.
Provides adaptive pacing, per-player intensity tracking,
spawn budget management, and phase-based encounter design.
Exposes C API for Lua game scripts to call into.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define DIRECTOR_MAX_PLAYERS    16
#define DIRECTOR_MAX_SPAWN_TYPES 32
#define DIRECTOR_MAX_ZONES      64

typedef enum {
	DIR_PHASE_BUILDUP,
	DIR_PHASE_SUSTAIN,
	DIR_PHASE_PEAK,
	DIR_PHASE_RESPITE,
	DIR_PHASE_RELAX,
	DIR_PHASE_COUNT
} dirPhase_t;

typedef enum {
	DIR_THREAT_NONE = 0,
	DIR_THREAT_LOW,
	DIR_THREAT_MEDIUM,
	DIR_THREAT_HIGH,
	DIR_THREAT_EXTREME
} dirThreat_t;

typedef struct dirPlayerState_s {
	int         clientNum;
	float       intensity;
	float       stress;
	float       health;
	float       ammoRatio;
	float       distanceFromGroup;
	float       timeSinceLastDamage;
	float       timeSinceLastKill;
	int         killCount;
	int         deathCount;
	vec3_t      position;
	qboolean    alive;
	qboolean    isolated;
} dirPlayerState_t;

typedef struct dirSpawnBudget_s {
	int         typeId;
	char        typeName[64];
	int         maxActive;
	int         currentActive;
	float       spawnCooldown;
	float       lastSpawnTime;
	float       minIntensity;
	float       maxIntensity;
	float       weight;
} dirSpawnBudget_t;

typedef struct dirZone_s {
	char        name[64];
	vec3_t      mins;
	vec3_t      maxs;
	dirThreat_t baseThreat;
	float       budgetMultiplier;
	qboolean    active;
} dirZone_t;

typedef struct dirConfig_s {
	float   intensityBuildRate;
	float   intensityDecayRate;
	float   intensityPeakThreshold;
	float   intensityRespiteThreshold;
	float   respiteDuration;
	float   peakDuration;
	float   buildupMinDuration;
	float   maxSpawnsPerWave;
	float   playerScaling;
	float   difficultyMultiplier;
	float   isolationBonusThreat;
	float   healthWeight;
	float   ammoWeight;
	float   distanceWeight;
} dirConfig_t;

typedef struct dirState_s {
	dirPhase_t      phase;
	float           phaseTimer;
	float           globalIntensity;
	float           globalThreat;
	int             totalKills;
	int             totalDeaths;
	int             waveCount;
	float           timeSinceLastWave;
	float           sessionTime;
} dirState_t;

void        Director_Init(void);
void        Director_Shutdown(void);
void        Director_Update(float dt);
void        Director_SetConfig(const dirConfig_t *config);
void        Director_DefaultConfig(dirConfig_t *config);
void        Director_GetState(dirState_t *state);

void        Director_UpdatePlayer(int clientNum, const vec3_t pos, float health, float ammo, qboolean alive);
void        Director_PlayerKill(int clientNum);
void        Director_PlayerDeath(int clientNum);
void        Director_PlayerDamage(int clientNum, float amount);
float       Director_GetPlayerIntensity(int clientNum);
float       Director_GetPlayerStress(int clientNum);
void        Director_GetPlayerState(int clientNum, dirPlayerState_t *state);

int         Director_AddSpawnType(const char *name, int maxActive, float cooldown, float minIntensity, float maxIntensity, float weight);
void        Director_SpawnTypeActivated(int typeId);
void        Director_SpawnTypeDeactivated(int typeId);
qboolean    Director_ShouldSpawn(int typeId);
int         Director_PickSpawnType(void);

int         Director_AddZone(const char *name, const vec3_t mins, const vec3_t maxs, dirThreat_t threat, float budgetMult);
void        Director_SetZoneActive(int zoneId, qboolean active);
dirThreat_t Director_GetZoneThreat(int zoneId);

dirPhase_t  Director_GetPhase(void);
float       Director_GetGlobalIntensity(void);
void        Director_ForcePhase(dirPhase_t phase);
void        Director_TriggerWave(float intensityBoost);

#ifdef __cplusplus
}
#endif
