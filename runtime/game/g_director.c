/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

AI Director implementation.
Adaptive pacing engine with per-player intensity tracking,
phase-based encounter design, and weighted spawn selection.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_director.h"
#include <math.h>

static qboolean dirInitialized = qfalse;
static dirConfig_t dirConfig;
static dirState_t dirState;
static dirPlayerState_t players[DIRECTOR_MAX_PLAYERS];
static int numActivePlayers;
static dirSpawnBudget_t spawnTypes[DIRECTOR_MAX_SPAWN_TYPES];
static int numSpawnTypes;
static dirZone_t zones[DIRECTOR_MAX_ZONES];
static int numZones;

void Director_DefaultConfig(dirConfig_t *config) {
	config->intensityBuildRate = 0.15f;
	config->intensityDecayRate = 0.08f;
	config->intensityPeakThreshold = 0.85f;
	config->intensityRespiteThreshold = 0.25f;
	config->respiteDuration = 12.0f;
	config->peakDuration = 20.0f;
	config->buildupMinDuration = 8.0f;
	config->maxSpawnsPerWave = 6.0f;
	config->playerScaling = 1.0f;
	config->difficultyMultiplier = 1.0f;
	config->isolationBonusThreat = 0.3f;
	config->healthWeight = 0.4f;
	config->ammoWeight = 0.2f;
	config->distanceWeight = 0.4f;
}

void Director_Init(void) {
	if (dirInitialized) return;

	Director_DefaultConfig(&dirConfig);
	Com_Memset(&dirState, 0, sizeof(dirState));
	Com_Memset(players, 0, sizeof(players));
	Com_Memset(spawnTypes, 0, sizeof(spawnTypes));
	Com_Memset(zones, 0, sizeof(zones));

	dirState.phase = DIR_PHASE_BUILDUP;
	numActivePlayers = 0;
	numSpawnTypes = 0;
	numZones = 0;

	dirInitialized = qtrue;
	Com_Printf("AI Director initialized\n");
}

void Director_Shutdown(void) {
	if (!dirInitialized) return;
	dirInitialized = qfalse;
	Com_Printf("AI Director shut down\n");
}

void Director_SetConfig(const dirConfig_t *config) {
	if (config) Com_Memcpy(&dirConfig, config, sizeof(dirConfig_t));
}

static void Director_ComputePlayerIntensity(dirPlayerState_t *p, float dt) {
	float healthFactor = 1.0f - (p->health / 100.0f);
	float ammoFactor = 1.0f - p->ammoRatio;
	float distFactor = p->distanceFromGroup / 500.0f;
	if (distFactor > 1.0f) distFactor = 1.0f;

	float threatInput = healthFactor * dirConfig.healthWeight +
		ammoFactor * dirConfig.ammoWeight +
		distFactor * dirConfig.distanceWeight;

	if (p->isolated) {
		threatInput += dirConfig.isolationBonusThreat;
	}

	float recentCombat = p->timeSinceLastDamage < 3.0f ? 0.3f : 0.0f;
	float recentKills = p->timeSinceLastKill < 5.0f ? 0.1f : 0.0f;
	threatInput += recentCombat + recentKills;

	if (threatInput > 1.0f) threatInput = 1.0f;

	float targetIntensity = threatInput * dirConfig.difficultyMultiplier;
	float rate = (targetIntensity > p->intensity) ? dirConfig.intensityBuildRate : dirConfig.intensityDecayRate;
	p->intensity += (targetIntensity - p->intensity) * rate * dt;

	if (p->intensity < 0.0f) p->intensity = 0.0f;
	if (p->intensity > 1.0f) p->intensity = 1.0f;

	p->stress = p->intensity * 0.7f + healthFactor * 0.3f;
	p->timeSinceLastDamage += dt;
	p->timeSinceLastKill += dt;
}

