/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Procedural animation controller.
Layered on top of Bullet Physics ragdolls to provide:
- Active balance with center-of-mass tracking
- Protective reactions (brace for impact, catch self)
- Stumble/stagger with recovery
- Limb IK targeting (reach, grab, brace)
- Impact response blending (physics ↔ animation)
- Muscle tone simulation (stiffness varies by state)
- Look-at / head tracking
- Getup behaviors from prone/supine
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"
#include "phys_bullet.h"

#define PROCANIM_MAX_CONTROLLERS  64

typedef enum {
	PROCANIM_STATE_ANIMATED,
	PROCANIM_STATE_BALANCE,
	PROCANIM_STATE_STUMBLE,
	PROCANIM_STATE_FALLING,
	PROCANIM_STATE_RAGDOLL,
	PROCANIM_STATE_IMPACT,
	PROCANIM_STATE_GETUP,
	PROCANIM_STATE_BRACING,
	PROCANIM_STATE_REACHING,
	PROCANIM_STATE_GRABBED,
	PROCANIM_STATE_DEAD
} procAnimState_t;

typedef enum {
	PROCANIM_BONE_PELVIS = 0,
	PROCANIM_BONE_SPINE,
	PROCANIM_BONE_HEAD,
	PROCANIM_BONE_UPPER_ARM_L,
	PROCANIM_BONE_LOWER_ARM_L,
	PROCANIM_BONE_UPPER_ARM_R,
	PROCANIM_BONE_LOWER_ARM_R,
	PROCANIM_BONE_UPPER_LEG_L,
	PROCANIM_BONE_LOWER_LEG_L,
	PROCANIM_BONE_UPPER_LEG_R,
	PROCANIM_BONE_LOWER_LEG_R,
	PROCANIM_BONE_COUNT
} procAnimBone_t;

typedef struct procAnimConfig_s {
	float   balanceStiffness;
	float   balanceDamping;
	float   balanceRecoverySpeed;
	float   stumbleThreshold;
	float   fallThreshold;
	float   impactRecoveryTime;
	float   muscleStiffnessMin;
	float   muscleStiffnessMax;
	float   braceReactionTime;
	float   braceArmExtension;
	float   headTrackSpeed;
	float   headTrackMaxAngle;
	float   getupSpeed;
	float   grabStrength;
	float   painSensitivity;
	float   massScale;
} procAnimConfig_t;

typedef struct procAnimIKTarget_s {
	qboolean active;
	vec3_t   target;
	float    weight;
	float    speed;
	int      boneIndex;
} procAnimIKTarget_t;

#define PROCANIM_MAX_IK_TARGETS 8

typedef int procAnimHandle_t;

typedef struct procAnimState_s {
	procAnimState_t     state;
	float           balance;
	float           painLevel;
	float           consciousness;
	vec3_t          centerOfMass;
	vec3_t          velocity;
	float           stateTime;
	float           muscleStiffness;
	qboolean        onGround;
	qboolean        canRecover;
} procAnimStatus_t;

procAnimHandle_t ProcAnim_Create(physRagdollHandle_t ragdoll, const procAnimConfig_t *config);
void         ProcAnim_Destroy(procAnimHandle_t handle);
void         ProcAnim_Update(procAnimHandle_t handle, float dt);
void         ProcAnim_GetStatus(procAnimHandle_t handle, procAnimStatus_t *status);

void ProcAnim_ApplyImpact(procAnimHandle_t handle, const vec3_t point, const vec3_t force, float radius);
void ProcAnim_SetLookAt(procAnimHandle_t handle, const vec3_t target);
void ProcAnim_SetIKTarget(procAnimHandle_t handle, int slot, int boneIndex, const vec3_t target, float weight, float speed);
void ProcAnim_ClearIKTarget(procAnimHandle_t handle, int slot);
void ProcAnim_SetAnimationBlend(procAnimHandle_t handle, float blend);
void ProcAnim_ForceState(procAnimHandle_t handle, procAnimState_t state);
void ProcAnim_SetPainLevel(procAnimHandle_t handle, float pain);
void ProcAnim_Kill(procAnimHandle_t handle);

void ProcAnim_DefaultConfig(procAnimConfig_t *config);

#ifdef __cplusplus
}
#endif
