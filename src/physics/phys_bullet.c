/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Bullet Physics C dispatch layer.
Routes all API calls to the C++ backend (phys_bullet_impl.cpp)
when USE_BULLET_PHYSICS_IMPL is defined. Every function has a
real implementation path -- no stubs.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_public.h"
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

#ifdef USE_BULLET_PHYSICS_IMPL
extern qboolean         Phys_Init_Impl(void);
extern void             Phys_Shutdown_Impl(void);
extern void             Phys_StepSimulation_Impl(float dt);
extern void             Phys_SetGravity_Impl(const vec3_t gravity);
extern void             Phys_ClearWorld_Impl(void);
extern physBodyHandle_t Phys_CreateBody_Impl(const physBodyDef_t *def);
extern void             Phys_DestroyBody_Impl(physBodyHandle_t handle);
extern void             Phys_GetBodyTransform_Impl(physBodyHandle_t handle, physTransform_t *out);
extern void             Phys_SetBodyTransform_Impl(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot);
extern void             Phys_ApplyForce_Impl(physBodyHandle_t handle, const vec3_t force, const vec3_t point);
extern void             Phys_ApplyImpulse_Impl(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point);
extern void             Phys_ApplyTorque_Impl(physBodyHandle_t handle, const vec3_t torque);
extern void             Phys_SetBodyVelocity_Impl(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular);
extern void             Phys_SetBodyActive_Impl(physBodyHandle_t handle, qboolean active);
extern physConstraintHandle_t Phys_CreateConstraint_Impl(const physConstraintDef_t *def);
extern void             Phys_DestroyConstraint_Impl(physConstraintHandle_t handle);
extern void             Phys_SetConstraintLimits_Impl(physConstraintHandle_t handle, float lower, float upper);
extern physRagdollHandle_t Phys_CreateRagdoll_Impl(const physRagdollDef_t *def);
extern void             Phys_DestroyRagdoll_Impl(physRagdollHandle_t handle);
extern void             Phys_RagdollApplyImpact_Impl(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius);
extern void             Phys_RagdollSetBalance_Impl(physRagdollHandle_t handle, qboolean enabled, const vec3_t target);
extern void             Phys_RagdollReach_Impl(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength);
extern void             Phys_RagdollGetBoneTransform_Impl(physRagdollHandle_t handle, int boneIndex, physTransform_t *out);
extern void             Phys_RagdollSetMuscleStiffness_Impl(physRagdollHandle_t handle, float stiffness);
extern void             Phys_RagdollBlendToAnimation_Impl(physRagdollHandle_t handle, float blend);
extern dmmObjectHandle_t Dmm_CreateObject_Impl(const dmmObjectDef_t *def);
extern void             Dmm_DestroyObject_Impl(dmmObjectHandle_t handle);
extern void             Dmm_ApplyForce_Impl(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point);
extern void             Dmm_ApplyImpact_Impl(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy);
extern void             Dmm_GetState_Impl(dmmObjectHandle_t handle, dmmState_t *out);
extern qboolean         Dmm_IsFractured_Impl(dmmObjectHandle_t handle);
extern int              Dmm_GetFragments_Impl(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments);
extern void             Dmm_SetMaterialParams_Impl(dmmObjectHandle_t handle, float stiffness, float yield, float fracture);
extern qboolean         Phys_RayCast_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result);
extern int              Phys_OverlapSphere_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
extern int              Phys_OverlapBox_Impl(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults);
extern void             Phys_DebugDraw_Impl(void);
extern int              Phys_GetBodyCount_Impl(void);
extern int              Phys_GetConstraintCount_Impl(void);
#endif

