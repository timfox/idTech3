/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Modular behavior tree implementation.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "g_bt.h"
#include "g_horde.h"
#include "g_goap.h"

#define BT_MAX_TREES  16

typedef struct btTree_s {
	btNode_t nodes[BT_MAX_NODES];
	int      numNodes;
	int      rootNodeId;
	qboolean active;
} btTree_t;

static btTree_t  trees[BT_MAX_TREES];
static btAgent_t agents[BT_MAX_AGENTS];
static int       treeCount = 0;
static int       agentCount = 0;
static qboolean  btInitialized = qfalse;

static int FindBBKey(btAgent_t *a, const char *key) {
	int i;
	for (i = 0; i < BT_BLACKBOARD_KEYS; i++) {
		if (a->blackboard[i].type == 0) continue;
		if (Q_stricmp(a->blackboard[i].key, key) == 0)
			return i;
	}
	return -1;
}

static int AllocBBSlot(btAgent_t *a, const char *key) {
	int i;
	for (i = 0; i < BT_BLACKBOARD_KEYS; i++) {
		if (a->blackboard[i].type == 0) {
			Q_strncpyz(a->blackboard[i].key, key, sizeof(a->blackboard[i].key));
			return i;
		}
	}
	return -1;
}

static btStatus_t TickNode(btAgent_t *a, btTree_t *t, int nodeId, const btContext_t *ctx);

/*
===============
BT_Init
===============
*/
void BT_Init(void) {
	Com_Memset(trees, 0, sizeof(trees));
	Com_Memset(agents, 0, sizeof(agents));
	treeCount = agentCount = 0;
	btInitialized = qtrue;
	Com_Printf("Behavior tree system initialized (max %d agents, %d trees)\n",
		BT_MAX_AGENTS, BT_MAX_TREES);
}

/*
===============
BT_Shutdown
===============
*/
void BT_Shutdown(void) {
	btInitialized = qfalse;
	treeCount = agentCount = 0;
	Com_Printf("Behavior tree system shut down\n");
}

/*
===============
BT_CreateTree
===============
*/
btTreeHandle_t BT_CreateTree(void) {
	int i;
	for (i = 0; i < BT_MAX_TREES; i++) {
		if (!trees[i].active) {
			Com_Memset(&trees[i], 0, sizeof(trees[i]));
			trees[i].active = qtrue;
			trees[i].rootNodeId = -1;
			return i;
		}
	}
	return -1;
}

/*
===============
BT_AddNode
===============
*/
int BT_AddNode(btTreeHandle_t treeH, btNodeType_t type, int *childIds, int numChildren) {
	btTree_t *t;
	btNode_t *n;
	int i;

	if (treeH < 0 || treeH >= BT_MAX_TREES || !trees[treeH].active)
		return -1;

	t = &trees[treeH];
	if (t->numNodes >= BT_MAX_NODES)
		return -1;

	n = &t->nodes[t->numNodes];
	Com_Memset(n, 0, sizeof(*n));
	n->type = type;
	n->nodeId = t->numNodes;

	if (childIds && numChildren > 0) {
		if (numChildren > BT_MAX_CHILDREN)
			numChildren = BT_MAX_CHILDREN;
		for (i = 0; i < numChildren; i++)
			n->childIndices[i] = childIds[i];
		n->numChildren = numChildren;
	}

	return t->numNodes++;
}

/*
===============
BT_SetRoot
===============
*/
void BT_SetRoot(btTreeHandle_t treeH, int nodeId) {
	if (treeH >= 0 && treeH < BT_MAX_TREES && trees[treeH].active)
		trees[treeH].rootNodeId = nodeId;
}

/*
===============
BT_SetNodeTick
===============
*/
void BT_SetNodeTick(btTreeHandle_t treeH, int nodeId,
	btStatus_t (*tick)(int agentId, int nodeId, const btContext_t *ctx)) {
	btTree_t *t;
	if (treeH < 0 || treeH >= BT_MAX_TREES || !trees[treeH].active)
		return;
	t = &trees[treeH];
	if (nodeId >= 0 && nodeId < t->numNodes)
		t->nodes[nodeId].tick = tick;
}

/*
===============
BT_CreateAgent
===============
*/
btAgentHandle_t BT_CreateAgent(btTreeHandle_t treeH) {
	int i;
	if (treeH < 0 || treeH >= BT_MAX_TREES || !trees[treeH].active)
		return -1;

	for (i = 0; i < BT_MAX_AGENTS; i++) {
		if (!agents[i].active) {
			Com_Memset(&agents[i], 0, sizeof(agents[i]));
			agents[i].active = qtrue;
			agents[i].treeHandle = treeH;
			agents[i].rootNodeId = trees[treeH].rootNodeId;
			agents[i].runningNodeId = -1;
			return i;
		}
	}
	return -1;
}

