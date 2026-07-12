/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared substrate entry points implemented by phys_box3d_impl.c or
phys_bullet_impl.cpp (exactly one backend compiled in).
===========================================================================
*/

#pragma once

#include "phys_bullet.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean             Phys_Init_Impl(void);
void                 Phys_Shutdown_Impl(void);
void                 Phys_StepSimulation_Impl(float dt);
void                 Phys_SetGravity_Impl(const vec3_t gravity);
void                 Phys_ClearWorld_Impl(void);

physBodyHandle_t     Phys_CreateBody_Impl(const physBodyDef_t *def);
void                 Phys_DestroyBody_Impl(physBodyHandle_t handle);
void                 Phys_GetBodyTransform_Impl(physBodyHandle_t handle, physTransform_t *out);
void                 Phys_SetBodyTransform_Impl(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot);
void                 Phys_ApplyForce_Impl(physBodyHandle_t handle, const vec3_t force, const vec3_t point);
void                 Phys_ApplyImpulse_Impl(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point);
void                 Phys_ApplyTorque_Impl(physBodyHandle_t handle, const vec3_t torque);
void                 Phys_SetBodyVelocity_Impl(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular);
void                 Phys_SetBodyActive_Impl(physBodyHandle_t handle, qboolean active);
physBodyType_t       Phys_GetBodyType_Impl(physBodyHandle_t handle);
qboolean             Phys_IsBodyDynamic_Impl(physBodyHandle_t handle);

physConstraintHandle_t Phys_CreateConstraint_Impl(const physConstraintDef_t *def);
void                 Phys_DestroyConstraint_Impl(physConstraintHandle_t handle);
void                 Phys_SetConstraintLimits_Impl(physConstraintHandle_t handle, float lower, float upper);

physRagdollHandle_t  Phys_CreateRagdoll_Impl(const physRagdollDef_t *def);
void                 Phys_DestroyRagdoll_Impl(physRagdollHandle_t handle);
void                 Phys_RagdollApplyImpact_Impl(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius);
void                 Phys_RagdollSetBalance_Impl(physRagdollHandle_t handle, qboolean enabled, const vec3_t target);
void                 Phys_RagdollReach_Impl(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength);
void                 Phys_RagdollGetBoneTransform_Impl(physRagdollHandle_t handle, int boneIndex, physTransform_t *out);
void                 Phys_RagdollSetMuscleStiffness_Impl(physRagdollHandle_t handle, float stiffness);
void                 Phys_RagdollBlendToAnimation_Impl(physRagdollHandle_t handle, float blend);
void                 Phys_RagdollApplyBoneTorque_Impl(physRagdollHandle_t handle, int boneIndex, const vec3_t torque);
int                  Phys_GetRagdollCount_Impl(void);

dmmObjectHandle_t    Dmm_CreateObject_Impl(const dmmObjectDef_t *def);
void                 Dmm_DestroyObject_Impl(dmmObjectHandle_t handle);
void                 Dmm_ApplyForce_Impl(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point);
void                 Dmm_ApplyImpact_Impl(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy);
void                 Dmm_GetState_Impl(dmmObjectHandle_t handle, dmmState_t *out);
qboolean             Dmm_IsFractured_Impl(dmmObjectHandle_t handle);
int                  Dmm_GetFragments_Impl(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments);
void                 Dmm_SetMaterialParams_Impl(dmmObjectHandle_t handle, float stiffness, float yield, float fracture);

qboolean             Phys_RayCast_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result);
qboolean             Phys_ConvexSweep_Impl(const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result);
int                  Phys_OverlapSphere_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
int                  Phys_OverlapBox_Impl(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults);
void                 Phys_DebugDraw_Impl(void);
void                 Phys_ProcessContactEvents_Impl(void);
void                 Phys_SetBodyMaterial_Impl(physBodyHandle_t handle, int materialId);
int                  Phys_GetBodyMaterial_Impl(physBodyHandle_t handle);
int                  Phys_GetBodyCount_Impl(void);
int                  Phys_GetConstraintCount_Impl(void);
physBodyHandle_t     Phys_AddStaticTriMesh_Impl(const float *verts, int numVerts, const int *indices, int numIndices);
physBodyHandle_t     Phys_AddStaticCompoundBoxes_Impl(const float *centersXYZ, const float *halfExtentsXYZ, int count);
/* Returns qtrue if the backend handled the step (updates origin/velocity/grounded). */
qboolean             Phys_MoverStep_Impl(vec3_t origin, vec3_t velocity, float radius, float height,
	const vec3_t wishDir, float wishSpeed, float dt, qboolean jump, qboolean *groundedOut);
int                  Phys_GetWorkerCount_Impl(void);
/* Returns >=0 if handled (bodies affected); -1 to use generic overlap+impulse path. */
int                  Phys_ApplyImpulseRadius_Impl(const vec3_t center, float radius, float magnitude, float falloff);

#ifdef __cplusplus
}
#endif
