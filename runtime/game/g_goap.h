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

#include "q_shared.h"

#define GOAP_MAX_STATE_PROPS   32
#define GOAP_MAX_ACTIONS       64
#define GOAP_MAX_GOALS         16
#define GOAP_MAX_PLAN_STEPS    16
#define GOAP_MAX_AGENTS        64
#define GOAP_BLACKBOARD_SIZE   16

typedef struct goapState_s {
	int     values[GOAP_MAX_STATE_PROPS];
	int     mask[GOAP_MAX_STATE_PROPS];
	int     numProps;
} goapState_t;

typedef enum {
	GOAP_ACTION_IDLE,
	GOAP_ACTION_RUNNING,
	GOAP_ACTION_SUCCESS,
	GOAP_ACTION_FAILED
} goapActionStatus_t;

typedef struct goapAction_s {
	char            name[64];
	goapState_t     preconditions;
	goapState_t     effects;
	float           cost;
	int             id;
	qboolean        active;
	float           duration;
	float           range;

	qboolean (*checkProceduralPrecondition)(int agentId, const goapState_t *worldState);
	float    (*getDynamicCost)(int agentId, const goapState_t *worldState);
	void     (*onEnter)(int agentId);
	goapActionStatus_t (*onUpdate)(int agentId, float dt);
	void     (*onExit)(int agentId, goapActionStatus_t status);
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

typedef struct {
	char    key[32];
	union { float f; int i; vec3_t v; } val;
	int     type; /* 0=unused, 1=float, 2=int, 3=vec3 */
} goapBBEntry_t;

typedef struct goapAgent_s {
	int             id;
	qboolean        active;
	goapState_t     worldState;
	qboolean        worldStateDirty; /* external SetAgentWorldState: invalidate plan */
	goapPlan_t      currentPlan;
	int             currentGoalId;
	int             availableActions[GOAP_MAX_ACTIONS];
	int             numAvailableActions;
	float           replanTimer;
	float           replanInterval;
	float           actionTimer;
	goapActionStatus_t actionStatus;
	goapBBEntry_t   blackboard[GOAP_BLACKBOARD_SIZE];
} goapAgent_t;

typedef int goapAgentHandle_t;

void GOAP_Init(void);
void GOAP_Shutdown(void);
void GOAP_Update(float dt);

/* Max A* expansions per GOAP_Plan (default 4096). Higher = deeper search, more CPU. */
void GOAP_SetMaxPlanIterations( int maxIterations );
int  GOAP_GetMaxPlanIterations( void );
/* Last GOAP_Plan expansion count (for tuning / debug). */
int  GOAP_GetLastPlanIterations( void );
void GOAP_ForceReplan( goapAgentHandle_t handle );

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

/* ---- Named state properties ---- */

#define GOAP_MAX_PROP_NAMES  32

int         GOAP_DefineProperty( const char *name );
const char *GOAP_GetPropertyName( int propIndex );
int         GOAP_FindProperty( const char *name );

/* ---- Blackboard (per-agent key-value memory) ---- */

void        GOAP_BBSetFloat( goapAgentHandle_t h, const char *key, float value );
float       GOAP_BBGetFloat( goapAgentHandle_t h, const char *key );
void        GOAP_BBSetInt( goapAgentHandle_t h, const char *key, int value );
int         GOAP_BBGetInt( goapAgentHandle_t h, const char *key );
void        GOAP_BBSetVec( goapAgentHandle_t h, const char *key, const vec3_t v );
void        GOAP_BBGetVec( goapAgentHandle_t h, const char *key, vec3_t out );
void        GOAP_BBClear( goapAgentHandle_t h );

/* ---- Action execution ---- */

void GOAP_SetActionCallbacks( int actionId,
	void (*onEnter)(int agentId),
	goapActionStatus_t (*onUpdate)(int agentId, float dt),
	void (*onExit)(int agentId, goapActionStatus_t status) );
void GOAP_SetActionDuration( int actionId, float duration );
void GOAP_SetActionRange( int actionId, float range );

/* ---- Default FPS action library ---- */

void GOAP_RegisterDefaultActions( void );
void GOAP_RegisterDefaultGoals( void );

/* ---- Debug / introspection ---- */

void GOAP_DebugPrintAgent( goapAgentHandle_t h );
void GOAP_DebugPrintPlan( goapAgentHandle_t h );
void GOAP_DebugPrintActions( void );
void GOAP_DebugPrintGoals( void );
const char *GOAP_GetActionName( int actionId );
const char *GOAP_GetGoalName( int goalId );

#ifdef __cplusplus
}
#endif
