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

static goapAction_t actions[GOAP_MAX_ACTIONS];
static int numActions = 0;
static goapGoal_t goals[GOAP_MAX_GOALS];
static int numGoals = 0;
static goapAgent_t agents[GOAP_MAX_AGENTS];
static int numAgents = 0;

#define VALID_AGENT(h) ((h) >= 0 && (h) < numAgents && agents[(h)].active)

void GOAP_Init(void) {
	Com_Memset(actions, 0, sizeof(actions));
	Com_Memset(goals, 0, sizeof(goals));
	Com_Memset(agents, 0, sizeof(agents));
	numActions = numGoals = numAgents = 0;
	Com_Printf("GOAP system initialized\n");
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

#define GOAP_MAX_OPEN 256

qboolean GOAP_Plan(goapAgentHandle_t h, int goalId) {
	goapAgent_t *agent;
	goapGoal_t *goal;
	goapNode_t open[GOAP_MAX_OPEN];
	int openCount = 0;
	int i, best, ai;

	if (!VALID_AGENT(h) || goalId < 0 || goalId >= numGoals) return qfalse;
	agent = &agents[h];
	goal = &goals[goalId];

	agent->currentPlan.valid = qfalse;
	agent->currentGoalId = goalId;

	Com_Memcpy(&open[0].state, &agent->worldState, sizeof(goapState_t));
	open[0].g = 0;
	open[0].f = (float)GOAP_Heuristic(&agent->worldState, &goal->desiredState);
	open[0].pathLen = 0;
	openCount = 1;

	for (int iteration = 0; iteration < 1000 && openCount > 0; iteration++) {
		best = 0;
		for (i = 1; i < openCount; i++) {
			if (open[i].f < open[best].f) best = i;
		}

		goapNode_t current = open[best];
		open[best] = open[--openCount];

		if (GOAP_StateSatisfies(&current.state, &goal->desiredState)) {
			agent->currentPlan.numSteps = current.pathLen;
			agent->currentPlan.currentStep = 0;
			agent->currentPlan.totalCost = current.g;
			agent->currentPlan.goalId = goalId;
			agent->currentPlan.valid = qtrue;
			for (i = 0; i < current.pathLen; i++) {
				agent->currentPlan.steps[i].actionId = current.actionPath[i];
				agent->currentPlan.steps[i].cost = actions[current.actionPath[i]].cost;
			}
			return qtrue;
		}

		for (ai = 0; ai < agent->numAvailableActions; ai++) {
			int actionId = agent->availableActions[ai];
			goapAction_t *action = &actions[actionId];

			if (!GOAP_MeetsPreconditions(action, &current.state, h)) continue;
			if (current.pathLen >= GOAP_MAX_PLAN_STEPS) continue;

			float actionCost = action->cost;
			if (action->getDynamicCost) actionCost = action->getDynamicCost(h, &current.state);

			if (openCount >= GOAP_MAX_OPEN) continue;

			goapNode_t *next = &open[openCount];
			GOAP_ApplyEffects(&current.state, &action->effects, &next->state);
			next->g = current.g + actionCost;
			next->f = next->g + (float)GOAP_Heuristic(&next->state, &goal->desiredState);
			next->pathLen = current.pathLen + 1;
			Com_Memcpy(next->actionPath, current.actionPath, current.pathLen * sizeof(int));
			next->actionPath[current.pathLen] = actionId;
			openCount++;
		}
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
		if (!agents[i].active) continue;
		agents[i].replanTimer += dt;
		if (agents[i].replanTimer >= agents[i].replanInterval) {
			agents[i].replanTimer = 0;
			if (!agents[i].currentPlan.valid || GOAP_IsPlanComplete(i)) {
				GOAP_AutoPlan(i);
			}
		}
	}
}

int GOAP_GetActionCount(void) { return numActions; }
int GOAP_GetGoalCount(void) { return numGoals; }
int GOAP_GetAgentCount(void) { return numAgents; }
