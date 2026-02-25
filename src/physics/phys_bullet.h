/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Bullet Physics integration for id Tech 3 engine.
Provides rigid body dynamics, ragdoll/procedural animation
(Procedural animation), and Digital Molecular Matter (DMM) capabilities
for real-time material deformation and destruction.

Bullet Physics is licensed under the zlib license.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define PHYS_MAX_RIGID_BODIES    4096
#define PHYS_MAX_RAGDOLLS        64
#define PHYS_MAX_CONSTRAINTS     512
#define PHYS_MAX_DMM_OBJECTS     256

typedef enum {
	PHYS_SHAPE_BOX,
	PHYS_SHAPE_SPHERE,
	PHYS_SHAPE_CAPSULE,
	PHYS_SHAPE_CYLINDER,
	PHYS_SHAPE_CONVEX_HULL,
	PHYS_SHAPE_TRIANGLE_MESH,
	PHYS_SHAPE_COMPOUND,
	PHYS_SHAPE_HEIGHTFIELD
} physShape_t;

typedef enum {
	PHYS_BODY_STATIC,
	PHYS_BODY_DYNAMIC,
	PHYS_BODY_KINEMATIC
} physBodyType_t;

typedef enum {
	PHYS_CONSTRAINT_POINT,
	PHYS_CONSTRAINT_HINGE,
	PHYS_CONSTRAINT_SLIDER,
	PHYS_CONSTRAINT_CONE_TWIST,
	PHYS_CONSTRAINT_GENERIC_6DOF,
	PHYS_CONSTRAINT_FIXED
} physConstraintType_t;

typedef enum {
	DMM_WOOD,
	DMM_GLASS,
	DMM_METAL_THIN,
	DMM_METAL_THICK,
	DMM_CONCRETE,
	DMM_STONE,
	DMM_ICE,
	DMM_PLASTIC,
	DMM_CLOTH,
	DMM_RUBBER,
	DMM_FLESH,
	DMM_CUSTOM
} dmmMaterialType_t;

typedef int physBodyHandle_t;
typedef int physConstraintHandle_t;
typedef int physRagdollHandle_t;
typedef int dmmObjectHandle_t;

typedef struct physBodyDef_s {
	physShape_t     shape;
	physBodyType_t  type;
	vec3_t          position;
	vec3_t          rotation;
	vec3_t          halfExtents;
	float           radius;
	float           height;
	float           mass;
	float           friction;
	float           restitution;
	float           linearDamping;
	float           angularDamping;
	int             collisionGroup;
	int             collisionMask;
} physBodyDef_t;

typedef struct physConstraintDef_s {
	physConstraintType_t type;
	physBodyHandle_t     bodyA;
	physBodyHandle_t     bodyB;
	vec3_t               pivotA;
	vec3_t               pivotB;
	vec3_t               axisA;
	vec3_t               axisB;
	float                lowerLimit;
	float                upperLimit;
	float                softness;
	float                biasFactor;
	float                relaxationFactor;
	qboolean             disableCollision;
} physConstraintDef_t;

typedef struct physRagdollDef_s {
	vec3_t          rootPosition;
	float           scale;
	int             entityNum;

	float           jointStiffness;
	float           jointDamping;
	float           muscleStrength;
	float           balanceForce;
	float           reachForce;
	float           impactResponse;
	float           limbMass;
	qboolean        selfCollision;
} physRagdollDef_t;

typedef struct dmmObjectDef_s {
	dmmMaterialType_t material;
	vec3_t            position;
	vec3_t            rotation;
	vec3_t            dimensions;
	float             thickness;
	float             density;
	float             stiffness;
	float             damping;
	float             yieldStrength;
	float             fractureStrength;
	float             deformability;
	int               gridResolution;
	int               entityNum;
} dmmObjectDef_t;

typedef struct physTransform_s {
	vec3_t  position;
	vec3_t  rotation;
	vec3_t  linearVelocity;
	vec3_t  angularVelocity;
} physTransform_t;

typedef struct dmmState_s {
	float   strain;
	float   stress;
	float   deformation;
	float   integrity;
	int     numFragments;
	qboolean fractured;
} dmmState_t;

/* core system */
qboolean    Phys_Init(void);
void        Phys_Shutdown(void);
void        Phys_RegisterCvars(void);
void        Phys_StepSimulation(float dt);
void        Phys_SetGravity(const vec3_t gravity);
void        Phys_ClearWorld(void);

/* rigid bodies */
physBodyHandle_t Phys_CreateBody(const physBodyDef_t *def);
void             Phys_DestroyBody(physBodyHandle_t handle);
void             Phys_GetBodyTransform(physBodyHandle_t handle, physTransform_t *out);
void             Phys_SetBodyTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot);
void             Phys_ApplyForce(physBodyHandle_t handle, const vec3_t force, const vec3_t point);
void             Phys_ApplyImpulse(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point);
void             Phys_ApplyTorque(physBodyHandle_t handle, const vec3_t torque);
void             Phys_SetBodyVelocity(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular);
void             Phys_SetBodyActive(physBodyHandle_t handle, qboolean active);

/* constraints */
physConstraintHandle_t Phys_CreateConstraint(const physConstraintDef_t *def);
void                   Phys_DestroyConstraint(physConstraintHandle_t handle);
void                   Phys_SetConstraintLimits(physConstraintHandle_t handle, float lower, float upper);

/* ragdoll / Procedural animation */
physRagdollHandle_t Phys_CreateRagdoll(const physRagdollDef_t *def);
void                Phys_DestroyRagdoll(physRagdollHandle_t handle);
void                Phys_RagdollApplyImpact(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius);
void                Phys_RagdollSetBalance(physRagdollHandle_t handle, qboolean enabled, const vec3_t target);
void                Phys_RagdollReach(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength);
void                Phys_RagdollGetBoneTransform(physRagdollHandle_t handle, int boneIndex, physTransform_t *out);
void                Phys_RagdollSetMuscleStiffness(physRagdollHandle_t handle, float stiffness);
void                Phys_RagdollBlendToAnimation(physRagdollHandle_t handle, float blend);

/* Digital Molecular Matter (DMM) */
dmmObjectHandle_t Dmm_CreateObject(const dmmObjectDef_t *def);
void              Dmm_DestroyObject(dmmObjectHandle_t handle);
void              Dmm_ApplyForce(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point);
void              Dmm_ApplyImpact(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy);
void              Dmm_GetState(dmmObjectHandle_t handle, dmmState_t *out);
qboolean          Dmm_IsFractured(dmmObjectHandle_t handle);
int               Dmm_GetFragments(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments);
void              Dmm_SetMaterialParams(dmmObjectHandle_t handle, float stiffness, float yield, float fracture);

/* ray cast / queries */
typedef struct physRayResult_s {
	qboolean        hit;
	vec3_t          hitPoint;
	vec3_t          hitNormal;
	float           fraction;
	physBodyHandle_t body;
} physRayResult_t;

qboolean Phys_RayCast(const vec3_t from, const vec3_t to, physRayResult_t *result);
int      Phys_OverlapSphere(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
int      Phys_OverlapBox(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults);

/* debug */
void Phys_DebugDraw(void);
int  Phys_GetBodyCount(void);
int  Phys_GetConstraintCount(void);

#ifdef __cplusplus
}
#endif
