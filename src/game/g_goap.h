/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Goal-Oriented Action Planning (GOAP) system.
Provides AI decision-making through planning: agents evaluate
available actions against world state to find the cheapest
sequence of actions that achieves a goal. Used by the AI Director,
horde AI, and NPC behavior systems.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define GOAP_MAX_STATE_PROPS   32
#define GOAP_MAX_ACTIONS       64
#define GOAP_MAX_GOALS         16
#define GOAP_MAX_PLAN_STEPS    16
#define GOAP_MAX_AGENTS        64

typedef struct goapState_s {
	int     values[GOAP_MAX_STATE_PROPS];
	int     mask[GOAP_MAX_STATE_PROPS];
	int     numProps;
} goapState_t;

typedef struct goapAction_s {
	char            name[64];
	goapState_t     preconditions;
	goapState_t     effects;
	float           cost;
	int             id;
	qboolean        active;

	qboolean (*checkProceduralPrecondition)(int agentId, const goapState_t *worldState);
	float    (*getDynamicCost)(int agentId, const goapState_t *worldState);
} goapAction_t;

typedef struct goapGoal_s {
	char            name[64];
	goapState_t     desiredState;
	float           priority;
	int             id;
	qboolean        active;

	float (*getPriority)(int agentId, const goapState_t *worldState);
} goapGoal_t;

typedef struct goapPlanStep_s {
	int     actionId;
	float   cost;
} goapPlanStep_t;

typedef struct goapPlan_s {
	goapPlanStep_t  steps[GOAP_MAX_PLAN_STEPS];
	int             numSteps;
	int             currentStep;
	float           totalCost;
	int             goalId;
	qboolean        valid;
} goapPlan_t;

typedef struct goapAgent_s {
	int             id;
	qboolean        active;
	goapState_t     worldState;
	goapPlan_t      currentPlan;
	int             currentGoalId;
	int             availableActions[GOAP_MAX_ACTIONS];
	int             numAvailableActions;
	float           replanTimer;
	float           replanInterval;
} goapAgent_t;

typedef int goapAgentHandle_t;

void GOAP_Init(void);
void GOAP_Shutdown(void);
void GOAP_Update(float dt);

int  GOAP_RegisterAction(const char *name, float cost);
void GOAP_SetActionPrecondition(int actionId, int propIndex, int value);
void GOAP_SetActionEffect(int actionId, int propIndex, int value);
void GOAP_SetActionProceduralCheck(int actionId,
     qboolean (*check)(int agentId, const goapState_t *worldState));
void GOAP_SetActionDynamicCost(int actionId,
     float (*getCost)(int agentId, const goapState_t *worldState));
void GOAP_SetActionActive(int actionId, qboolean active);

int  GOAP_RegisterGoal(const char *name, float priority);
void GOAP_SetGoalState(int goalId, int propIndex, int value);
void GOAP_SetGoalPriorityFunc(int goalId,
     float (*getPriority)(int agentId, const goapState_t *worldState));
void GOAP_SetGoalActive(int goalId, qboolean active);

goapAgentHandle_t GOAP_CreateAgent(void);
void GOAP_DestroyAgent(goapAgentHandle_t handle);
void GOAP_SetAgentWorldState(goapAgentHandle_t handle, int propIndex, int value);
int  GOAP_GetAgentWorldState(goapAgentHandle_t handle, int propIndex);
void GOAP_AddAgentAction(goapAgentHandle_t handle, int actionId);
void GOAP_SetAgentReplanInterval(goapAgentHandle_t handle, float interval);

qboolean       GOAP_Plan(goapAgentHandle_t handle, int goalId);
qboolean       GOAP_AutoPlan(goapAgentHandle_t handle);
const goapPlan_t *GOAP_GetPlan(goapAgentHandle_t handle);
int            GOAP_GetCurrentAction(goapAgentHandle_t handle);
void           GOAP_AdvancePlan(goapAgentHandle_t handle);
void           GOAP_AbortPlan(goapAgentHandle_t handle);
qboolean       GOAP_IsPlanComplete(goapAgentHandle_t handle);

int  GOAP_GetActionCount(void);
int  GOAP_GetGoalCount(void);
int  GOAP_GetAgentCount(void);

#ifdef __cplusplus
}
#endif
