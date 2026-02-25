/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Horde AI with LOD and flocking implementation.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_horde.h"

static hordeAgent_t agents[HORDE_MAX_AGENTS];
static hordeGroup_t groups[HORDE_MAX_GROUPS];
static int agentCount = 0;
static int groupCount = 0;
static hordeConfig_t config;
static float simTime = 0;

#define VALID_AGENT(h) ((h) >= 0 && (h) < HORDE_MAX_AGENTS && agents[(h)].active)

void Horde_DefaultConfig(hordeConfig_t *c) {
	c->fullLODDist = 300; c->mediumLODDist = 800; c->lowLODDist = 2000;
	c->fullUpdateRate = 0.016f; c->mediumUpdateRate = 0.1f; c->lowUpdateRate = 0.5f;
	c->flockCohesion = 0.02f; c->flockSeparation = 1.5f; c->flockAlignment = 0.05f;
	c->separationDist = 30; c->maxFullLOD = 32; c->maxMediumLOD = 64;
}

void Horde_Init(void) {
	Com_Memset(agents, 0, sizeof(agents));
	Com_Memset(groups, 0, sizeof(groups));
	agentCount = groupCount = 0; simTime = 0;
	Horde_DefaultConfig(&config);
	Com_Printf("Horde AI initialized (max %d agents, LOD tiers: %d/%d)\n",
		HORDE_MAX_AGENTS, config.maxFullLOD, config.maxMediumLOD);
}

void Horde_Shutdown(void) { agentCount = groupCount = 0; }
void Horde_SetConfig(const hordeConfig_t *c) { if (c) Com_Memcpy(&config, c, sizeof(hordeConfig_t)); }

static hordeLOD_t Horde_ComputeLOD(const vec3_t agentPos, const vec3_t playerPos) {
	float dist = Distance(agentPos, playerPos);
	if (dist < config.fullLODDist) return HORDE_LOD_FULL;
	if (dist < config.mediumLODDist) return HORDE_LOD_MEDIUM;
	if (dist < config.lowLODDist) return HORDE_LOD_LOW;
	return HORDE_LOD_DORMANT;
}

static void Horde_ApplyFlocking(hordeAgent_t *a, float dt) {
	int gi = a->groupId;
	if (gi < 0 || gi >= groupCount || !groups[gi].active) return;
	hordeGroup_t *g = &groups[gi];

	vec3_t toCenter, sep;
	VectorSubtract(g->center, a->position, toCenter);
	VectorScale(toCenter, config.flockCohesion * dt, toCenter);
	VectorAdd(a->velocity, toCenter, a->velocity);

	int i;
	VectorClear(sep);
	for (i = 0; i < HORDE_MAX_AGENTS; i++) {
		if (i == (int)(a - agents) || !agents[i].active || agents[i].groupId != gi) continue;
		float d = Distance(a->position, agents[i].position);
		if (d < config.separationDist && d > 0.01f) {
			vec3_t away;
			VectorSubtract(a->position, agents[i].position, away);
			VectorScale(away, config.flockSeparation / (d * d), away);
			VectorAdd(sep, away, sep);
		}
	}
	VectorMA(a->velocity, dt, sep, a->velocity);
}

static void Horde_UpdateAgent(hordeAgent_t *a, float dt, const vec3_t playerPos) {
	vec3_t toTarget;
	float dist, speed;

	a->lod = Horde_ComputeLOD(a->position, playerPos);

	float requiredInterval;
	switch (a->lod) {
		case HORDE_LOD_FULL:    requiredInterval = config.fullUpdateRate; break;
		case HORDE_LOD_MEDIUM:  requiredInterval = config.mediumUpdateRate; break;
		case HORDE_LOD_LOW:     requiredInterval = config.lowUpdateRate; break;
		default:                return;
	}

	if (simTime - a->lastUpdateTime < requiredInterval) return;
	a->lastUpdateTime = simTime;

	if (a->state == HORDE_STATE_DEAD) return;

	switch (a->state) {
		case HORDE_STATE_IDLE:
			dist = Distance(a->position, playerPos);
			if (dist < a->sightRange) {
				VectorCopy(playerPos, a->targetPos);
				a->targetEntity = 0;
				a->state = HORDE_STATE_CHASE;
			}
			break;

		case HORDE_STATE_WANDER:
			VectorSubtract(a->targetPos, a->position, toTarget);
			dist = VectorLength(toTarget);
			if (dist < 20) {
				a->state = HORDE_STATE_IDLE;
			} else {
				VectorNormalize(toTarget);
				VectorScale(toTarget, a->speed * 0.5f, a->velocity);
			}
			break;

		case HORDE_STATE_CHASE:
			VectorCopy(playerPos, a->targetPos);
			VectorSubtract(a->targetPos, a->position, toTarget);
			dist = VectorLength(toTarget);
			if (dist < a->attackRange) {
				a->state = HORDE_STATE_ATTACK;
			} else {
				VectorNormalize(toTarget);
				speed = a->speed;
				if (a->lod == HORDE_LOD_MEDIUM) speed *= 0.9f;
				VectorScale(toTarget, speed, a->velocity);
			}
			Horde_ApplyFlocking(a, dt);
			break;

		case HORDE_STATE_ATTACK:
			dist = Distance(a->position, playerPos);
			if (dist > a->attackRange * 1.5f) {
				a->state = HORDE_STATE_CHASE;
			}
			VectorClear(a->velocity);
			break;

		case HORDE_STATE_FLEE:
			VectorSubtract(a->position, playerPos, toTarget);
			VectorNormalize(toTarget);
			VectorScale(toTarget, a->speed * 1.2f, a->velocity);
			break;

		default: break;
	}

	VectorMA(a->position, dt, a->velocity, a->position);
}