/*
===============
BT_DestroyAgent
===============
*/
void BT_DestroyAgent(btAgentHandle_t handle) {
	if (handle >= 0 && handle < BT_MAX_AGENTS)
		agents[handle].active = qfalse;
}

/*
===============
BT_SetAgentEntity
===============
*/
void BT_SetAgentEntity(btAgentHandle_t handle, int entityNum) {
	if (handle >= 0 && handle < BT_MAX_AGENTS)
		agents[handle].entityNum = entityNum;
}

/*
===============
BT_SetAgentTarget
===============
*/
void BT_SetAgentTarget(btAgentHandle_t handle, const vec3_t target, int targetEntity) {
	if (handle >= 0 && handle < BT_MAX_AGENTS) {
		VectorCopy(target, agents[handle].ctx.targetPos);
		agents[handle].ctx.targetEntity = targetEntity;
	}
}

/*
===============
BT_SetAgentContext
===============
*/
void BT_SetAgentContext(btAgentHandle_t handle, float health, float sightRange, float attackRange) {
	if (handle >= 0 && handle < BT_MAX_AGENTS) {
		agents[handle].ctx.health = health;
		agents[handle].ctx.sightRange = sightRange;
		agents[handle].ctx.attackRange = attackRange;
	}
}

/*
===============
BT_BBSetFloat / BT_BBGetFloat
===============
*/
void BT_BBSetFloat(btAgentHandle_t h, const char *key, float value) {
	int idx;
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return;
	idx = FindBBKey(&agents[h], key);
	if (idx < 0) idx = AllocBBSlot(&agents[h], key);
	if (idx >= 0) {
		agents[h].blackboard[idx].type = 1;
		agents[h].blackboard[idx].val.f = value;
	}
}

float BT_BBGetFloat(btAgentHandle_t h, const char *key) {
	int idx;
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return 0.0f;
	idx = FindBBKey(&agents[h], key);
	if (idx < 0) return 0.0f;
	return agents[h].blackboard[idx].type == 1 ? agents[h].blackboard[idx].val.f : 0.0f;
}

void BT_BBSetInt(btAgentHandle_t h, const char *key, int value) {
	int idx;
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return;
	idx = FindBBKey(&agents[h], key);
	if (idx < 0) idx = AllocBBSlot(&agents[h], key);
	if (idx >= 0) {
		agents[h].blackboard[idx].type = 2;
		agents[h].blackboard[idx].val.i = value;
	}
}

int BT_BBGetInt(btAgentHandle_t h, const char *key) {
	int idx;
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return 0;
	idx = FindBBKey(&agents[h], key);
	if (idx < 0) return 0;
	return agents[h].blackboard[idx].type == 2 ? agents[h].blackboard[idx].val.i : 0;
}

void BT_BBSetVec(btAgentHandle_t h, const char *key, const vec3_t v) {
	int idx;
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return;
	idx = FindBBKey(&agents[h], key);
	if (idx < 0) idx = AllocBBSlot(&agents[h], key);
	if (idx >= 0) {
		agents[h].blackboard[idx].type = 3;
		VectorCopy(v, agents[h].blackboard[idx].val.v);
	}
}

void BT_BBGetVec(btAgentHandle_t h, const char *key, vec3_t out) {
	int idx;
	VectorClear(out);
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return;
	idx = FindBBKey(&agents[h], key);
	if (idx < 0 || agents[h].blackboard[idx].type != 3) return;
	VectorCopy(agents[h].blackboard[idx].val.v, out);
}

/*
===============
BT_GetAnimOutput / BT_SetAnimOutput / BT_SetMorphWeight
===============
*/
const btAnimOutput_t *BT_GetAnimOutput(btAgentHandle_t handle) {
	if (handle < 0 || handle >= BT_MAX_AGENTS || !agents[handle].active)
		return NULL;
	return &agents[handle].animOutput;
}

void BT_SetAnimOutput(btAgentHandle_t handle, int legsAnim, int torsoAnim) {
	if (handle < 0 || handle >= BT_MAX_AGENTS || !agents[handle].active) return;
	agents[handle].animOutput.legsAnim = legsAnim;
	agents[handle].animOutput.torsoAnim = torsoAnim;
	agents[handle].animOutput.valid = qtrue;
}

