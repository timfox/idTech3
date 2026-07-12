/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Horde/swarm AI with LOD-based simulation.
Manages hundreds of agents with tiered update rates:
  Full AI: near players, full pathfinding + animation
  Medium: visible but distant, simplified navigation
  Low: offscreen, position-only updates
  Dormant: far away, no updates until proximity trigger
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define HORDE_MAX_AGENTS     512
#define HORDE_MAX_GROUPS      32

typedef enum {
	HORDE_LOD_FULL,
	HORDE_LOD_MEDIUM,
	HORDE_LOD_LOW,
	HORDE_LOD_DORMANT
} hordeLOD_t;

typedef enum {
	HORDE_STATE_IDLE,
	HORDE_STATE_WANDER,
	HORDE_STATE_CHASE,
	HORDE_STATE_ATTACK,
	HORDE_STATE_FLEE,
	HORDE_STATE_DEAD
} hordeState_t;

typedef struct hordeAgent_s {
	qboolean    active;
	int         entityNum;
	vec3_t      position;
	vec3_t      velocity;
	vec3_t      targetPos;
	int         targetEntity;
	hordeState_t state;
	hordeLOD_t  lod;
	float       health;
	float       speed;
	float       attackRange;
	float       sightRange;
	float       lastUpdateTime;
	float       updateInterval;
	int         groupId;
	int         navAgentId;
} hordeAgent_t;

typedef struct hordeGroup_s {
	qboolean    active;
	vec3_t      center;
	vec3_t      targetCenter;
	float       radius;
	int         agentCount;
	float       cohesion;
	float       separation;
	float       alignment;
} hordeGroup_t;

typedef struct hordeConfig_s {
	float   fullLODDist;
	float   mediumLODDist;
	float   lowLODDist;
	float   fullUpdateRate;
	float   mediumUpdateRate;
	float   lowUpdateRate;
	float   flockCohesion;
	float   flockSeparation;
	float   flockAlignment;
	float   separationDist;
	int     maxFullLOD;
	int     maxMediumLOD;
} hordeConfig_t;

typedef int hordeHandle_t;

void Horde_Init(void);
void Horde_Shutdown(void);
void Horde_SetConfig(const hordeConfig_t *config);
void Horde_DefaultConfig(hordeConfig_t *config);
void Horde_Update(float dt, const vec3_t playerPos);

hordeHandle_t Horde_SpawnAgent(const vec3_t pos, float health, float speed, int groupId);
void          Horde_KillAgent(hordeHandle_t handle);
void          Horde_SetTarget(hordeHandle_t handle, const vec3_t target, int targetEntity);
void          Horde_GetAgentPos(hordeHandle_t handle, vec3_t out);
hordeState_t  Horde_GetAgentState(hordeHandle_t handle);
hordeLOD_t    Horde_GetAgentLOD(hordeHandle_t handle);

int  Horde_CreateGroup(const vec3_t center, float radius);
void Horde_SetGroupTarget(int groupId, const vec3_t target);

int  Horde_GetActiveCount(void);
int  Horde_GetCountByLOD(hordeLOD_t lod);

#ifdef __cplusplus
}
#endif
