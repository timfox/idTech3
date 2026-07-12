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

#include "q_shared.h"

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
	PHYS_CONSTRAINT_SLIDER,   /* Box3D prismatic */
	PHYS_CONSTRAINT_CONE_TWIST,
	PHYS_CONSTRAINT_GENERIC_6DOF,
	PHYS_CONSTRAINT_FIXED,
	PHYS_CONSTRAINT_DISTANCE, /* soft spring / rope (Box3D distance joint) */
	PHYS_CONSTRAINT_WHEEL,    /* Box3D wheel (chassis A, wheel B) */
	PHYS_CONSTRAINT_MOTOR,    /* Box3D motor joint (relative vel / pose) */
	PHYS_CONSTRAINT_FILTER,   /* Soft Step: disable collision between body pair */
	PHYS_CONSTRAINT_PARALLEL  /* Soft Step: keep axes parallel (upright spring) */
} physConstraintType_t;

/* Bitmask for Phys_SetBodyMotionLocks / physBodyDef_t.motionLocks */
enum {
	PHYS_LOCK_LIN_X = 1 << 0,
	PHYS_LOCK_LIN_Y = 1 << 1,
	PHYS_LOCK_LIN_Z = 1 << 2,
	PHYS_LOCK_ANG_X = 1 << 3,
	PHYS_LOCK_ANG_Y = 1 << 4,
	PHYS_LOCK_ANG_Z = 1 << 5
};

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
	float           gravityScale;   /* 1 = default; 0 = zero-G */
	int             motionLocks;    /* PHYS_LOCK_* bits */
	qboolean        isSensor;       /* trigger volume (Box3D sensor shape) */
	int             collisionGroup;
	int             collisionMask;
	int             materialId; /* phys_materials.h PHYS_MAT_* */
	/* PHYS_SHAPE_CONVEX_HULL: xyz triples (count = hullPointCount) */
	const float    *hullPoints;
	int             hullPointCount;
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
	qboolean             enableMotor;
	float                motorSpeed;
	float                maxMotorForce;
	float                breakForce;   /* Soft Step joint force threshold (0 = none) */
	float                breakTorque;  /* Soft Step joint torque threshold (0 = none) */
	/* Soft Step spherical / cone-twist (radians; 0 cone = default 0.5) */
	float                coneAngle;
	float                twistLower;
	float                twistUpper;
	/* Soft Step spring override (0 hertz = softness mapping / joint default) */
	float                springHertz;
	float                springDamping;
	float                maxSpringTorque; /* parallel joint */
} physConstraintDef_t;

#define PHYS_RAGDOLL_MAX_BONES 32

typedef struct physRagdollBoneDef_s {
	float           radius;
	float           height;
	int             parent;         /* -1 = root */
	vec3_t          localOffset;    /* from root (Quake units) */
	char            tagName[32];    /* optional MD3 tag for game bind */
} physRagdollBoneDef_t;

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

	/* Optional bind: numBones > 0 uses bones[]; else procedural 11-bone layout */
	int             numBones;
	physRagdollBoneDef_t bones[PHYS_RAGDOLL_MAX_BONES];
	char            modelPath[MAX_QPATH];
} physRagdollDef_t;

typedef struct physSoftStepProfile_s {
	float stepMs;
	float collideMs;
	float solveMs;
	float jointEventsMs;
	int   bodyCount;
	int   constraintCount;
	int   contactHitCount;
	int   contactCount;   /* Soft Step awake/graph contacts */
	int   shapeCount;
	int   islandCount;
} physSoftStepProfile_t;

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
qboolean    Phys_LoadBSPCollision(void);
physBodyHandle_t Phys_AddStaticTriMesh(const float *verts, int numVerts, const int *indices, int numIndices);
/* Quake Z-up height samples on an X/Y grid → Box3D height field (rotated to Z-up). */
physBodyHandle_t Phys_AddStaticHeightField(const float *heights, int countX, int countY,
	float cellSize, float heightScale, const vec3_t origin);
/* One static body with N child boxes (Box3D compound; Bullet = separate boxes / AABB). */
physBodyHandle_t Phys_AddStaticCompoundBoxes(const float *centersXYZ, const float *halfExtentsXYZ, int count);

/* Capsule character step: Box3D CastMover/CollideMover/SolvePlanes; else returns qfalse. */
qboolean Phys_MoverStep(vec3_t origin, vec3_t velocity, float radius, float height,
	const vec3_t wishDir, float wishSpeed, float dt, qboolean jump, qboolean *groundedOut);