void BT_SetMorphWeight(btAgentHandle_t handle, const char *name, float weight) {
	btAgent_t *a;
	int i;
	if (handle < 0 || handle >= BT_MAX_AGENTS || !agents[handle].active) return;
	a = &agents[handle];
	for (i = 0; i < a->animOutput.morphCount; i++) {
		if (Q_stricmp(a->animOutput.morphNames[i], name) == 0) {
			a->animOutput.morphWeights[i] = weight;
			return;
		}
	}
	if (a->animOutput.morphCount < BT_MAX_MORPH_NAMES) {
		Q_strncpyz(a->animOutput.morphNames[a->animOutput.morphCount], name, 32);
		a->animOutput.morphWeights[a->animOutput.morphCount] = weight;
		a->animOutput.morphCount++;
	}
}

/*
===============
BT_LinkHordeAgent / BT_LinkGOAPAgent
===============
*/
void BT_LinkHordeAgent(btAgentHandle_t btHandle, int hordeHandle) {
	if (btHandle >= 0 && btHandle < BT_MAX_AGENTS)
		agents[btHandle].ctx.hordeHandle = hordeHandle;
}

void BT_LinkGOAPAgent(btAgentHandle_t btHandle, int goapHandle) {
	if (btHandle >= 0 && btHandle < BT_MAX_AGENTS)
		agents[btHandle].ctx.goapHandle = goapHandle;
}

/*
===============
TickNode - core tick logic
===============
*/
static btStatus_t TickNode(btAgent_t *a, btTree_t *t, int nodeId, const btContext_t *ctx) {
	btNode_t *n;
	int i;
	btStatus_t childStatus;

	if (nodeId < 0 || nodeId >= t->numNodes)
		return BT_STATUS_FAILURE;

	n = &t->nodes[nodeId];

	switch (n->type) {
	case BT_NODE_SELECTOR:
		for (i = 0; i < n->numChildren; i++) {
			childStatus = TickNode(a, t, n->childIndices[i], ctx);
			if (childStatus == BT_STATUS_SUCCESS)
				return BT_STATUS_SUCCESS;
			if (childStatus == BT_STATUS_RUNNING)
				return BT_STATUS_RUNNING;
		}
		return BT_STATUS_FAILURE;

	case BT_NODE_SEQUENCE:
		for (i = 0; i < n->numChildren; i++) {
			childStatus = TickNode(a, t, n->childIndices[i], ctx);
			if (childStatus == BT_STATUS_FAILURE)
				return BT_STATUS_FAILURE;
			if (childStatus == BT_STATUS_RUNNING)
				return BT_STATUS_RUNNING;
		}
		return BT_STATUS_SUCCESS;

	case BT_NODE_INVERTER:
		if (n->numChildren < 1) return BT_STATUS_FAILURE;
		childStatus = TickNode(a, t, n->childIndices[0], ctx);
		if (childStatus == BT_STATUS_SUCCESS) return BT_STATUS_FAILURE;
		if (childStatus == BT_STATUS_FAILURE) return BT_STATUS_SUCCESS;
		return BT_STATUS_RUNNING;

	case BT_NODE_ACTION:
	case BT_NODE_CONDITION:
		if (n->tick) {
			int agentId = (int)(a - agents);
			return n->tick(agentId, nodeId, ctx);
		}
		return BT_STATUS_FAILURE;

	default:
		return BT_STATUS_FAILURE;
	}
}

/*
===============
BT_Update
===============
*/
void BT_Update(float dt) {
	int i;
	btAgent_t *a;
	btTree_t *t;
	int treeH;

	if (!btInitialized) return;

	for (i = 0; i < BT_MAX_AGENTS; i++) {
		a = &agents[i];
		if (!a->active || a->rootNodeId < 0) continue;

		/* Keep the exact tree selected at creation time. Root node IDs are
		 * local to each tree and are not globally unique. */
		treeH = a->treeHandle;

		if (treeH < 0 || treeH >= BT_MAX_TREES || !trees[treeH].active) continue;

		t = &trees[treeH];
		a->ctx.dt = dt;
		/* Sync from Horde if linked */
		if (a->ctx.hordeHandle >= 0) {
			Horde_GetAgentPos(a->ctx.hordeHandle, a->ctx.agentPos);
			Horde_GetAgentPos(a->ctx.hordeHandle, a->ctx.targetPos);
		}

		(void)TickNode(a, t, a->rootNodeId, &a->ctx);
	}
}

/*
===============
BT_GetActiveCount
===============
*/
int BT_GetActiveCount(void) {
	int i, c = 0;
	for (i = 0; i < BT_MAX_AGENTS; i++)
		if (agents[i].active) c++;
	return c;
}

/*
===============
BT_DebugPrintAgent
===============
*/
void BT_DebugPrintAgent(btAgentHandle_t h) {
	if (h < 0 || h >= BT_MAX_AGENTS || !agents[h].active) return;
	Com_Printf("BT Agent %d: entity %d, root %d\n", h, agents[h].entityNum, agents[h].rootNodeId);
}
