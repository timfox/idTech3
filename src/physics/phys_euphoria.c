/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Euphoria-inspired procedural animation controller implementation.
Runs a state machine per-character that drives Bullet Physics ragdoll
bones with corrective forces for balance, reactions, and recovery.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_bullet.h"
#include "phys_euphoria.h"

typedef struct euphController_s {
	qboolean            active;
	physRagdollHandle_t ragdoll;
	euphConfig_t        config;
	euphState_t         state;
	float               stateTimer;
	float               balance;
	float               painLevel;
	float               consciousness;
	float               muscleStiffness;
	vec3_t              lookAtTarget;
	qboolean            lookAtActive;
	euphIKTarget_t      ikTargets[EUPH_MAX_IK_TARGETS];
	float               animBlend;
	vec3_t              lastCOM;
	vec3_t              comVelocity;
	qboolean            onGround;
	float               groundTimer;
	float               impactAccum;
	float               recoveryTimer;
} euphController_t;

static euphController_t controllers[EUPH_MAX_CONTROLLERS];
static int controllerCount = 0;

#define VALID_EUPH(h) ((h) >= 0 && (h) < controllerCount && controllers[(h)].active)

void Euph_DefaultConfig(euphConfig_t *config) {
	config->balanceStiffness = 200.0f;
	config->balanceDamping = 40.0f;
	config->balanceRecoverySpeed = 2.0f;
	config->stumbleThreshold = 0.4f;
	config->fallThreshold = 0.15f;
	config->impactRecoveryTime = 0.8f;
	config->muscleStiffnessMin = 0.1f;
	config->muscleStiffnessMax = 1.0f;
	config->braceReactionTime = 0.15f;
	config->braceArmExtension = 0.8f;
	config->headTrackSpeed = 5.0f;
	config->headTrackMaxAngle = 80.0f;
	config->getupSpeed = 1.5f;
	config->grabStrength = 300.0f;
	config->painSensitivity = 1.0f;
	config->massScale = 1.0f;
}

static void Euph_ComputeCOM(euphController_t *ctrl) {
	physTransform_t t;
	vec3_t com;
	int count = 0;
	int i;

	VectorClear(com);
	for (i = 0; i < EUPH_BONE_COUNT; i++) {
		Phys_RagdollGetBoneTransform(ctrl->ragdoll, i, &t);
		VectorAdd(com, t.position, com);
		count++;
	}
	if (count > 0) {
		VectorScale(com, 1.0f / (float)count, com);
	}

	ctrl->comVelocity[0] = (com[0] - ctrl->lastCOM[0]);
	ctrl->comVelocity[1] = (com[1] - ctrl->lastCOM[1]);
	ctrl->comVelocity[2] = (com[2] - ctrl->lastCOM[2]);
	VectorCopy(com, ctrl->lastCOM);
}

static float Euph_ComputeBalance(euphController_t *ctrl) {
	physTransform_t pelvis, lfoot, rfoot;
	vec3_t supportCenter;
	float dx, dy, supportRadius, comOffset, balance;

	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_PELVIS, &pelvis);
	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_LOWER_LEG_L, &lfoot);
	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_LOWER_LEG_R, &rfoot);

	supportCenter[0] = (lfoot.position[0] + rfoot.position[0]) * 0.5f;
	supportCenter[1] = (lfoot.position[1] + rfoot.position[1]) * 0.5f;
	supportCenter[2] = (lfoot.position[2] + rfoot.position[2]) * 0.5f;

	dx = ctrl->lastCOM[0] - supportCenter[0];
	dy = ctrl->lastCOM[1] - supportCenter[1];
	supportRadius = Distance(lfoot.position, rfoot.position) * 0.5f + 10.0f;
	comOffset = sqrtf(dx * dx + dy * dy);

	balance = 1.0f - (comOffset / supportRadius);
	if (balance < 0.0f) balance = 0.0f;
	if (balance > 1.0f) balance = 1.0f;

	return balance;
}

