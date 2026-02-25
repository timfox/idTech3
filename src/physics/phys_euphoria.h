/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Euphoria-inspired procedural animation controller.
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

#define EUPH_MAX_CONTROLLERS  64

typedef enum {
	EUPH_STATE_ANIMATED,
	EUPH_STATE_BALANCE,
	EUPH_STATE_STUMBLE,
	EUPH_STATE_FALLING,
	EUPH_STATE_RAGDOLL,
	EUPH_STATE_IMPACT,
	EUPH_STATE_GETUP,
	EUPH_STATE_BRACING,
	EUPH_STATE_REACHING,
	EUPH_STATE_GRABBED,
	EUPH_STATE_DEAD
} euphState_t;

typedef enum {
	EUPH_BONE_PELVIS = 0,
	EUPH_BONE_SPINE,
	EUPH_BONE_HEAD,
	EUPH_BONE_UPPER_ARM_L,
	EUPH_BONE_LOWER_ARM_L,
	EUPH_BONE_UPPER_ARM_R,
	EUPH_BONE_LOWER_ARM_R,
	EUPH_BONE_UPPER_LEG_L,
	EUPH_BONE_LOWER_LEG_L,
	EUPH_BONE_UPPER_LEG_R,
	EUPH_BONE_LOWER_LEG_R,
	EUPH_BONE_COUNT
} euphBone_t;

typedef struct euphConfig_s {
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
} euphConfig_t;

typedef struct euphIKTarget_s {
	qboolean active;
	vec3_t   target;
	float    weight;
	float    speed;
	int      boneIndex;
} euphIKTarget_t;

#define EUPH_MAX_IK_TARGETS 8

typedef int euphHandle_t;

typedef struct euphState_s {
	euphState_t     state;
	float           balance;
	float           painLevel;
	float           consciousness;
	vec3_t          centerOfMass;
	vec3_t          velocity;
	float           stateTime;
	float           muscleStiffness;
	qboolean        onGround;
	qboolean        canRecover;
} euphStatus_t;

euphHandle_t Euph_Create(physRagdollHandle_t ragdoll, const euphConfig_t *config);
void         Euph_Destroy(euphHandle_t handle);
void         Euph_Update(euphHandle_t handle, float dt);
void         Euph_GetStatus(euphHandle_t handle, euphStatus_t *status);

void Euph_ApplyImpact(euphHandle_t handle, const vec3_t point, const vec3_t force, float radius);
void Euph_SetLookAt(euphHandle_t handle, const vec3_t target);
void Euph_SetIKTarget(euphHandle_t handle, int slot, int boneIndex, const vec3_t target, float weight, float speed);
void Euph_ClearIKTarget(euphHandle_t handle, int slot);
void Euph_SetAnimationBlend(euphHandle_t handle, float blend);
void Euph_ForceState(euphHandle_t handle, euphState_t state);
void Euph_SetPainLevel(euphHandle_t handle, float pain);
void Euph_Kill(euphHandle_t handle);

void Euph_DefaultConfig(euphConfig_t *config);

#ifdef __cplusplus
}
#endif
