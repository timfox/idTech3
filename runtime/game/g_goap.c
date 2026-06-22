/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

GOAP implementation.
A* search over action space to find cheapest action sequence
that transitions world state to goal state.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_goap.h"
#include <string.h>

static goapAction_t actions[GOAP_MAX_ACTIONS];
static int numActions = 0;
static goapGoal_t goals[GOAP_MAX_GOALS];
static int numGoals = 0;
static goapAgent_t agents[GOAP_MAX_AGENTS];
static int numAgents = 0;

static int goap_max_plan_iterations = 4096;
static int goap_last_plan_iterations;
static cvar_t *ai_goapMaxIterations;
static cvar_t *ai_goapDebug;

#define VALID_AGENT(h) ((h) >= 0 && (h) < numAgents && agents[(h)].active)

void GOAP_SetMaxPlanIterations( int maxIterations ) {
	goap_max_plan_iterations = ( maxIterations > 64 ) ? maxIterations : 64;
}

int GOAP_GetMaxPlanIterations( void ) {
	return goap_max_plan_iterations;
}

int GOAP_GetLastPlanIterations( void ) {
	return goap_last_plan_iterations;
}

void GOAP_ForceReplan( goapAgentHandle_t h ) {
	if ( !VALID_AGENT( h ) ) {
		return;
	}
	agents[h].currentPlan.valid = qfalse;
	agents[h].replanTimer = agents[h].replanInterval;
	agents[h].worldStateDirty = qtrue;
}