static void Euph_ApplyBalanceForces(euphController_t *ctrl, float dt) {
	physTransform_t pelvis, lfoot, rfoot;
	vec3_t supportCenter, corrective, damping, zero;
	float stiffness, dampCoeff;

	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_PELVIS, &pelvis);
	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_LOWER_LEG_L, &lfoot);
	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_LOWER_LEG_R, &rfoot);

	supportCenter[0] = (lfoot.position[0] + rfoot.position[0]) * 0.5f;
	supportCenter[1] = (lfoot.position[1] + rfoot.position[1]) * 0.5f;
	supportCenter[2] = (lfoot.position[2] + rfoot.position[2]) * 0.5f;

	stiffness = ctrl->config.balanceStiffness * ctrl->muscleStiffness;
	dampCoeff = ctrl->config.balanceDamping * ctrl->muscleStiffness;

	corrective[0] = (supportCenter[0] - ctrl->lastCOM[0]) * stiffness * dt;
	corrective[1] = 0;
	corrective[2] = (supportCenter[2] - ctrl->lastCOM[2]) * stiffness * dt;

	damping[0] = -ctrl->comVelocity[0] * dampCoeff;
	damping[1] = 0;
	damping[2] = -ctrl->comVelocity[2] * dampCoeff;

	VectorAdd(corrective, damping, corrective);
	VectorSet(zero, 0, 0, 0);

	Phys_RagdollApplyImpact(ctrl->ragdoll, pelvis.position, corrective, 50.0f);
}

static void Euph_ApplyBraceReaction(euphController_t *ctrl, float dt) {
	physTransform_t pelvis;
	vec3_t braceTarget, zero;
	float extension;

	(void)dt;

	Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_PELVIS, &pelvis);

	extension = ctrl->config.braceArmExtension;

	VectorCopy(ctrl->lastCOM, braceTarget);
	braceTarget[0] += ctrl->comVelocity[0] * extension * 50.0f;
	braceTarget[2] += ctrl->comVelocity[2] * extension * 50.0f;
	braceTarget[1] -= 20.0f;

	VectorSet(zero, 0, 0, 0);
	Phys_RagdollReach(ctrl->ragdoll, EUPH_BONE_LOWER_ARM_L, braceTarget, ctrl->config.grabStrength * 0.5f);
	Phys_RagdollReach(ctrl->ragdoll, EUPH_BONE_LOWER_ARM_R, braceTarget, ctrl->config.grabStrength * 0.5f);
}

static void Euph_ApplyHeadTracking(euphController_t *ctrl, float dt) {
	vec3_t headTarget;
	float trackSpeed;

	if (!ctrl->lookAtActive) return;

	trackSpeed = ctrl->config.headTrackSpeed * dt;
	VectorCopy(ctrl->lookAtTarget, headTarget);
	Phys_RagdollReach(ctrl->ragdoll, EUPH_BONE_HEAD, headTarget, trackSpeed * 50.0f);
}

static void Euph_ApplyIKTargets(euphController_t *ctrl, float dt) {
	int i;
	for (i = 0; i < EUPH_MAX_IK_TARGETS; i++) {
		if (!ctrl->ikTargets[i].active) continue;

		float force = ctrl->ikTargets[i].weight * ctrl->ikTargets[i].speed * dt * 100.0f;
		Phys_RagdollReach(ctrl->ragdoll, ctrl->ikTargets[i].boneIndex,
			ctrl->ikTargets[i].target, force);
	}
}

static void Euph_TransitionState(euphController_t *ctrl, euphState_t newState) {
	if (ctrl->state == newState) return;

	switch (newState) {
		case EUPH_STATE_STUMBLE:
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax * 0.6f;
			break;
		case EUPH_STATE_FALLING:
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax * 0.3f;
			break;
		case EUPH_STATE_RAGDOLL:
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMin;
			Phys_RagdollSetMuscleStiffness(ctrl->ragdoll, ctrl->muscleStiffness);
			break;
		case EUPH_STATE_IMPACT:
			ctrl->recoveryTimer = ctrl->config.impactRecoveryTime;
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax * 0.4f;
			break;
		case EUPH_STATE_BRACING:
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax * 0.7f;
			break;
		case EUPH_STATE_GETUP:
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax;
			break;
		case EUPH_STATE_BALANCE:
			ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax;
			break;
		case EUPH_STATE_DEAD:
			ctrl->muscleStiffness = 0.0f;
			ctrl->consciousness = 0.0f;
			Phys_RagdollSetMuscleStiffness(ctrl->ragdoll, 0.0f);
			break;
		default:
			break;
	}

	ctrl->state = newState;
	ctrl->stateTimer = 0;
	Phys_RagdollSetMuscleStiffness(ctrl->ragdoll, ctrl->muscleStiffness);
}