static void Director_UpdatePhase(float dt) {
	dirState.phaseTimer += dt;
	dirState.timeSinceLastWave += dt;
	dirState.sessionTime += dt;

	float avgIntensity = dirState.globalIntensity;

	switch (dirState.phase) {
		case DIR_PHASE_BUILDUP:
			if (dirState.phaseTimer >= dirConfig.buildupMinDuration &&
				avgIntensity >= dirConfig.intensityPeakThreshold * 0.5f) {
				dirState.phase = DIR_PHASE_SUSTAIN;
				dirState.phaseTimer = 0;
			}
			break;

		case DIR_PHASE_SUSTAIN:
			if (avgIntensity >= dirConfig.intensityPeakThreshold) {
				dirState.phase = DIR_PHASE_PEAK;
				dirState.phaseTimer = 0;
			}
			break;

		case DIR_PHASE_PEAK:
			if (dirState.phaseTimer >= dirConfig.peakDuration ||
				avgIntensity < dirConfig.intensityPeakThreshold * 0.6f) {
				dirState.phase = DIR_PHASE_RESPITE;
				dirState.phaseTimer = 0;
			}
			break;

		case DIR_PHASE_RESPITE:
			if (dirState.phaseTimer >= dirConfig.respiteDuration) {
				dirState.phase = DIR_PHASE_RELAX;
				dirState.phaseTimer = 0;
			}
			break;

		case DIR_PHASE_RELAX:
			if (avgIntensity >= dirConfig.intensityRespiteThreshold * 2.0f ||
				dirState.phaseTimer >= dirConfig.respiteDuration * 2.0f) {
				dirState.phase = DIR_PHASE_BUILDUP;
				dirState.phaseTimer = 0;
			}
			break;

		default:
			break;
	}
}

void Director_Update(float dt) {
	int i;
	float totalIntensity = 0;

	if (!dirInitialized) return;

	numActivePlayers = 0;
	for (i = 0; i < DIRECTOR_MAX_PLAYERS; i++) {
		if (!players[i].alive) continue;
		Director_ComputePlayerIntensity(&players[i], dt);
		totalIntensity += players[i].intensity;
		numActivePlayers++;

		float minGroupDist = 99999.0f;
		int j;
		for (j = 0; j < DIRECTOR_MAX_PLAYERS; j++) {
			if (j == i || !players[j].alive) continue;
			float d = Distance(players[i].position, players[j].position);
			if (d < minGroupDist) minGroupDist = d;
		}
		players[i].distanceFromGroup = (minGroupDist < 99999.0f) ? minGroupDist : 0.0f;
		players[i].isolated = (minGroupDist > 400.0f) ? qtrue : qfalse;
	}

	if (numActivePlayers > 0) {
		dirState.globalIntensity = totalIntensity / (float)numActivePlayers;
	} else {
		dirState.globalIntensity = 0;
	}

	float zoneThreatMax = 0;
	for (i = 0; i < numZones; i++) {
		if (!zones[i].active) continue;
		float threat = (float)zones[i].baseThreat / 4.0f;
		if (threat > zoneThreatMax) zoneThreatMax = threat;
	}
	dirState.globalThreat = dirState.globalIntensity * 0.7f + zoneThreatMax * 0.3f;

	Director_UpdatePhase(dt);
}

void Director_GetState(dirState_t *state) {
	if (state) Com_Memcpy(state, &dirState, sizeof(dirState_t));
}

void Director_UpdatePlayer(int cn, const vec3_t pos, float health, float ammo, qboolean alive) {
	if (cn < 0 || cn >= DIRECTOR_MAX_PLAYERS) return;
	players[cn].clientNum = cn;
	VectorCopy(pos, players[cn].position);
	players[cn].health = health;
	players[cn].ammoRatio = ammo;
	players[cn].alive = alive;
}

void Director_PlayerKill(int cn) {
	if (cn < 0 || cn >= DIRECTOR_MAX_PLAYERS) return;
	players[cn].killCount++;
	players[cn].timeSinceLastKill = 0;
	dirState.totalKills++;
}

void Director_PlayerDeath(int cn) {
	if (cn < 0 || cn >= DIRECTOR_MAX_PLAYERS) return;
	players[cn].deathCount++;
	players[cn].alive = qfalse;
	dirState.totalDeaths++;
}

void Director_PlayerDamage(int cn, float amount) {
	if (cn < 0 || cn >= DIRECTOR_MAX_PLAYERS) return;
	players[cn].timeSinceLastDamage = 0;
	players[cn].intensity += amount * 0.005f;
	if (players[cn].intensity > 1.0f) players[cn].intensity = 1.0f;
}

float Director_GetPlayerIntensity(int cn) {
	return (cn >= 0 && cn < DIRECTOR_MAX_PLAYERS) ? players[cn].intensity : 0;
}

float Director_GetPlayerStress(int cn) {
	return (cn >= 0 && cn < DIRECTOR_MAX_PLAYERS) ? players[cn].stress : 0;
}

