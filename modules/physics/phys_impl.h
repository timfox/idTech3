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
void                 Phys_SetBodyTargetTransform_Impl(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot, float timeStep);
void                 Phys_SetBodyGravityScale_Impl(physBodyHandle_t handle, float scale);
void                 Phys_SetBodyMotionLocks_Impl(physBodyHandle_t handle, int lockBits);
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
void                 Phys_SetConstraintMotor_Impl(physConstraintHandle_t handle, qboolean enable,
	float speed, float maxForce);
void                 Phys_SetConstraintBreakForce_Impl(physConstraintHandle_t handle, float force, float torque);
void                 Phys_SetWheelSteering_Impl(physConstraintHandle_t handle, float angleRadians, float maxTorque);
void                 Phys_SetConstraintSpring_Impl(physConstraintHandle_t handle, qboolean enable,
	float hertz, float dampingRatio);
void                 Phys_SetSphericalLimits_Impl(physConstraintHandle_t handle, float coneAngleRadians,
	float twistLowerRadians, float twistUpperRadians);
void                 Phys_GetConstraintReaction_Impl(physConstraintHandle_t handle, vec3_t forceOut, vec3_t torqueOut);

int                  Phys_AttachShape_Impl(physBodyHandle_t body, const physBodyDef_t *shapeDef);
void                 Phys_DestroyAttachedShape_Impl(physBodyHandle_t body, int shapeIndex);
void                 Phys_SetBodyFilter_Impl(physBodyHandle_t body, int categoryBits, int maskBits);
void                 Phys_SetBodyFilterEx_Impl(physBodyHandle_t body, int categoryBits, int maskBits, int groupIndex);

physRagdollHandle_t  Phys_CreateRagdoll_Impl(const physRagdollDef_t *def);
void                 Phys_DestroyRagdoll_Impl(physRagdollHandle_t handle);
void                 Phys_RagdollApplyImpact_Impl(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius);
void                 Phys_RagdollSetBalance_Impl(physRagdollHandle_t handle, qboolean enabled, const vec3_t target);
void                 Phys_RagdollReach_Impl(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength);
void                 Phys_RagdollGetBoneTransform_Impl(physRagdollHandle_t handle, int boneIndex, physTransform_t *out);
void                 Phys_RagdollSetMuscleStiffness_Impl(physRagdollHandle_t handle, float stiffness);
void                 Phys_RagdollBlendToAnimation_Impl(physRagdollHandle_t handle, float blend);
void                 Phys_RagdollApplyBoneTorque_Impl(physRagdollHandle_t handle, int boneIndex, const vec3_t torque);
void                 Phys_RagdollSetBoneAnimTarget_Impl(physRagdollHandle_t handle, int boneIndex,
	const vec3_t position, const vec3_t rotationDeg);
void                 Phys_RagdollClearAnimTargets_Impl(physRagdollHandle_t handle);
int                  Phys_GetRagdollCount_Impl(void);

dmmObjectHandle_t    Dmm_CreateObject_Impl(const dmmObjectDef_t *def);
void                 Dmm_DestroyObject_Impl(dmmObjectHandle_t handle);
void                 Dmm_ApplyForce_Impl(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point);
void                 Dmm_ApplyImpact_Impl(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy);
void                 Dmm_GetState_Impl(dmmObjectHandle_t handle, dmmState_t *out);
qboolean             Dmm_IsFractured_Impl(dmmObjectHandle_t handle);
int                  Dmm_GetFragments_Impl(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments);
void                 Dmm_SetMaterialParams_Impl(dmmObjectHandle_t handle, float stiffness, float yield, float fracture);
int                  Dmm_SpawnFragments_Impl(dmmObjectHandle_t handle, const vec3_t impactPoint, float energy);

qboolean             Phys_RayCast_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result);
qboolean             Phys_RayCastFiltered_Impl(const vec3_t from, const vec3_t to, physRayResult_t *result,
	const physQueryFilter_t *filter);
qboolean             Phys_ConvexSweep_Impl(const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result);
qboolean             Phys_ConvexSweepFiltered_Impl(const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result, const physQueryFilter_t *filter);
int                  Phys_OverlapSphere_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
int                  Phys_OverlapBox_Impl(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults);
int                  Phys_OverlapShape_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
int                  Phys_OverlapShapeFiltered_Impl(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults,
	const physQueryFilter_t *filter);