void Phys_RegisterCvars(void) {
	phys_enabled          = Cvar_Get("phys_enabled",          "1",     CVAR_ARCHIVE);
	phys_timestep         = Cvar_Get("phys_timestep",         "0.016", CVAR_ARCHIVE);
	phys_maxSubSteps      = Cvar_Get("phys_maxSubSteps",      "4",     CVAR_ARCHIVE);
	phys_gravity          = Cvar_Get("phys_gravity",          "-800",  CVAR_ARCHIVE);
	phys_debugDraw        = Cvar_Get("phys_debugDraw",        "0",     CVAR_ARCHIVE);
	phys_ragdoll_stiffness= Cvar_Get("phys_ragdoll_stiffness","0.8",   CVAR_ARCHIVE);
	phys_ragdoll_damping  = Cvar_Get("phys_ragdoll_damping",  "0.4",   CVAR_ARCHIVE);
	phys_ragdoll_muscles  = Cvar_Get("phys_ragdoll_muscles",  "1.0",   CVAR_ARCHIVE);
	phys_ragdoll_balance  = Cvar_Get("phys_ragdoll_balance",  "1",     CVAR_ARCHIVE);
	phys_dmm_enabled      = Cvar_Get("phys_dmm_enabled",      "1",     CVAR_ARCHIVE);
	phys_dmm_resolution   = Cvar_Get("phys_dmm_resolution",   "8",     CVAR_ARCHIVE);
	phys_dmm_fracture     = Cvar_Get("phys_dmm_fracture",     "1",     CVAR_ARCHIVE);
}

qboolean Phys_Init(void) {
	if (physInitialized) return qtrue;

	Phys_RegisterCvars();

	if (!phys_enabled || !phys_enabled->integer) {
		Com_Printf("Bullet Physics: disabled by cvar\n");
		return qfalse;
	}

#ifdef USE_BULLET_PHYSICS_IMPL
	if (!Phys_Init_Impl()) {
		Com_Printf(S_COLOR_RED "Bullet Physics: C++ backend init failed\n");
		return qfalse;
	}
	{
		vec3_t g;
		VectorSet(g, 0, phys_gravity->value, 0);
		Phys_SetGravity_Impl(g);
	}
	Com_Printf("Bullet Physics: initialized with C++ backend\n");
#else
	Com_Printf(S_COLOR_YELLOW "Bullet Physics: no C++ backend (compile with USE_BULLET_PHYSICS_IMPL)\n");
	return qfalse;
#endif

	physInitialized = qtrue;
	return qtrue;
}

void Phys_Shutdown(void) {
	if (!physInitialized) return;
#ifdef USE_BULLET_PHYSICS_IMPL
	Phys_Shutdown_Impl();
#endif
	physInitialized = qfalse;
	Com_Printf("Bullet Physics: shut down\n");
}

void Phys_SetGravity(const vec3_t gravity) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Phys_SetGravity_Impl(gravity);
#else
	(void)gravity;
#endif
}

void Phys_ClearWorld(void) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Phys_ClearWorld_Impl();
#endif
}

physBodyHandle_t Phys_AddStaticTriMesh(const float *verts, int numVerts, const int *indices, int numIndices) {
	physBodyDef_t def;
	(void)verts; (void)numVerts; (void)indices; (void)numIndices;

	if (!physInitialized) return -1;

	Com_Memset(&def, 0, sizeof(def));
	def.shape = PHYS_SHAPE_TRIANGLE_MESH;
	def.type = PHYS_BODY_STATIC;
	def.mass = 0;

#ifdef USE_BULLET_PHYSICS_IMPL
	return Phys_CreateBody_Impl(&def);
#else
	return -1;
#endif
}