void Director_GetPlayerState(int cn, dirPlayerState_t *state) {
	if (cn >= 0 && cn < DIRECTOR_MAX_PLAYERS && state)
		Com_Memcpy(state, &players[cn], sizeof(dirPlayerState_t));
}

int Director_AddSpawnType(const char *name, int maxActive, float cooldown,
                          float minIntensity, float maxIntensity, float weight) {
	if (numSpawnTypes >= DIRECTOR_MAX_SPAWN_TYPES) return -1;
	int idx = numSpawnTypes++;
	Q_strncpyz(spawnTypes[idx].typeName, name, sizeof(spawnTypes[idx].typeName));
	spawnTypes[idx].typeId = idx;
	spawnTypes[idx].maxActive = maxActive;
	spawnTypes[idx].currentActive = 0;
	spawnTypes[idx].spawnCooldown = cooldown;
	spawnTypes[idx].lastSpawnTime = -cooldown;
	spawnTypes[idx].minIntensity = minIntensity;
	spawnTypes[idx].maxIntensity = maxIntensity;
	spawnTypes[idx].weight = weight;
	return idx;
}

void Director_SpawnTypeActivated(int id) {
	if (id >= 0 && id < numSpawnTypes) spawnTypes[id].currentActive++;
}

void Director_SpawnTypeDeactivated(int id) {
	if (id >= 0 && id < numSpawnTypes && spawnTypes[id].currentActive > 0)
		spawnTypes[id].currentActive--;
}

qboolean Director_ShouldSpawn(int id) {
	if (id < 0 || id >= numSpawnTypes) return qfalse;
	dirSpawnBudget_t *s = &spawnTypes[id];
	if (s->currentActive >= s->maxActive) return qfalse;
	if (dirState.sessionTime - s->lastSpawnTime < s->spawnCooldown) return qfalse;
	if (dirState.globalIntensity < s->minIntensity) return qfalse;
	if (dirState.globalIntensity > s->maxIntensity) return qfalse;
	if (dirState.phase == DIR_PHASE_RESPITE || dirState.phase == DIR_PHASE_RELAX) return qfalse;
	return qtrue;
}

int Director_PickSpawnType(void) {
	float totalWeight = 0;
	float roll, accum;
	int i;

	for (i = 0; i < numSpawnTypes; i++) {
		if (Director_ShouldSpawn(i)) totalWeight += spawnTypes[i].weight;
	}
	if (totalWeight <= 0) return -1;

	roll = ((float)(rand() & 0x7FFF) / 0x7FFF) * totalWeight;
	accum = 0;
	for (i = 0; i < numSpawnTypes; i++) {
		if (!Director_ShouldSpawn(i)) continue;
		accum += spawnTypes[i].weight;
		if (roll <= accum) {
			spawnTypes[i].lastSpawnTime = dirState.sessionTime;
			return i;
		}
	}
	return -1;
}

int Director_AddZone(const char *name, const vec3_t mins, const vec3_t maxs,
                     dirThreat_t threat, float budgetMult) {
	if (numZones >= DIRECTOR_MAX_ZONES) return -1;
	int idx = numZones++;
	Q_strncpyz(zones[idx].name, name, sizeof(zones[idx].name));
	VectorCopy(mins, zones[idx].mins);
	VectorCopy(maxs, zones[idx].maxs);
	zones[idx].baseThreat = threat;
	zones[idx].budgetMultiplier = budgetMult;
	zones[idx].active = qtrue;
	return idx;
}

void Director_SetZoneActive(int id, qboolean active) {
	if (id >= 0 && id < numZones) zones[id].active = active;
}

dirThreat_t Director_GetZoneThreat(int id) {
	return (id >= 0 && id < numZones) ? zones[id].baseThreat : DIR_THREAT_NONE;
}

dirPhase_t Director_GetPhase(void) { return dirState.phase; }
float Director_GetGlobalIntensity(void) { return dirState.globalIntensity; }

void Director_ForcePhase(dirPhase_t phase) {
	dirState.phase = phase;
	dirState.phaseTimer = 0;
}

void Director_TriggerWave(float intensityBoost) {
	dirState.globalIntensity += intensityBoost;
	if (dirState.globalIntensity > 1.0f) dirState.globalIntensity = 1.0f;
	dirState.waveCount++;
	dirState.timeSinceLastWave = 0;
}