int Phys_GetWorkerCount(void);

typedef enum {
	PHYS_BACKEND_NONE = 0,
	PHYS_BACKEND_BOX3D,
	PHYS_BACKEND_BULLET
} physBackendKind_t;

physBackendKind_t Phys_GetBackend(void);
const char       *Phys_GetBackendName(void);

/* rigid bodies */
physBodyHandle_t Phys_CreateBody(const physBodyDef_t *def);
void             Phys_DestroyBody(physBodyHandle_t handle);
void             Phys_GetBodyTransform(physBodyHandle_t handle, physTransform_t *out);
void             Phys_SetBodyTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot);
/* Kinematic platforms: Soft Step interpolates to target over timeStep (Box3D SetTargetTransform). */
void             Phys_SetBodyTargetTransform(physBodyHandle_t handle, const vec3_t pos, const vec3_t rot, float timeStep);
void             Phys_SetBodyGravityScale(physBodyHandle_t handle, float scale);
void             Phys_SetBodyMotionLocks(physBodyHandle_t handle, int lockBits);
void             Phys_ApplyForce(physBodyHandle_t handle, const vec3_t force, const vec3_t point);
void             Phys_ApplyImpulse(physBodyHandle_t handle, const vec3_t impulse, const vec3_t point);
void             Phys_ApplyTorque(physBodyHandle_t handle, const vec3_t torque);
void             Phys_SetBodyVelocity(physBodyHandle_t handle, const vec3_t linear, const vec3_t angular);
void             Phys_SetBodyActive(physBodyHandle_t handle, qboolean active);
physBodyType_t   Phys_GetBodyType(physBodyHandle_t handle);
qboolean         Phys_IsBodyDynamic(physBodyHandle_t handle);
int              Phys_ApplyImpulseRadius(const vec3_t center, float radius, float magnitude, float falloff);

/* constraints */
physConstraintHandle_t Phys_CreateConstraint(const physConstraintDef_t *def);
void                   Phys_DestroyConstraint(physConstraintHandle_t handle);
void                   Phys_SetConstraintLimits(physConstraintHandle_t handle, float lower, float upper);
void                   Phys_SetConstraintMotor(physConstraintHandle_t handle, qboolean enable,
	float speed, float maxForce);
void                   Phys_SetConstraintBreakForce(physConstraintHandle_t handle, float force, float torque);
void                   Phys_SetWheelSteering(physConstraintHandle_t handle, float angleRadians, float maxTorque);
void                   Phys_SetConstraintSpring(physConstraintHandle_t handle, qboolean enable,
	float hertz, float dampingRatio);
void                   Phys_SetSphericalLimits(physConstraintHandle_t handle, float coneAngleRadians,
	float twistLowerRadians, float twistUpperRadians);
void                   Phys_GetConstraintReaction(physConstraintHandle_t handle, vec3_t forceOut, vec3_t torqueOut);

/* Multi-shape / filters (Box3D Soft Step; Bullet best-effort) */
int              Phys_AttachShape(physBodyHandle_t body, const physBodyDef_t *shapeDef);
void             Phys_DestroyAttachedShape(physBodyHandle_t body, int shapeIndex);
void             Phys_SetBodyFilter(physBodyHandle_t body, int categoryBits, int maskBits);
/* groupIndex: Soft Step same negative index = no self-collide; 0 clears to default */
void             Phys_SetBodyFilterEx(physBodyHandle_t body, int categoryBits, int maskBits, int groupIndex);

/* ragdoll / Procedural animation */
physRagdollHandle_t Phys_CreateRagdoll(const physRagdollDef_t *def);
void                Phys_DestroyRagdoll(physRagdollHandle_t handle);
void                Phys_RagdollApplyImpact(physRagdollHandle_t handle, const vec3_t point, const vec3_t impulse, float radius);
void                Phys_RagdollSetBalance(physRagdollHandle_t handle, qboolean enabled, const vec3_t target);
void                Phys_RagdollReach(physRagdollHandle_t handle, int limbIndex, const vec3_t target, float strength);
void                Phys_RagdollGetBoneTransform(physRagdollHandle_t handle, int boneIndex, physTransform_t *out);
void                Phys_RagdollSetMuscleStiffness(physRagdollHandle_t handle, float stiffness);
void                Phys_RagdollBlendToAnimation(physRagdollHandle_t handle, float blend);
void                Phys_RagdollApplyBoneTorque(physRagdollHandle_t handle, int boneIndex, const vec3_t torque);
void                Phys_RagdollSetBoneAnimTarget(physRagdollHandle_t handle, int boneIndex,
	const vec3_t position, const vec3_t rotationDeg);