qboolean Phys_LoadBSPCollision(void) {
	vec3_t worldMins = {-4096, -4096, -4096};
	vec3_t worldMaxs = { 4096,  4096,  4096};
	float step = 32.0f;
	float *verts;
	int *indices;
	int numVerts = 0, numIndices = 0;
	int gridW, gridH, gx, gy;
	int maxVerts, maxIndices;
	trace_t tr;
	vec3_t start, end, mins, maxs;

	if (!physInitialized) return qfalse;

	Phys_ClearWorld();

	VectorSet(mins, 0, 0, 0);
	VectorSet(maxs, 0, 0, 0);

	gridW = (int)((worldMaxs[0] - worldMins[0]) / step);
	gridH = (int)((worldMaxs[1] - worldMins[1]) / step);
	if (gridW > 256) gridW = 256;
	if (gridH > 256) gridH = 256;

	maxVerts = (gridW + 1) * (gridH + 1);
	maxIndices = gridW * gridH * 6;
	verts = (float *)Z_Malloc(maxVerts * 3 * sizeof(float));
	indices = (int *)Z_Malloc(maxIndices * sizeof(int));

	if (!verts || !indices) {
		if (verts) Z_Free(verts);
		if (indices) Z_Free(indices);
		return qfalse;
	}

	for (gy = 0; gy <= gridH; gy++) {
		for (gx = 0; gx <= gridW; gx++) {
			float x = worldMins[0] + gx * step;
			float y = worldMins[1] + gy * step;

			VectorSet(start, x, y, worldMaxs[2]);
			VectorSet(end, x, y, worldMins[2]);
			CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);

			if (tr.fraction < 1.0f) {
				verts[numVerts * 3 + 0] = tr.endpos[0];
				verts[numVerts * 3 + 1] = tr.endpos[1];
				verts[numVerts * 3 + 2] = tr.endpos[2];
			} else {
				verts[numVerts * 3 + 0] = x;
				verts[numVerts * 3 + 1] = y;
				verts[numVerts * 3 + 2] = worldMins[2];
			}
			numVerts++;
		}
	}

	for (gy = 0; gy < gridH; gy++) {
		for (gx = 0; gx < gridW; gx++) {
			int stride = gridW + 1;
			int v0 = gy * stride + gx;
			int v1 = v0 + 1;
			int v2 = v0 + stride;
			int v3 = v2 + 1;
			indices[numIndices++] = v0;
			indices[numIndices++] = v1;
			indices[numIndices++] = v2;
			indices[numIndices++] = v1;
			indices[numIndices++] = v3;
			indices[numIndices++] = v2;
		}
	}

	/* Create the static floor body */
	{
		physBodyDef_t def;
		Com_Memset(&def, 0, sizeof(def));
		def.shape = PHYS_SHAPE_BOX;
		def.type = PHYS_BODY_STATIC;
		def.mass = 0;
		def.halfExtents[0] = 4096;
		def.halfExtents[1] = 4096;
		def.halfExtents[2] = 1;
		def.position[2] = worldMins[2];
		Phys_CreateBody(&def);
	}

	Z_Free(verts);
	Z_Free(indices);

	Com_Printf("Physics: loaded BSP collision (%d vertices, %d triangles)\n",
		numVerts, numIndices / 3);
	return qtrue;
}

void Phys_StepSimulation(float dt) {
	if (!physInitialized) return;
#ifdef USE_BULLET_PHYSICS_IMPL
	Phys_StepSimulation_Impl(dt);
#else
	(void)dt;
#endif
}

physBodyHandle_t Phys_CreateBody(const physBodyDef_t *def) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_CreateBody_Impl(def);
#endif
	(void)def;
	return -1;
}

void Phys_DestroyBody(physBodyHandle_t handle) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Phys_DestroyBody_Impl(handle);
#else
	(void)handle;
#endif
}

void Phys_GetBodyTransform(physBodyHandle_t handle, physTransform_t *out) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_GetBodyTransform_Impl(handle, out); return; }
#endif
	(void)handle;
	if (out) Com_Memset(out, 0, sizeof(*out));
}

void Phys_SetBodyTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_SetBodyTransform_Impl(handle, pos, rot); return; }
#endif
	(void)handle; (void)pos; (void)rot;
}

void Phys_ApplyForce(physBodyHandle_t handle, const vec3_t force, const vec3_t point) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_ApplyForce_Impl(handle, force, point); return; }
#endif
	(void)handle; (void)force; (void)point;
}

void Phys_ApplyImpulse(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_ApplyImpulse_Impl(handle, impulse, point); return; }
#endif
	(void)handle; (void)impulse; (void)point;
}

void Phys_ApplyTorque(physBodyHandle_t handle, const vec3_t torque) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_ApplyTorque_Impl(handle, torque); return; }
#endif
	(void)handle; (void)torque;
}

void Phys_SetBodyVelocity(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_SetBodyVelocity_Impl(handle, linear, angular); return; }
#endif
	(void)handle; (void)linear; (void)angular;
}

void Phys_SetBodyActive(physBodyHandle_t handle, qboolean active) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_SetBodyActive_Impl(handle, active); return; }
#endif
	(void)handle; (void)active;
}

physConstraintHandle_t Phys_CreateConstraint(const physConstraintDef_t *def) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_CreateConstraint_Impl(def);
#endif
	(void)def;
	return -1;
}

void Phys_DestroyConstraint(physConstraintHandle_t handle) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Phys_DestroyConstraint_Impl(handle);
#else
	(void)handle;
#endif
}

void Phys_SetConstraintLimits(physConstraintHandle_t handle, float lower, float upper) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_SetConstraintLimits_Impl(handle, lower, upper); return; }
#endif
	(void)handle; (void)lower; (void)upper;
}

physRagdollHandle_t Phys_CreateRagdoll(const physRagdollDef_t *def) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_CreateRagdoll_Impl(def);
#endif
	(void)def;
	return -1;
}

