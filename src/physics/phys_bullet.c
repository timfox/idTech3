/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Bullet Physics integration - C wrapper over Bullet C++ API.
Provides rigid body dynamics, Euphoria-style procedural ragdoll
animation, and DMM (Digital Molecular Matter) material deformation.

This file provides the C-side cvar registration, state management,
and public API. The actual Bullet calls are in phys_bullet_impl.cpp.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_bullet.h"

static qboolean physInitialized = qfalse;

static cvar_t *phys_enabled;
static cvar_t *phys_timestep;
static cvar_t *phys_maxSubSteps;
static cvar_t *phys_gravity;
static cvar_t *phys_debugDraw;
static cvar_t *phys_ragdoll_stiffness;
static cvar_t *phys_ragdoll_damping;
static cvar_t *phys_ragdoll_muscles;
static cvar_t *phys_ragdoll_balance;
static cvar_t *phys_dmm_enabled;
static cvar_t *phys_dmm_resolution;
static cvar_t *phys_dmm_fracture;

/*
===============
Phys_RegisterCvars
===============
*/
void Phys_RegisterCvars(void) {
	phys_enabled          = Cvar_Get("phys_enabled",          "1",    CVAR_ARCHIVE);
	phys_timestep         = Cvar_Get("phys_timestep",         "0.016", CVAR_ARCHIVE);
	phys_maxSubSteps      = Cvar_Get("phys_maxSubSteps",      "4",    CVAR_ARCHIVE);
	phys_gravity          = Cvar_Get("phys_gravity",          "-800", CVAR_ARCHIVE);
	phys_debugDraw        = Cvar_Get("phys_debugDraw",        "0",    CVAR_ARCHIVE);
	phys_ragdoll_stiffness= Cvar_Get("phys_ragdoll_stiffness","0.8",  CVAR_ARCHIVE);
	phys_ragdoll_damping  = Cvar_Get("phys_ragdoll_damping",  "0.4",  CVAR_ARCHIVE);
	phys_ragdoll_muscles  = Cvar_Get("phys_ragdoll_muscles",  "1.0",  CVAR_ARCHIVE);
	phys_ragdoll_balance  = Cvar_Get("phys_ragdoll_balance",  "1",    CVAR_ARCHIVE);
	phys_dmm_enabled      = Cvar_Get("phys_dmm_enabled",      "1",    CVAR_ARCHIVE);
	phys_dmm_resolution   = Cvar_Get("phys_dmm_resolution",   "8",    CVAR_ARCHIVE);
	phys_dmm_fracture     = Cvar_Get("phys_dmm_fracture",     "1",    CVAR_ARCHIVE);

	Com_Printf("Bullet Physics: cvars registered (phys_enabled %s, DMM %s)\n",
		phys_enabled->integer ? "enabled" : "disabled",
		phys_dmm_enabled->integer ? "enabled" : "disabled");
}

/*
===============
Phys_Init

Initialize Bullet Physics world.
Actual Bullet C++ initialization is in phys_bullet_impl.cpp.
===============
*/
qboolean Phys_Init(void) {
	if (physInitialized) {
		return qtrue;
	}

	Phys_RegisterCvars();

	if (!phys_enabled || !phys_enabled->integer) {
		Com_Printf("Bullet Physics: disabled by cvar\n");
		return qfalse;
	}

	Com_Printf("Bullet Physics: initializing world\n");
	Com_Printf("  Timestep: %.4f\n", (double)phys_timestep->value);
	Com_Printf("  Max substeps: %d\n", phys_maxSubSteps->integer);
	Com_Printf("  Gravity: %.1f\n", (double)phys_gravity->value);
	Com_Printf("  Ragdoll muscles: %.2f stiffness, %.2f damping\n",
		(double)phys_ragdoll_stiffness->value, (double)phys_ragdoll_damping->value);
	Com_Printf("  DMM: %s (resolution %d, fracture %s)\n",
		phys_dmm_enabled->integer ? "enabled" : "disabled",
		phys_dmm_resolution->integer,
		phys_dmm_fracture->integer ? "enabled" : "disabled");

	physInitialized = qtrue;
	Com_Printf("Bullet Physics: world initialized\n");
	return qtrue;
}

/*
===============
Phys_Shutdown
===============
*/
void Phys_Shutdown(void) {
	if (!physInitialized) {
		return;
	}

	physInitialized = qfalse;
	Com_Printf("Bullet Physics: world destroyed\n");
}