void                Phys_RagdollClearAnimTargets(physRagdollHandle_t handle);
int                 Phys_GetRagdollCount(void);

/* Digital Molecular Matter (DMM) */
dmmObjectHandle_t Dmm_CreateObject(const dmmObjectDef_t *def);
void              Dmm_DestroyObject(dmmObjectHandle_t handle);
void              Dmm_ApplyForce(dmmObjectHandle_t handle, const vec3_t force, const vec3_t point);
void              Dmm_ApplyImpact(dmmObjectHandle_t handle, const vec3_t point, const vec3_t direction, float energy);
void              Dmm_GetState(dmmObjectHandle_t handle, dmmState_t *out);
qboolean          Dmm_IsFractured(dmmObjectHandle_t handle);
int               Dmm_GetFragments(dmmObjectHandle_t handle, physBodyHandle_t *fragments, int maxFragments);
void              Dmm_SetMaterialParams(dmmObjectHandle_t handle, float stiffness, float yield, float fracture);
/* Soft Step: destroy proxy body and spawn rigid debris chunks. */
int               Dmm_SpawnFragments(dmmObjectHandle_t handle, const vec3_t impactPoint, float energy);

/* ray cast / queries */
typedef struct physRayResult_s {
	qboolean        hit;
	vec3_t          hitPoint;
	vec3_t          hitNormal;
	float           fraction;
	physBodyHandle_t body;
	unsigned        userMaterialId; /* Soft Step mesh/child material */
	int             triangleIndex;
	int             childIndex;
} physRayResult_t;

/* Soft Step query filter; category/mask 0 = Box3D default (accept all) */
typedef struct physQueryFilter_s {
	unsigned categoryBits;
	unsigned maskBits;
} physQueryFilter_t;

typedef struct physContact_s {
	physBodyHandle_t otherBody;
	vec3_t           point;
	vec3_t           normal;
	float            normalImpulse;
	float            separation;
} physContact_t;

qboolean Phys_RayCast(const vec3_t from, const vec3_t to, physRayResult_t *result);
qboolean Phys_RayCastFiltered(const vec3_t from, const vec3_t to, physRayResult_t *result,
	const physQueryFilter_t *filter);
qboolean Phys_ConvexSweep(const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result);
qboolean Phys_ConvexSweepFiltered(const physBodyDef_t *shapeDef, const vec3_t from, const vec3_t to,
	const vec3_t rotation, physRayResult_t *result, const physQueryFilter_t *filter);
int      Phys_OverlapSphere(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
int      Phys_OverlapBox(const vec3_t center, const vec3_t halfExtents, physBodyHandle_t *results, int maxResults);
/* Soft Step OverlapShape (sphere proxy); falls back to OverlapSphere. */
int      Phys_OverlapShape(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults);
int      Phys_OverlapShapeFiltered(const vec3_t center, float radius, physBodyHandle_t *results, int maxResults,
	const physQueryFilter_t *filter);
int      Phys_GetBodyContacts(physBodyHandle_t body, physContact_t *out, int maxOut);
void     Phys_SetHitEventThreshold(float approachSpeed);

void     Phys_SetBodyMaterial(physBodyHandle_t handle, int materialId);
int      Phys_GetBodyMaterial(physBodyHandle_t handle);
void     Phys_SetBodyFriction(physBodyHandle_t handle, float friction);
void     Phys_SetBodyRestitution(physBodyHandle_t handle, float restitution);

/* Soft Step profile / recording (Box3D; no-ops on Bullet) */
void     Phys_GetSoftStepProfile(physSoftStepProfile_t *out);
void     Phys_StartRecording(void);
void     Phys_StopRecording(const char *path);
/* Validate a saved Soft Step recording reproduces StateHash (QA). */
qboolean  Phys_ValidateReplay(const char *path);
void      Phys_DumpWorld(void);

/* debug */
void Phys_DebugDraw(void);
int  Phys_GetBodyCount(void);
int  Phys_GetConstraintCount(void);

#ifdef __cplusplus
}
#endif