euphHandle_t Euph_Create(physRagdollHandle_t ragdoll, const euphConfig_t *config) {
	int idx;
	euphController_t *ctrl;

	if (controllerCount >= EUPH_MAX_CONTROLLERS) return -1;

	idx = controllerCount++;
	ctrl = &controllers[idx];
	Com_Memset(ctrl, 0, sizeof(*ctrl));

	ctrl->active = qtrue;
	ctrl->ragdoll = ragdoll;
	if (config) {
		Com_Memcpy(&ctrl->config, config, sizeof(euphConfig_t));
	} else {
		Euph_DefaultConfig(&ctrl->config);
	}

	ctrl->state = EUPH_STATE_ANIMATED;
	ctrl->balance = 1.0f;
	ctrl->painLevel = 0.0f;
	ctrl->consciousness = 1.0f;
	ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax;
	ctrl->animBlend = 1.0f;
	ctrl->onGround = qtrue;

	Phys_RagdollSetBalance(ragdoll, qtrue, ctrl->lastCOM);

	Com_Printf("Euphoria: created controller %d for ragdoll %d\n", idx, ragdoll);
	return idx;
}

void Euph_Destroy(euphHandle_t handle) {
	if (!VALID_EUPH(handle)) return;
	controllers[handle].active = qfalse;
}

void Euph_Update(euphHandle_t handle, float dt) {
	euphController_t *ctrl;

	if (!VALID_EUPH(handle)) return;
	ctrl = &controllers[handle];

	if (ctrl->state == EUPH_STATE_DEAD) return;

	ctrl->stateTimer += dt;

	Euph_ComputeCOM(ctrl);
	ctrl->balance = Euph_ComputeBalance(ctrl);

	if (ctrl->painLevel > 0) {
		ctrl->painLevel -= dt * 0.5f;
		if (ctrl->painLevel < 0) ctrl->painLevel = 0;
	}

	if (ctrl->consciousness < 1.0f && ctrl->state != EUPH_STATE_DEAD) {
		ctrl->consciousness += dt * 0.1f;
		if (ctrl->consciousness > 1.0f) ctrl->consciousness = 1.0f;
	}

	switch (ctrl->state) {
		case EUPH_STATE_ANIMATED:
		case EUPH_STATE_BALANCE:
			Euph_ApplyBalanceForces(ctrl, dt);
			Euph_ApplyHeadTracking(ctrl, dt);
			Euph_ApplyIKTargets(ctrl, dt);

			if (ctrl->balance < ctrl->config.stumbleThreshold) {
				Euph_TransitionState(ctrl, EUPH_STATE_STUMBLE);
			}
			break;

		case EUPH_STATE_STUMBLE:
			Euph_ApplyBalanceForces(ctrl, dt);

			if (ctrl->balance > ctrl->config.stumbleThreshold + 0.1f) {
				Euph_TransitionState(ctrl, EUPH_STATE_BALANCE);
			} else if (ctrl->balance < ctrl->config.fallThreshold) {
				Euph_TransitionState(ctrl, EUPH_STATE_FALLING);
			}
			break;

		case EUPH_STATE_FALLING:
			Euph_ApplyBraceReaction(ctrl, dt);

			if (ctrl->stateTimer > ctrl->config.braceReactionTime) {
				Euph_TransitionState(ctrl, EUPH_STATE_BRACING);
			}
			break;

		case EUPH_STATE_BRACING:
			Euph_ApplyBraceReaction(ctrl, dt);

			if (ctrl->onGround && ctrl->stateTimer > 0.5f) {
				if (ctrl->consciousness > 0.3f) {
					Euph_TransitionState(ctrl, EUPH_STATE_GETUP);
				} else {
					Euph_TransitionState(ctrl, EUPH_STATE_RAGDOLL);
				}
			}
			break;

		case EUPH_STATE_RAGDOLL:
			if (ctrl->consciousness > 0.5f && ctrl->onGround) {
				Euph_TransitionState(ctrl, EUPH_STATE_GETUP);
			}
			break;

		case EUPH_STATE_IMPACT:
			ctrl->recoveryTimer -= dt;
			if (ctrl->recoveryTimer <= 0) {
				if (ctrl->balance > ctrl->config.stumbleThreshold) {
					Euph_TransitionState(ctrl, EUPH_STATE_BALANCE);
				} else {
					Euph_TransitionState(ctrl, EUPH_STATE_STUMBLE);
				}
			}
			break;

		case EUPH_STATE_GETUP: {
			physTransform_t pelvis;
			vec3_t upForce, zero;

			Phys_RagdollGetBoneTransform(ctrl->ragdoll, EUPH_BONE_PELVIS, &pelvis);

			VectorSet(upForce, 0, ctrl->config.getupSpeed * 200.0f * dt, 0);
			VectorSet(zero, 0, 0, 0);
			Phys_RagdollApplyImpact(ctrl->ragdoll, pelvis.position, upForce, 30.0f);

			ctrl->muscleStiffness += dt * ctrl->config.balanceRecoverySpeed;
			if (ctrl->muscleStiffness > ctrl->config.muscleStiffnessMax) {
				ctrl->muscleStiffness = ctrl->config.muscleStiffnessMax;
			}
			Phys_RagdollSetMuscleStiffness(ctrl->ragdoll, ctrl->muscleStiffness);

			if (ctrl->stateTimer > 2.0f / ctrl->config.getupSpeed) {
				Euph_TransitionState(ctrl, EUPH_STATE_BALANCE);
			}
			break;
		}

		default:
			break;
	}

	Phys_RagdollBlendToAnimation(ctrl->ragdoll, ctrl->animBlend);
}