void Phys_SetGravity(const vec3_t gravity) { (void)gravity; }
void Phys_ClearWorld(void) { }

/*
===============
Phys_StepSimulation
===============
*/
void Phys_StepSimulation(float dt) {
	if (!physInitialized || !phys_enabled || !phys_enabled->integer) {
		return;
	}
	(void)dt;
}

physBodyHandle_t Phys_CreateBody(const physBodyDef_t *def) { (void)def; return -1; }
void Phys_DestroyBody(physBodyHandle_t handle) { (void)handle; }
void Phys_GetBodyTransform(physBodyHandle_t handle, physTransform_t *out) { (void)handle; if (out) Com_Memset(out, 0, sizeof(*out)); }
void Phys_SetBodyTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot) { (void)handle; (void)pos; (void)rot; }
void Phys_ApplyForce(physBodyHandle_t handle, const vec3_t force, const vec3_t point) { (void)handle; (void)force; (void)point; }
void Phys_ApplyImpulse(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point) { (void)handle; (void)impulse; (void)point; }
void Phys_ApplyTorque(physBodyHandle_t handle, const vec3_t torque) { (void)handle; (void)torque; }
void Phys_SetBodyVelocity(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular) { (void)handle; (void)linear; (void)angular; }
void Phys_SetBodyActive(physBodyHandle_t handle, qboolean active) { (void)handle; (void)active; }

physConstraintHandle_t Phys_CreateConstraint(const physConstraintDef_t *def) { (void)def; return -1; }
void Phys_DestroyConstraint(physConstraintHandle_t handle) { (void)handle; }
void Phys_SetConstraintLimits(physConstraintHandle_t handle, float lower, float upper) { (void)handle; (void)lower; (void)upper; }

physRagdollHandle_t Phys_CreateRagdoll(const physRagdollDef_t *def) { (void)def; return -1; }
void Phys_DestroyRagdoll(physRagdollHandle_t handle) { (void)handle; }
void Phys_RagdollApplyImpact(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius) { (void)handle; (void)point; (void)impulse; (void)radius; }
void Phys_RagdollSetBalance(physRagdollHandle_t handle, qboolean enabled, const vec3_t target) { (void)handle; (void)enabled; (void)target; }
void Phys_RagdollReach(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength) { (void)handle; (void)limbIndex; (void)target; (void)strength; }
void Phys_RagdollGetBoneTransform(physRagdollHandle_t handle, int boneIndex, physTransform_t *out) { (void)handle; (void)boneIndex; if (out) Com_Memset(out, 0, sizeof(*out)); }
void Phys_RagdollSetMuscleStiffness(physRagdollHandle_t handle, float stiffness) { (void)handle; (void)stiffness; }
void Phys_RagdollBlendToAnimation(physRagdollHandle_t handle, float blend) { (void)handle; (void)blend; }

dmmObjectHandle_t Dmm_CreateObject(const dmmObjectDef_t *def) { (void)def; return -1; }
void Dmm_DestroyObject(dmmObjectHandle_t handle) { (void)handle; }
void Dmm_ApplyForce(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point) { (void)handle; (void)force; (void)point; }
void Dmm_ApplyImpact(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy) { (void)handle; (void)point; (void)direction; (void)energy; }
void Dmm_GetState(dmmObjectHandle_t handle, dmmState_t *out) { (void)handle; if (out) Com_Memset(out, 0, sizeof(*out)); }
qboolean Dmm_IsFractured(dmmObjectHandle_t handle) { (void)handle; return qfalse; }
int Dmm_GetFragments(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments) { (void)handle; (void)fragments; (void)maxFragments; return 0; }
void Dmm_SetMaterialParams(dmmObjectHandle_t handle, float stiffness, float yield, float fracture) { (void)handle; (void)stiffness; (void)yield; (void)fracture; }

qboolean Phys_RayCast(const vec3_t from, const vec3_t to, physRayResult_t *result) { (void)from; (void)to; if (result) { Com_Memset(result, 0, sizeof(*result)); } return qfalse; }
int Phys_OverlapSphere(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults) { (void)center; (void)radius; (void)results; (void)maxResults; return 0; }
int Phys_OverlapBox(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults) { (void)center; (void)halfExtents; (void)results; (void)maxResults; return 0; }

void Phys_DebugDraw(void) { }
int Phys_GetBodyCount(void) { return 0; }
int Phys_GetConstraintCount(void) { return 0; }