void Horde_Update(float dt, const vec3_t playerPos) {
	int i, gi;
	simTime += dt;

	for (i = 0; i < HORDE_MAX_AGENTS; i++) {
		if (agents[i].active) Horde_UpdateAgent(&agents[i], dt, playerPos);
	}

	for (gi = 0; gi < groupCount; gi++) {
		if (!groups[gi].active) continue;
		vec3_t sum; int count = 0;
		VectorClear(sum);
		for (i = 0; i < HORDE_MAX_AGENTS; i++) {
			if (agents[i].active && agents[i].groupId == gi) {
				VectorAdd(sum, agents[i].position, sum);
				count++;
			}
		}
		groups[gi].agentCount = count;
		if (count > 0) VectorScale(sum, 1.0f / count, groups[gi].center);
	}
}

hordeHandle_t Horde_SpawnAgent(const vec3_t pos, float health, float speed, int groupId) {
	int i;
	for (i = 0; i < HORDE_MAX_AGENTS; i++) {
		if (!agents[i].active) {
			Com_Memset(&agents[i], 0, sizeof(hordeAgent_t));
			agents[i].active = qtrue;
			VectorCopy(pos, agents[i].position);
			agents[i].health = health;
			agents[i].speed = speed;
			agents[i].attackRange = 40;
			agents[i].sightRange = 600;
			agents[i].state = HORDE_STATE_IDLE;
			agents[i].lod = HORDE_LOD_DORMANT;
			agents[i].groupId = groupId;
			agents[i].navAgentId = -1;
			agentCount++;
			return i;
		}
	}
	return -1;
}

void Horde_KillAgent(hordeHandle_t h) {
	if (VALID_AGENT(h)) { agents[h].state = HORDE_STATE_DEAD; VectorClear(agents[h].velocity); }
}

void Horde_SetTarget(hordeHandle_t h, const vec3_t target, int targetEntity) {
	if (!VALID_AGENT(h)) return;
	VectorCopy(target, agents[h].targetPos);
	agents[h].targetEntity = targetEntity;
	if (agents[h].state == HORDE_STATE_IDLE) agents[h].state = HORDE_STATE_CHASE;
}

void Horde_GetAgentPos(hordeHandle_t h, vec3_t out) {
	if (VALID_AGENT(h)) VectorCopy(agents[h].position, out);
	else VectorClear(out);
}

hordeState_t Horde_GetAgentState(hordeHandle_t h) {
	return VALID_AGENT(h) ? agents[h].state : HORDE_STATE_DEAD;
}

hordeLOD_t Horde_GetAgentLOD(hordeHandle_t h) {
	return VALID_AGENT(h) ? agents[h].lod : HORDE_LOD_DORMANT;
}

int Horde_CreateGroup(const vec3_t center, float radius) {
	if (groupCount >= HORDE_MAX_GROUPS) return -1;
	int idx = groupCount++;
	Com_Memset(&groups[idx], 0, sizeof(hordeGroup_t));
	groups[idx].active = qtrue;
	VectorCopy(center, groups[idx].center);
	groups[idx].radius = radius;
	return idx;
}

void Horde_SetGroupTarget(int gi, const vec3_t target) {
	if (gi >= 0 && gi < groupCount && groups[gi].active) VectorCopy(target, groups[gi].targetCenter);
}

int Horde_GetActiveCount(void) { return agentCount; }

int Horde_GetCountByLOD(hordeLOD_t lod) {
	int i, count = 0;
	for (i = 0; i < HORDE_MAX_AGENTS; i++)
		if (agents[i].active && agents[i].lod == lod) count++;
	return count;
}