void Euph_GetStatus(euphHandle_t handle, euphStatus_t *status) {
	euphController_t *ctrl;

	if (!VALID_EUPH(handle) || !status) return;
	ctrl = &controllers[handle];

	status->state = ctrl->state;
	status->balance = ctrl->balance;
	status->painLevel = ctrl->painLevel;
	status->consciousness = ctrl->consciousness;
	VectorCopy(ctrl->lastCOM, status->centerOfMass);
	VectorCopy(ctrl->comVelocity, status->velocity);
	status->stateTime = ctrl->stateTimer;
	status->muscleStiffness = ctrl->muscleStiffness;
	status->onGround = ctrl->onGround;
	status->canRecover = (ctrl->consciousness > 0.3f) ? qtrue : qfalse;
}

void Euph_ApplyImpact(euphHandle_t handle, const vec3_t point, const vec3_t force, float radius) {
	euphController_t *ctrl;
	float impactMag;

	if (!VALID_EUPH(handle)) return;
	ctrl = &controllers[handle];

	if (ctrl->state == EUPH_STATE_DEAD) return;

	Phys_RagdollApplyImpact(ctrl->ragdoll, point, force, radius);

	impactMag = VectorLength(force) * ctrl->config.painSensitivity;
	ctrl->painLevel += impactMag * 0.01f;
	if (ctrl->painLevel > 1.0f) ctrl->painLevel = 1.0f;

	ctrl->consciousness -= impactMag * 0.005f;
	if (ctrl->consciousness < 0.0f) ctrl->consciousness = 0.0f;

	ctrl->impactAccum += impactMag;

	if (ctrl->consciousness <= 0.0f) {
		Euph_TransitionState(ctrl, EUPH_STATE_RAGDOLL);
	} else if (impactMag > 500.0f) {
		Euph_TransitionState(ctrl, EUPH_STATE_IMPACT);
	} else if (impactMag > 200.0f && ctrl->state == EUPH_STATE_BALANCE) {
		Euph_TransitionState(ctrl, EUPH_STATE_STUMBLE);
	}
}

void Euph_SetLookAt(euphHandle_t handle, const vec3_t target) {
	if (!VALID_EUPH(handle)) return;
	VectorCopy(target, controllers[handle].lookAtTarget);
	controllers[handle].lookAtActive = qtrue;
}

void Euph_SetIKTarget(euphHandle_t handle, int slot, int boneIndex, const vec3_t target, float weight, float speed) {
	if (!VALID_EUPH(handle) || slot < 0 || slot >= EUPH_MAX_IK_TARGETS) return;
	controllers[handle].ikTargets[slot].active = qtrue;
	controllers[handle].ikTargets[slot].boneIndex = boneIndex;
	VectorCopy(target, controllers[handle].ikTargets[slot].target);
	controllers[handle].ikTargets[slot].weight = weight;
	controllers[handle].ikTargets[slot].speed = speed;
}

void Euph_ClearIKTarget(euphHandle_t handle, int slot) {
	if (!VALID_EUPH(handle) || slot < 0 || slot >= EUPH_MAX_IK_TARGETS) return;
	controllers[handle].ikTargets[slot].active = qfalse;
}

void Euph_SetAnimationBlend(euphHandle_t handle, float blend) {
	if (!VALID_EUPH(handle)) return;
	controllers[handle].animBlend = blend < 0 ? 0 : (blend > 1 ? 1 : blend);
}

void Euph_ForceState(euphHandle_t handle, euphState_t state) {
	if (!VALID_EUPH(handle)) return;
	Euph_TransitionState(&controllers[handle], state);
}

void Euph_SetPainLevel(euphHandle_t handle, float pain) {
	if (!VALID_EUPH(handle)) return;
	controllers[handle].painLevel = pain < 0 ? 0 : (pain > 1 ? 1 : pain);
}

void Euph_Kill(euphHandle_t handle) {
	if (!VALID_EUPH(handle)) return;
	Euph_TransitionState(&controllers[handle], EUPH_STATE_DEAD);
}