int                  Phys_GetBodyContacts_Impl(physBodyHandle_t body, physContact_t *out, int maxOut);
void                 Phys_SetHitEventThreshold_Impl(float approachSpeed);
void                 Phys_DebugDraw_Impl(void);
void                 Phys_ProcessContactEvents_Impl(void);
void                 Phys_SetBodyMaterial_Impl(physBodyHandle_t handle, int materialId);
int                  Phys_GetBodyMaterial_Impl(physBodyHandle_t handle);
void                 Phys_SetBodyFriction_Impl(physBodyHandle_t handle, float friction);
void                 Phys_SetBodyRestitution_Impl(physBodyHandle_t handle, float restitution);
int                  Phys_GetBodyCount_Impl(void);
int                  Phys_GetConstraintCount_Impl(void);
physBodyHandle_t     Phys_AddStaticTriMesh_Impl(const float *verts, int numVerts, const int *indices, int numIndices);
physBodyHandle_t     Phys_AddStaticHeightField_Impl(const float *heights, int countX, int countY,
	float cellSize, float heightScale, const vec3_t origin);
physBodyHandle_t     Phys_AddStaticCompoundBoxes_Impl(const float *centersXYZ, const float *halfExtentsXYZ, int count);
/* Returns qtrue if the backend handled the step (updates origin/velocity/grounded). */
qboolean             Phys_MoverStep_Impl(vec3_t origin, vec3_t velocity, float radius, float height,
	const vec3_t wishDir, float wishSpeed, float dt, qboolean jump, qboolean *groundedOut);
int                  Phys_GetWorkerCount_Impl(void);
/* Returns >=0 if handled (bodies affected); -1 to use generic overlap+impulse path. */
int                  Phys_ApplyImpulseRadius_Impl(const vec3_t center, float radius, float magnitude, float falloff);
void                 Phys_GetSoftStepProfile_Impl(physSoftStepProfile_t *out);
void                 Phys_StartRecording_Impl(void);
void                 Phys_StopRecording_Impl(const char *path);
qboolean             Phys_ValidateReplay_Impl(const char *path);
void                 Phys_DumpWorld_Impl(void);

qboolean             Phys_GetClosestPoint_Impl(physBodyHandle_t body, const vec3_t target,
	vec3_t closestOut, float *distanceOut);
qboolean             Phys_SphereTimeOfImpact_Impl(const vec3_t from, const vec3_t to, float radius,
	physBodyHandle_t againstBody, physRayResult_t *result);
void                 Phys_SetCustomFilterCallback_Impl(PhysCustomFilterFn fn, void *userData);
void                 Phys_SetPreSolveCallback_Impl(PhysPreSolveFn fn, void *userData);
void                 Phys_SetBodyContinuous_Impl(physBodyHandle_t body, qboolean enable);
void                 Phys_SetBodySleepEnabled_Impl(physBodyHandle_t body, qboolean enable);
void                 Phys_SetBodySleepThreshold_Impl(physBodyHandle_t body, float linearThreshold);
void                 Phys_SetContactTuning_Impl(float hertz, float dampingRatio, float contactSpeed);
void                 Phys_SetMaxLinearSpeed_Impl(float maxSpeed);
void                 Phys_EnableSpeculative_Impl(qboolean enable);
void                 Phys_SetDebugDrawFlags_Impl(unsigned flags);
qboolean             Phys_UpdateStaticTriMesh_Impl(physBodyHandle_t body, const float *verts, int numVerts,
	const int *indices, int numIndices);
void                 Phys_RebuildStaticTree_Impl(void);
qboolean             Phys_ReplayOpen_Impl(const char *path);
void                 Phys_ReplayClose_Impl(void);
qboolean             Phys_ReplayStep_Impl(void);
void                 Phys_ReplaySeek_Impl(int frame);
int                  Phys_ReplayGetFrame_Impl(void);
int                  Phys_ReplayGetFrameCount_Impl(void);
qboolean             Phys_ReplayHasDiverged_Impl(void);
qboolean             Phys_ReplayIsOpen_Impl(void);
void                 Phys_SetHingeTargetAngle_Impl(physConstraintHandle_t handle, float targetRadians);
void                 Phys_SetSliderTarget_Impl(physConstraintHandle_t handle, float targetTranslation);
void                 Phys_SetDistanceLength_Impl(physConstraintHandle_t handle, float length);

#ifdef __cplusplus
}
#endif