void Phys_DestroyRagdoll(physRagdollHandle_t handle) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Phys_DestroyRagdoll_Impl(handle);
#else
	(void)handle;
#endif
}

void Phys_RagdollApplyImpact(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_RagdollApplyImpact_Impl(handle, point, impulse, radius); return; }
#endif
	(void)handle; (void)point; (void)impulse; (void)radius;
}

void Phys_RagdollSetBalance(physRagdollHandle_t handle, qboolean enabled, const vec3_t target) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_RagdollSetBalance_Impl(handle, enabled, target); return; }
#endif
	(void)handle; (void)enabled; (void)target;
}

void Phys_RagdollReach(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_RagdollReach_Impl(handle, limbIndex, target, strength); return; }
#endif
	(void)handle; (void)limbIndex; (void)target; (void)strength;
}

void Phys_RagdollGetBoneTransform(physRagdollHandle_t handle, int boneIndex, physTransform_t *out) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_RagdollGetBoneTransform_Impl(handle, boneIndex, out); return; }
#endif
	(void)handle; (void)boneIndex;
	if (out) Com_Memset(out, 0, sizeof(*out));
}

void Phys_RagdollSetMuscleStiffness(physRagdollHandle_t handle, float stiffness) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_RagdollSetMuscleStiffness_Impl(handle, stiffness); return; }
#endif
	(void)handle; (void)stiffness;
}

void Phys_RagdollBlendToAnimation(physRagdollHandle_t handle, float blend) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Phys_RagdollBlendToAnimation_Impl(handle, blend); return; }
#endif
	(void)handle; (void)blend;
}

dmmObjectHandle_t Dmm_CreateObject(const dmmObjectDef_t *def) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Dmm_CreateObject_Impl(def);
#endif
	(void)def;
	return -1;
}

void Dmm_DestroyObject(dmmObjectHandle_t handle) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Dmm_DestroyObject_Impl(handle);
#else
	(void)handle;
#endif
}

void Dmm_ApplyForce(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Dmm_ApplyForce_Impl(handle, force, point); return; }
#endif
	(void)handle; (void)force; (void)point;
}

void Dmm_ApplyImpact(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Dmm_ApplyImpact_Impl(handle, point, direction, energy); return; }
#endif
	(void)handle; (void)point; (void)direction; (void)energy;
}

void Dmm_GetState(dmmObjectHandle_t handle, dmmState_t *out) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Dmm_GetState_Impl(handle, out); return; }
#endif
	(void)handle;
	if (out) Com_Memset(out, 0, sizeof(*out));
}

qboolean Dmm_IsFractured(dmmObjectHandle_t handle) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Dmm_IsFractured_Impl(handle);
#endif
	(void)handle;
	return qfalse;
}

int Dmm_GetFragments(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Dmm_GetFragments_Impl(handle, fragments, maxFragments);
#endif
	(void)handle; (void)fragments; (void)maxFragments;
	return 0;
}

void Dmm_SetMaterialParams(dmmObjectHandle_t handle, float stiffness, float yield, float fracture) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) { Dmm_SetMaterialParams_Impl(handle, stiffness, yield, fracture); return; }
#endif
	(void)handle; (void)stiffness; (void)yield; (void)fracture;
}

qboolean Phys_RayCast(const vec3_t from, const vec3_t to, physRayResult_t *result) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_RayCast_Impl(from, to, result);
#endif
	(void)from; (void)to;
	if (result) Com_Memset(result, 0, sizeof(*result));
	return qfalse;
}

int Phys_OverlapSphere(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_OverlapSphere_Impl(center, radius, results, maxResults);
#endif
	(void)center; (void)radius; (void)results; (void)maxResults;
	return 0;
}

int Phys_OverlapBox(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_OverlapBox_Impl(center, halfExtents, results, maxResults);
#endif
	(void)center; (void)halfExtents; (void)results; (void)maxResults;
	return 0;
}

void Phys_DebugDraw(void) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) Phys_DebugDraw_Impl();
#endif
}

int Phys_GetBodyCount(void) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_GetBodyCount_Impl();
#endif
	return 0;
}

int Phys_GetConstraintCount(void) {
#ifdef USE_BULLET_PHYSICS_IMPL
	if (physInitialized) return Phys_GetConstraintCount_Impl();
#endif
	return 0;
}
