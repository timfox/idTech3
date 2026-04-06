/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Modular behavior tree system for AI and animation.
Extends GOAP, Horde, and procedural animation with hierarchical
decision-making. Trees are data-driven and composable.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define BT_MAX_AGENTS        64
#define BT_MAX_NODES         128
#define BT_MAX_CHILDREN      8
#define BT_BLACKBOARD_KEYS   32
#define BT_MAX_MORPH_NAMES   8

/* Tick result: running = continue next frame, success/failure = propagate up */
typedef enum {
	BT_STATUS_RUNNING,
	BT_STATUS_SUCCESS,
	BT_STATUS_FAILURE
} btStatus_t;

/* Node types */
typedef enum {
	BT_NODE_SELECTOR,   /* Try children until one succeeds; fail if all fail */
	BT_NODE_SEQUENCE,   /* Run children in order; fail on first failure */
	BT_NODE_INVERTER,   /* Invert child result (success<->failure) */
	BT_NODE_ACTION,     /* Leaf: execute action, return running/success/failure */
	BT_NODE_CONDITION   /* Leaf: check condition, return success or failure */
} btNodeType_t;

/* Per-agent blackboard entry */
typedef struct btBBEntry_s {
	char    key[32];
	union { float f; int i; vec3_t v; } val;
	int     type; /* 0=unused, 1=float, 2=int, 3=vec3 */
} btBBEntry_t;

/* Animation output from BT (read by game/animation systems) */
typedef struct btAnimOutput_s {
	int     legsAnim;
	int     torsoAnim;
	float   morphWeights[BT_MAX_MORPH_NAMES];
	char    morphNames[BT_MAX_MORPH_NAMES][32];
	int     morphCount;
	qboolean valid;
} btAnimOutput_t;

/* World context passed to tick (read-only for conditions) */
typedef struct btContext_s {
	vec3_t      agentPos;
	vec3_t      targetPos;
	int         targetEntity;
	float       health;
	float       sightRange;
	float       attackRange;
	float       dt;
	int         hordeHandle;
	int         goapHandle;
} btContext_t;

/* Node definition (tree structure) */
typedef struct btNode_s {
	btNodeType_t    type;
	int             childIndices[BT_MAX_CHILDREN];
	int             numChildren;
	int             nodeId;

	/* For ACTION: custom tick; for CONDITION: custom check */
	btStatus_t (*tick)(int agentId, int nodeId, const btContext_t *ctx);
} btNode_t;

/* Per-agent runtime state */
typedef struct btAgent_s {
	qboolean    active;
	int         treeHandle;
	int         entityNum;
	int         rootNodeId;
	int         runningNodeId;  /* Last running node for resume */
	btBBEntry_t blackboard[BT_BLACKBOARD_KEYS];
	btAnimOutput_t animOutput;
	btContext_t ctx;
} btAgent_t;

typedef int btAgentHandle_t;
typedef int btTreeHandle_t;

/* ---- Lifecycle ---- */
void BT_Init(void);
void BT_Shutdown(void);
void BT_Update(float dt);

/* ---- Tree creation ---- */
btTreeHandle_t BT_CreateTree(void);
int            BT_AddNode(btTreeHandle_t tree, btNodeType_t type, int *childIds, int numChildren);
void           BT_SetRoot(btTreeHandle_t tree, int nodeId);
void           BT_SetNodeTick(btTreeHandle_t tree, int nodeId,
	btStatus_t (*tick)(int agentId, int nodeId, const btContext_t *ctx));

/* ---- Agent management ---- */
btAgentHandle_t BT_CreateAgent(btTreeHandle_t tree);
void            BT_DestroyAgent(btAgentHandle_t handle);
void            BT_SetAgentEntity(btAgentHandle_t handle, int entityNum);
void            BT_SetAgentTarget(btAgentHandle_t handle, const vec3_t target, int targetEntity);
void            BT_SetAgentContext(btAgentHandle_t handle, float health, float sightRange, float attackRange);

/* ---- Blackboard ---- */
void   BT_BBSetFloat(btAgentHandle_t h, const char *key, float value);
float  BT_BBGetFloat(btAgentHandle_t h, const char *key);
void   BT_BBSetInt(btAgentHandle_t h, const char *key, int value);
int    BT_BBGetInt(btAgentHandle_t h, const char *key);
void   BT_BBSetVec(btAgentHandle_t h, const char *key, const vec3_t v);
void   BT_BBGetVec(btAgentHandle_t h, const char *key, vec3_t out);

/* ---- Animation output (for game/animation systems) ---- */
const btAnimOutput_t *BT_GetAnimOutput(btAgentHandle_t handle);
void BT_SetAnimOutput(btAgentHandle_t handle, int legsAnim, int torsoAnim);
void BT_SetMorphWeight(btAgentHandle_t handle, const char *name, float weight);

/* ---- Horde/GOAP bridge ---- */
void BT_LinkHordeAgent(btAgentHandle_t btHandle, int hordeHandle);
void BT_LinkGOAPAgent(btAgentHandle_t btHandle, int goapHandle);

/* ---- Introspection ---- */
int  BT_GetActiveCount(void);
void BT_DebugPrintAgent(btAgentHandle_t h);

#ifdef __cplusplus
}
#endif