void GOAP_Init(void) {
	Com_Memset(actions, 0, sizeof(actions));
	Com_Memset(goals, 0, sizeof(goals));
	Com_Memset(agents, 0, sizeof(agents));
	numActions = numGoals = numAgents = 0;
	ai_goapMaxIterations = Cvar_Get( "ai_goapMaxIterations", "4096", CVAR_ARCHIVE );
	Cvar_SetDescription( ai_goapMaxIterations,
		"Max A* expansions per GOAP plan (higher = deeper search, more CPU)." );
	ai_goapDebug = Cvar_Get( "ai_goapDebug", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( ai_goapDebug,
		"Log GOAP planning failures and iteration counts when non-zero." );
	Com_Printf( "GOAP system initialized (planner: A* + closed set, ai_goapMaxIterations=%d)\n",
		goap_max_plan_iterations );
}

void GOAP_Shutdown(void) { numActions = numGoals = numAgents = 0; }

int GOAP_RegisterAction(const char *name, float cost) {
	if (numActions >= GOAP_MAX_ACTIONS) return -1;
	int idx = numActions++;
	Com_Memset(&actions[idx], 0, sizeof(goapAction_t));
	Q_strncpyz(actions[idx].name, name, sizeof(actions[idx].name));
	actions[idx].cost = cost > 0 ? cost : 1.0f;
	actions[idx].id = idx;
	actions[idx].active = qtrue;
	return idx;
}

void GOAP_SetActionPrecondition(int id, int prop, int value) {
	if (id < 0 || id >= numActions || prop < 0 || prop >= GOAP_MAX_STATE_PROPS) return;
	actions[id].preconditions.values[prop] = value;
	actions[id].preconditions.mask[prop] = 1;
	if (prop >= actions[id].preconditions.numProps) actions[id].preconditions.numProps = prop + 1;
}

void GOAP_SetActionEffect(int id, int prop, int value) {
	if (id < 0 || id >= numActions || prop < 0 || prop >= GOAP_MAX_STATE_PROPS) return;
	actions[id].effects.values[prop] = value;
	actions[id].effects.mask[prop] = 1;
	if (prop >= actions[id].effects.numProps) actions[id].effects.numProps = prop + 1;
}

void GOAP_SetActionProceduralCheck(int id, qboolean (*check)(int, const goapState_t *)) {
	if (id >= 0 && id < numActions) actions[id].checkProceduralPrecondition = check;
}

void GOAP_SetActionDynamicCost(int id, float (*getCost)(int, const goapState_t *)) {
	if (id >= 0 && id < numActions) actions[id].getDynamicCost = getCost;
}

void GOAP_SetActionActive(int id, qboolean active) {
	if (id >= 0 && id < numActions) actions[id].active = active;
}

int GOAP_RegisterGoal(const char *name, float priority) {
	if (numGoals >= GOAP_MAX_GOALS) return -1;
	int idx = numGoals++;
	Com_Memset(&goals[idx], 0, sizeof(goapGoal_t));
	Q_strncpyz(goals[idx].name, name, sizeof(goals[idx].name));
	goals[idx].priority = priority;
	goals[idx].id = idx;
	goals[idx].active = qtrue;
	return idx;
}

void GOAP_SetGoalState(int id, int prop, int value) {
	if (id < 0 || id >= numGoals || prop < 0 || prop >= GOAP_MAX_STATE_PROPS) return;
	goals[id].desiredState.values[prop] = value;
	goals[id].desiredState.mask[prop] = 1;
	if (prop >= goals[id].desiredState.numProps) goals[id].desiredState.numProps = prop + 1;
}

void GOAP_SetGoalPriorityFunc(int id, float (*getPriority)(int, const goapState_t *)) {
	if (id >= 0 && id < numGoals) goals[id].getPriority = getPriority;
}

void GOAP_SetGoalActive(int id, qboolean active) {
	if (id >= 0 && id < numGoals) goals[id].active = active;
}

goapAgentHandle_t GOAP_CreateAgent(void) {
	if (numAgents >= GOAP_MAX_AGENTS) return -1;
	int idx = numAgents++;
	Com_Memset(&agents[idx], 0, sizeof(goapAgent_t));
	agents[idx].id = idx;
	agents[idx].active = qtrue;
	agents[idx].replanInterval = 1.0f;
	agents[idx].currentGoalId = -1;
	return idx;
}

void GOAP_DestroyAgent(goapAgentHandle_t h) {
	if (VALID_AGENT(h)) agents[h].active = qfalse;
}

void GOAP_SetAgentWorldState(goapAgentHandle_t h, int prop, int value) {
	if (!VALID_AGENT(h) || prop < 0 || prop >= GOAP_MAX_STATE_PROPS) return;
	if ( agents[h].worldState.values[prop] != value ) {
		agents[h].worldStateDirty = qtrue;
	}
	agents[h].worldState.values[prop] = value;
	agents[h].worldState.mask[prop] = 1;
	if (prop >= agents[h].worldState.numProps) agents[h].worldState.numProps = prop + 1;
}

int GOAP_GetAgentWorldState(goapAgentHandle_t h, int prop) {
	if (!VALID_AGENT(h) || prop < 0 || prop >= GOAP_MAX_STATE_PROPS) return 0;
	return agents[h].worldState.values[prop];
}

void GOAP_AddAgentAction(goapAgentHandle_t h, int actionId) {
	if (!VALID_AGENT(h) || agents[h].numAvailableActions >= GOAP_MAX_ACTIONS) return;
	agents[h].availableActions[agents[h].numAvailableActions++] = actionId;
}

void GOAP_SetAgentReplanInterval(goapAgentHandle_t h, float interval) {
	if (VALID_AGENT(h)) agents[h].replanInterval = interval > 0 ? interval : 0.5f;
}

static qboolean GOAP_StateSatisfies(const goapState_t *state, const goapState_t *goal) {
	int i;
	for (i = 0; i < GOAP_MAX_STATE_PROPS; i++) {
		if (goal->mask[i] && state->values[i] != goal->values[i]) return qfalse;
	}
	return qtrue;
}

static void GOAP_ApplyEffects(const goapState_t *state, const goapState_t *effects, goapState_t *out) {
	int i;
	Com_Memcpy(out, state, sizeof(goapState_t));
	for (i = 0; i < GOAP_MAX_STATE_PROPS; i++) {
		if (effects->mask[i]) {
			out->values[i] = effects->values[i];
			out->mask[i] = 1;
		}
	}
}

static int GOAP_Heuristic(const goapState_t *state, const goapState_t *goal) {
	int i, h = 0;
	for (i = 0; i < GOAP_MAX_STATE_PROPS; i++) {
		if (goal->mask[i] && state->values[i] != goal->values[i]) h++;
	}
	return h;
}

static qboolean GOAP_MeetsPreconditions(const goapAction_t *action, const goapState_t *state, int agentId) {
	if (!action->active) return qfalse;
	if (!GOAP_StateSatisfies(state, &action->preconditions)) return qfalse;
	if (action->checkProceduralPrecondition && !action->checkProceduralPrecondition(agentId, state)) return qfalse;
	return qtrue;
}

typedef struct {
	goapState_t state;
	float       g, f;
	int         actionPath[GOAP_MAX_PLAN_STEPS];
	int         pathLen;
} goapNode_t;

typedef struct {
	goapState_t state;
	float       g;
} goapClosedEntry_t;

#define GOAP_MAX_OPEN 512
#define GOAP_MAX_CLOSED 2048

static qboolean GOAP_StateEqual( const goapState_t *a, const goapState_t *b ) {
	return memcmp( a->values, b->values, sizeof( a->values ) ) == 0
		&& memcmp( a->mask, b->mask, sizeof( a->mask ) ) == 0;
}

static int GOAP_OpenFindState( goapNode_t *open, int openCount, const goapState_t *s ) {
	int i;
	for ( i = 0; i < openCount; i++ ) {
		if ( GOAP_StateEqual( &open[i].state, s ) ) {
			return i;
		}
	}
	return -1;
}

static int GOAP_ClosedFind( goapClosedEntry_t *closed, int n, const goapState_t *s ) {
	int i;
	for ( i = 0; i < n; i++ ) {
		if ( GOAP_StateEqual( &closed[i].state, s ) ) {
			return i;
		}
	}
	return -1;
}

qboolean GOAP_Plan(goapAgentHandle_t h, int goalId) {
	goapAgent_t *agent;
	goapGoal_t *goal;
	goapNode_t open[GOAP_MAX_OPEN];
	goapClosedEntry_t closed[GOAP_MAX_CLOSED];
	int openCount = 0;
	int closedCount = 0;
	int i, best, ai;
	int iteration;

	if (!VALID_AGENT(h) || goalId < 0 || goalId >= numGoals) return qfalse;
	agent = &agents[h];
	goal = &goals[goalId];

	if ( ai_goapMaxIterations && ai_goapMaxIterations->integer >= 64 ) {
		goap_max_plan_iterations = ai_goapMaxIterations->integer;
	}

	agent->currentPlan.valid = qfalse;
	agent->currentGoalId = goalId;
	goap_last_plan_iterations = 0;

	Com_Memcpy(&open[0].state, &agent->worldState, sizeof(goapState_t));
	open[0].g = 0;
	open[0].f = (float)GOAP_Heuristic(&agent->worldState, &goal->desiredState);
	open[0].pathLen = 0;
	openCount = 1;

	for ( iteration = 0; iteration < goap_max_plan_iterations && openCount > 0; iteration++ ) {
		goapNode_t current;

		best = 0;
		for (i = 1; i < openCount; i++) {
			if (open[i].f < open[best].f || (open[i].f == open[best].f && open[i].g > open[best].g)) {
				best = i;
			}
		}

		current = open[best];
		open[best] = open[--openCount];
		goap_last_plan_iterations = iteration + 1;

		{
			int ci = GOAP_ClosedFind( closed, closedCount, &current.state );
			if ( ci >= 0 && closed[ci].g <= current.g ) {
				continue;
			}
			if ( ci >= 0 ) {
				closed[ci].g = current.g;
			} else if ( closedCount < GOAP_MAX_CLOSED ) {
				Com_Memcpy( &closed[closedCount].state, &current.state, sizeof( goapState_t ) );
				closed[closedCount].g = current.g;
				closedCount++;
			}
			/* If closed table is full, still expand (bounded by max iterations). */
		}

		if (GOAP_StateSatisfies(&current.state, &goal->desiredState)) {
			agent->currentPlan.numSteps = current.pathLen;
			agent->currentPlan.currentStep = 0;
			agent->currentPlan.totalCost = current.g;
			agent->currentPlan.goalId = goalId;
			agent->currentPlan.valid = qtrue;
			agent->worldStateDirty = qfalse;
			for (i = 0; i < current.pathLen; i++) {
				agent->currentPlan.steps[i].actionId = current.actionPath[i];
				agent->currentPlan.steps[i].cost = actions[current.actionPath[i]].cost;
			}
			return qtrue;
		}

		for (ai = 0; ai < agent->numAvailableActions; ai++) {
			int actionId = agent->availableActions[ai];
			goapAction_t *action = &actions[actionId];
			goapNode_t succ;

			if (!GOAP_MeetsPreconditions(action, &current.state, h)) continue;
			if (current.pathLen >= GOAP_MAX_PLAN_STEPS) continue;

			float actionCost = action->cost;
			if (action->getDynamicCost) actionCost = action->getDynamicCost(h, &current.state);

			GOAP_ApplyEffects(&current.state, &action->effects, &succ.state);
			succ.g = current.g + actionCost;
			succ.f = succ.g + (float)GOAP_Heuristic(&succ.state, &goal->desiredState);
			succ.pathLen = current.pathLen + 1;
			Com_Memcpy(succ.actionPath, current.actionPath, current.pathLen * sizeof(int));
			succ.actionPath[current.pathLen] = actionId;

			{
				int cj = GOAP_ClosedFind( closed, closedCount, &succ.state );
				if ( cj >= 0 && closed[cj].g <= succ.g ) {
					continue;
				}
			}

			{
				int oi = GOAP_OpenFindState( open, openCount, &succ.state );
				if ( oi >= 0 ) {
					if ( succ.g >= open[oi].g - 0.0001f ) {
						continue;
					}
					open[oi] = succ;
				} else {
					if ( openCount >= GOAP_MAX_OPEN ) {
						continue;
					}
					open[openCount++] = succ;
				}
			}
		}
	}

	if ( ai_goapDebug && ai_goapDebug->integer ) {
		Com_Printf( S_COLOR_YELLOW "GOAP_Plan: no solution (agent %d goal %s, expansions %i)\n",
			h, GOAP_GetGoalName( goalId ), goap_last_plan_iterations );
	}

	return qfalse;
}

qboolean GOAP_AutoPlan(goapAgentHandle_t h) {
	int i, bestGoal = -1;
	float bestPriority = -1;

	if (!VALID_AGENT(h)) return qfalse;

	for (i = 0; i < numGoals; i++) {
		if (!goals[i].active) continue;
		float priority = goals[i].priority;
		if (goals[i].getPriority)
			priority = goals[i].getPriority(h, &agents[h].worldState);
		if (priority > bestPriority) {
			bestPriority = priority;
			bestGoal = i;
		}
	}

	if (bestGoal < 0) return qfalse;
	return GOAP_Plan(h, bestGoal);
}

const goapPlan_t *GOAP_GetPlan(goapAgentHandle_t h) {
	return VALID_AGENT(h) ? &agents[h].currentPlan : NULL;
}

int GOAP_GetCurrentAction(goapAgentHandle_t h) {
	if (!VALID_AGENT(h) || !agents[h].currentPlan.valid) return -1;
	goapPlan_t *p = &agents[h].currentPlan;
	if (p->currentStep >= p->numSteps) return -1;
	return p->steps[p->currentStep].actionId;
}

void GOAP_AdvancePlan(goapAgentHandle_t h) {
	if (!VALID_AGENT(h)) return;
	agents[h].currentPlan.currentStep++;
}

void GOAP_AbortPlan(goapAgentHandle_t h) {
	if (!VALID_AGENT(h)) return;
	agents[h].currentPlan.valid = qfalse;
}

qboolean GOAP_IsPlanComplete(goapAgentHandle_t h) {
	if (!VALID_AGENT(h) || !agents[h].currentPlan.valid) return qtrue;
	return agents[h].currentPlan.currentStep >= agents[h].currentPlan.numSteps;
}

void GOAP_Update(float dt) {
	int i;
	for (i = 0; i < numAgents; i++) {
		goapAgent_t *a = &agents[i];
		if (!a->active) continue;

		/* External world-state edits: invalidate and replan immediately (FEAR-style reactivity). */
		if ( a->worldStateDirty ) {
			a->currentPlan.valid = qfalse;
			a->worldStateDirty = qfalse;
			a->actionStatus = GOAP_ACTION_IDLE;
			a->actionTimer = 0;
			a->replanTimer = 0;
			GOAP_AutoPlan( i );
		}

		/* Execute current action */
		if (a->currentPlan.valid && !GOAP_IsPlanComplete(i)) {
			int actionId = a->currentPlan.steps[a->currentPlan.currentStep].actionId;
			goapAction_t *act = &actions[actionId];

			if (a->actionStatus == GOAP_ACTION_IDLE) {
				a->actionStatus = GOAP_ACTION_RUNNING;
				a->actionTimer = 0;
				if (act->onEnter) act->onEnter(i);
			}

			if (a->actionStatus == GOAP_ACTION_RUNNING) {
				a->actionTimer += dt;
				goapActionStatus_t result = GOAP_ACTION_RUNNING;

				if (act->onUpdate) {
					result = act->onUpdate(i, dt);
				} else if (act->duration > 0 && a->actionTimer >= act->duration) {
					result = GOAP_ACTION_SUCCESS;
				}

				if (result == GOAP_ACTION_SUCCESS) {
					if (act->onExit) act->onExit(i, GOAP_ACTION_SUCCESS);
					GOAP_ApplyEffects(&a->worldState, &act->effects, &a->worldState);
					a->currentPlan.currentStep++;
					a->actionStatus = GOAP_ACTION_IDLE;
					a->actionTimer = 0;
				} else if (result == GOAP_ACTION_FAILED) {
					if (act->onExit) act->onExit(i, GOAP_ACTION_FAILED);
					a->currentPlan.valid = qfalse;
					a->actionStatus = GOAP_ACTION_IDLE;
					a->actionTimer = 0;
				}
			}
		}

		/* Replan when needed */
		a->replanTimer += dt;
		if (a->replanTimer >= a->replanInterval) {
			a->replanTimer = 0;
			if (!a->currentPlan.valid || GOAP_IsPlanComplete(i)) {
				GOAP_AutoPlan(i);
			}
		}
	}
}

int GOAP_GetActionCount(void) { return numActions; }
int GOAP_GetGoalCount(void) { return numGoals; }
int GOAP_GetAgentCount(void) { return numAgents; }

/* ---- Named properties ---- */

static char propNames[GOAP_MAX_PROP_NAMES][32];
static int numPropNames = 0;

int GOAP_DefineProperty(const char *name) {
	int i;
	for (i = 0; i < numPropNames; i++) {
		if (Q_stricmp(propNames[i], name) == 0) return i;
	}
	if (numPropNames >= GOAP_MAX_PROP_NAMES) return -1;
	Q_strncpyz(propNames[numPropNames], name, sizeof(propNames[0]));
	return numPropNames++;
}

const char *GOAP_GetPropertyName(int idx) {
	return (idx >= 0 && idx < numPropNames) ? propNames[idx] : "unknown";
}

int GOAP_FindProperty(const char *name) {
	int i;
	for (i = 0; i < numPropNames; i++) {
		if (Q_stricmp(propNames[i], name) == 0) return i;
	}
	return -1;
}

/* ---- Blackboard ---- */

static goapBBEntry_t *BB_Find(goapAgentHandle_t h, const char *key, qboolean create) {
	int i;
	if (!VALID_AGENT(h)) return NULL;
	for (i = 0; i < GOAP_BLACKBOARD_SIZE; i++) {
		if (agents[h].blackboard[i].type && Q_stricmp(agents[h].blackboard[i].key, key) == 0)
			return &agents[h].blackboard[i];
	}
	if (!create) return NULL;
	for (i = 0; i < GOAP_BLACKBOARD_SIZE; i++) {
		if (!agents[h].blackboard[i].type) {
			Q_strncpyz(agents[h].blackboard[i].key, key, sizeof(agents[h].blackboard[i].key));
			return &agents[h].blackboard[i];
		}
	}
	return NULL;
}

void GOAP_BBSetFloat(goapAgentHandle_t h, const char *key, float v) {
	goapBBEntry_t *e = BB_Find(h, key, qtrue);
	if (e) { e->val.f = v; e->type = 1; }
}
float GOAP_BBGetFloat(goapAgentHandle_t h, const char *key) {
	goapBBEntry_t *e = BB_Find(h, key, qfalse);
	return e ? e->val.f : 0.0f;
}
void GOAP_BBSetInt(goapAgentHandle_t h, const char *key, int v) {
	goapBBEntry_t *e = BB_Find(h, key, qtrue);
	if (e) { e->val.i = v; e->type = 2; }
}
int GOAP_BBGetInt(goapAgentHandle_t h, const char *key) {
	goapBBEntry_t *e = BB_Find(h, key, qfalse);
	return e ? e->val.i : 0;
}
void GOAP_BBSetVec(goapAgentHandle_t h, const char *key, const vec3_t v) {
	goapBBEntry_t *e = BB_Find(h, key, qtrue);
	if (e) { VectorCopy(v, e->val.v); e->type = 3; }
}
void GOAP_BBGetVec(goapAgentHandle_t h, const char *key, vec3_t out) {
	goapBBEntry_t *e = BB_Find(h, key, qfalse);
	if (e && e->type == 3) VectorCopy(e->val.v, out);
	else VectorClear(out);
}
void GOAP_BBClear(goapAgentHandle_t h) {
	if (VALID_AGENT(h)) Com_Memset(agents[h].blackboard, 0, sizeof(agents[h].blackboard));
}

/* ---- Action execution setup ---- */

void GOAP_SetActionCallbacks(int id,
	void (*onEnter)(int), goapActionStatus_t (*onUpdate)(int, float),
	void (*onExit)(int, goapActionStatus_t))
{
	if (id < 0 || id >= numActions) return;
	actions[id].onEnter = onEnter;
	actions[id].onUpdate = onUpdate;
	actions[id].onExit = onExit;
}

void GOAP_SetActionDuration(int id, float d) {
	if (id >= 0 && id < numActions) actions[id].duration = d;
}
void GOAP_SetActionRange(int id, float r) {
	if (id >= 0 && id < numActions) actions[id].range = r;
}

/* ---- Default FPS action library ---- */

/* Property indices MUST match GOAP_DefineProperty order below */
enum {
	PROP_ALIVE = 0,
	PROP_HAS_WEAPON,
	PROP_HAS_AMMO,
	PROP_HAS_HEALTH,
	PROP_HAS_ARMOR,
	PROP_ENEMY_VISIBLE,
	PROP_ENEMY_DEAD,
	PROP_IN_COVER,
	PROP_AT_PATROL_POINT,
	PROP_AT_ITEM,
	PROP_ENEMY_IN_RANGE,
	PROP_LOW_HEALTH,
	PROP_UNDER_FIRE,
	PROP_HAS_GRENADE,
	PROP_GRENADE_READY,
	PROP_CAN_SEE_ITEM,
	PROP_CAN_REACH_ITEM,
	PROP_NEEDS_SWITCH,
	PROP_AT_SWITCH,
	PROP_OBJECTIVE_DONE,
	PROP_COUNT
};

void GOAP_RegisterDefaultActions(void) {
	int id;

	GOAP_DefineProperty("alive");
	GOAP_DefineProperty("has_weapon");
	GOAP_DefineProperty("has_ammo");
	GOAP_DefineProperty("has_health");
	GOAP_DefineProperty("has_armor");
	GOAP_DefineProperty("enemy_visible");
	GOAP_DefineProperty("enemy_dead");
	GOAP_DefineProperty("in_cover");
	GOAP_DefineProperty("at_patrol_point");
	GOAP_DefineProperty("at_item");
	GOAP_DefineProperty("enemy_in_range");
	GOAP_DefineProperty("low_health");
	GOAP_DefineProperty("under_fire");
	GOAP_DefineProperty("has_grenade");
	GOAP_DefineProperty("grenade_ready");
	GOAP_DefineProperty("can_see_item");
	GOAP_DefineProperty("can_reach_item");
	GOAP_DefineProperty("needs_switch");
	GOAP_DefineProperty("at_switch");
	GOAP_DefineProperty("objective_done");

	id = GOAP_RegisterAction("attack", 2.0f);
	GOAP_SetActionPrecondition(id, PROP_HAS_WEAPON, 1);
	GOAP_SetActionPrecondition(id, PROP_HAS_AMMO, 1);
	GOAP_SetActionPrecondition(id, PROP_ENEMY_VISIBLE, 1);
	GOAP_SetActionPrecondition(id, PROP_ENEMY_IN_RANGE, 1);
	GOAP_SetActionEffect(id, PROP_ENEMY_DEAD, 1);
	GOAP_SetActionDuration(id, 3.0f);

	id = GOAP_RegisterAction("chase", 3.0f);
	GOAP_SetActionPrecondition(id, PROP_ENEMY_VISIBLE, 1);
	GOAP_SetActionEffect(id, PROP_ENEMY_IN_RANGE, 1);
	GOAP_SetActionDuration(id, 5.0f);

	id = GOAP_RegisterAction("find_enemy", 4.0f);
	GOAP_SetActionEffect(id, PROP_ENEMY_VISIBLE, 1);
	GOAP_SetActionDuration(id, 8.0f);

	/* F.E.A.R.-style: break line of fire / suppress reaction */
	id = GOAP_RegisterAction("suppress_and_cover", 1.5f);
	GOAP_SetActionPrecondition(id, PROP_UNDER_FIRE, 1);
	GOAP_SetActionPrecondition(id, PROP_HAS_WEAPON, 1);
	GOAP_SetActionPrecondition(id, PROP_HAS_AMMO, 1);
	GOAP_SetActionEffect(id, PROP_IN_COVER, 1);
	GOAP_SetActionEffect(id, PROP_UNDER_FIRE, 0);
	GOAP_SetActionDuration(id, 1.2f);

	id = GOAP_RegisterAction("take_cover", 3.0f);
	GOAP_SetActionPrecondition(id, PROP_LOW_HEALTH, 1);
	GOAP_SetActionEffect(id, PROP_IN_COVER, 1);
	GOAP_SetActionDuration(id, 2.0f);

	id = GOAP_RegisterAction("flank", 4.0f);
	GOAP_SetActionPrecondition(id, PROP_ENEMY_VISIBLE, 1);
	GOAP_SetActionPrecondition(id, PROP_IN_COVER, 1);
	GOAP_SetActionEffect(id, PROP_ENEMY_IN_RANGE, 1);
	GOAP_SetActionDuration(id, 6.0f);

	id = GOAP_RegisterAction("grenade", 2.5f);
	GOAP_SetActionPrecondition(id, PROP_HAS_GRENADE, 1);
	GOAP_SetActionPrecondition(id, PROP_GRENADE_READY, 1);
	GOAP_SetActionPrecondition(id, PROP_ENEMY_VISIBLE, 1);
	GOAP_SetActionEffect(id, PROP_ENEMY_DEAD, 1);
	GOAP_SetActionEffect(id, PROP_HAS_GRENADE, 0);
	GOAP_SetActionEffect(id, PROP_GRENADE_READY, 0);
	GOAP_SetActionDuration(id, 2.0f);

	id = GOAP_RegisterAction("ready_grenade", 2.0f);
	GOAP_SetActionPrecondition(id, PROP_HAS_GRENADE, 1);
	GOAP_SetActionEffect(id, PROP_GRENADE_READY, 1);
	GOAP_SetActionDuration(id, 0.8f);

	id = GOAP_RegisterAction("find_grenade", 3.5f);
	GOAP_SetActionEffect(id, PROP_HAS_GRENADE, 1);
	GOAP_SetActionEffect(id, PROP_AT_ITEM, 1);
	GOAP_SetActionDuration(id, 5.0f);

	id = GOAP_RegisterAction("heal", 2.0f);
	GOAP_SetActionPrecondition(id, PROP_HAS_HEALTH, 1);
	GOAP_SetActionEffect(id, PROP_LOW_HEALTH, 0);
	GOAP_SetActionDuration(id, 1.5f);

	id = GOAP_RegisterAction("find_health", 4.0f);
	GOAP_SetActionEffect(id, PROP_HAS_HEALTH, 1);
	GOAP_SetActionEffect(id, PROP_AT_ITEM, 1);
	GOAP_SetActionDuration(id, 6.0f);

	id = GOAP_RegisterAction("find_weapon", 4.0f);
	GOAP_SetActionEffect(id, PROP_HAS_WEAPON, 1);
	GOAP_SetActionEffect(id, PROP_HAS_AMMO, 1);
	GOAP_SetActionDuration(id, 5.0f);

	id = GOAP_RegisterAction("find_ammo", 3.0f);
	GOAP_SetActionEffect(id, PROP_HAS_AMMO, 1);
	GOAP_SetActionDuration(id, 4.0f);

	id = GOAP_RegisterAction("find_armor", 4.0f);
	GOAP_SetActionEffect(id, PROP_HAS_ARMOR, 1);
	GOAP_SetActionDuration(id, 5.0f);

	id = GOAP_RegisterAction("patrol", 1.0f);
	GOAP_SetActionEffect(id, PROP_AT_PATROL_POINT, 1);
	GOAP_SetActionDuration(id, 10.0f);

	id = GOAP_RegisterAction("flee", 5.0f);
	GOAP_SetActionPrecondition(id, PROP_LOW_HEALTH, 1);
	GOAP_SetActionEffect(id, PROP_IN_COVER, 1);
	GOAP_SetActionEffect(id, PROP_ENEMY_VISIBLE, 0);
	GOAP_SetActionDuration(id, 4.0f);

	/* Traversal / interaction (Tomb Raider–style puzzle flow, abstract world facts) */
	id = GOAP_RegisterAction("survey_pickup", 2.0f);
	GOAP_SetActionEffect(id, PROP_CAN_SEE_ITEM, 1);
	GOAP_SetActionDuration(id, 2.5f);

	id = GOAP_RegisterAction("move_to_pickup", 2.5f);
	GOAP_SetActionPrecondition(id, PROP_CAN_SEE_ITEM, 1);
	GOAP_SetActionEffect(id, PROP_CAN_REACH_ITEM, 1);
	GOAP_SetActionDuration(id, 4.0f);

	id = GOAP_RegisterAction("locate_switch", 3.0f);
	GOAP_SetActionPrecondition(id, PROP_NEEDS_SWITCH, 1);
	GOAP_SetActionEffect(id, PROP_AT_SWITCH, 1);
	GOAP_SetActionDuration(id, 5.0f);

	id = GOAP_RegisterAction("use_switch", 1.5f);
	GOAP_SetActionPrecondition(id, PROP_AT_SWITCH, 1);
	GOAP_SetActionPrecondition(id, PROP_NEEDS_SWITCH, 1);
	GOAP_SetActionEffect(id, PROP_NEEDS_SWITCH, 0);
	GOAP_SetActionEffect(id, PROP_OBJECTIVE_DONE, 1);
	GOAP_SetActionDuration(id, 1.5f);

	Com_Printf("GOAP: %d default actions registered (FPS + pressure + traversal)\n", numActions);
}

void GOAP_RegisterDefaultGoals(void) {
	int id;

	id = GOAP_RegisterGoal("kill_enemy", 10.0f);
	GOAP_SetGoalState(id, PROP_ENEMY_DEAD, 1);

	id = GOAP_RegisterGoal("survive", 8.0f);
	GOAP_SetGoalState(id, PROP_LOW_HEALTH, 0);

	id = GOAP_RegisterGoal("clear_pressure", 9.0f);
	GOAP_SetGoalState(id, PROP_UNDER_FIRE, 0);

	id = GOAP_RegisterGoal("get_armed", 6.0f);
	GOAP_SetGoalState(id, PROP_HAS_WEAPON, 1);
	GOAP_SetGoalState(id, PROP_HAS_AMMO, 1);

	id = GOAP_RegisterGoal("patrol_area", 2.0f);
	GOAP_SetGoalState(id, PROP_AT_PATROL_POINT, 1);

	id = GOAP_RegisterGoal("solve_objective", 7.0f);
	GOAP_SetGoalState(id, PROP_OBJECTIVE_DONE, 1);

	Com_Printf("GOAP: %d default goals registered\n", numGoals);
}

/* ---- Debug ---- */

const char *GOAP_GetActionName(int id) {
	return (id >= 0 && id < numActions) ? actions[id].name : "none";
}

const char *GOAP_GetGoalName(int id) {
	return (id >= 0 && id < numGoals) ? goals[id].name : "none";
}

void GOAP_DebugPrintActions(void) {
	int i;
	Com_Printf("--- GOAP Actions (%d) ---\n", numActions);
	for (i = 0; i < numActions; i++) {
		Com_Printf("  [%d] %s (cost %.1f, duration %.1f, %s)\n",
			i, actions[i].name, actions[i].cost, actions[i].duration,
			actions[i].active ? "active" : "inactive");
	}
}

void GOAP_DebugPrintGoals(void) {
	int i;
	Com_Printf("--- GOAP Goals (%d) ---\n", numGoals);
	for (i = 0; i < numGoals; i++) {
		Com_Printf("  [%d] %s (priority %.1f, %s)\n",
			i, goals[i].name, goals[i].priority,
			goals[i].active ? "active" : "inactive");
	}
}

void GOAP_DebugPrintPlan(goapAgentHandle_t h) {
	int i;
	const goapPlan_t *p;
	if (!VALID_AGENT(h)) { Com_Printf("GOAP: invalid agent %d\n", h); return; }
	p = &agents[h].currentPlan;
	if (!p->valid) { Com_Printf("GOAP agent %d: no valid plan\n", h); return; }
	Com_Printf("GOAP agent %d plan (goal: %s, cost %.1f, step %d/%d):\n",
		h, GOAP_GetGoalName(p->goalId), p->totalCost, p->currentStep, p->numSteps);
	for (i = 0; i < p->numSteps; i++) {
		Com_Printf("  %s[%d] %s (cost %.1f)\n",
			i == p->currentStep ? ">" : " ",
			i, GOAP_GetActionName(p->steps[i].actionId), p->steps[i].cost);
	}
}

void GOAP_DebugPrintAgent(goapAgentHandle_t h) {
	int i;
	if (!VALID_AGENT(h)) { Com_Printf("GOAP: invalid agent %d\n", h); return; }
	goapAgent_t *a = &agents[h];
	Com_Printf("GOAP agent %d: goal=%s, actions=%d, status=%d\n",
		h, GOAP_GetGoalName(a->currentGoalId), a->numAvailableActions, a->actionStatus);
	Com_Printf("  World state: ");
	for (i = 0; i < GOAP_MAX_STATE_PROPS; i++) {
		if (a->worldState.mask[i]) {
			Com_Printf("%s=%d ", GOAP_GetPropertyName(i), a->worldState.values[i]);
		}
	}
	Com_Printf("\n");
	GOAP_DebugPrintPlan(h);
}
