/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Real-time cloth simulation using position-based dynamics with
Gauss-Seidel constraint projection. Supports:
- Stretch, shear, and bend constraints
- Self-collision via spatial hashing
- Wind and turbulence forces
- Attachment pins for character-driven cloth
- Adaptive stiffness with compliance-based XPBD
- Triangle-level collision with scene geometry
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define CLOTH_MAX_PARTICLES     4096
#define CLOTH_MAX_CONSTRAINTS   16384
#define CLOTH_MAX_PINS          64
#define CLOTH_MAX_INSTANCES     32
#define CLOTH_HASH_SIZE         1024

typedef enum {
	CLOTH_CONSTRAINT_STRETCH,
	CLOTH_CONSTRAINT_SHEAR,
	CLOTH_CONSTRAINT_BEND,
	CLOTH_CONSTRAINT_LONG_RANGE
} clothConstraintType_t;

typedef struct clothParticle_s {
	vec3_t  position;
	vec3_t  predicted;
	vec3_t  velocity;
	vec3_t  normal;
	vec2_t  texCoord;
	float   invMass;
	int     pinned;
	vec3_t  pinTarget;
} clothParticle_t;

typedef struct clothConstraint_s {
	int                     p0, p1;
	float                   restLength;
	float                   compliance;
	float                   lambda;
	clothConstraintType_t   type;
} clothConstraint_t;

typedef struct clothBendConstraint_s {
	int     p0, p1, p2, p3;
	float   restAngle;
	float   compliance;
	float   lambda;
} clothBendConstraint_t;

typedef struct clothConfig_s {
	float   gravity;
	float   damping;
	float   stretchCompliance;
	float   shearCompliance;
	float   bendCompliance;
	float   friction;
	float   thickness;
	float   windStrength;
	float   windTurbulence;
	vec3_t  windDirection;
	int     solverIterations;
	int     collisionIterations;
	float   selfCollisionRadius;
	qboolean selfCollision;
	float   sleepThreshold;
} clothConfig_t;

typedef int clothHandle_t;

clothHandle_t Cloth_Create(int width, int height, const vec3_t origin,
                           float particleSpacing, const clothConfig_t *config);
void          Cloth_Destroy(clothHandle_t handle);
void          Cloth_DefaultConfig(clothConfig_t *config);

void Cloth_Simulate(clothHandle_t handle, float dt);
void Cloth_SimulateAll(float dt);

void Cloth_PinParticle(clothHandle_t handle, int particleIndex, const vec3_t position);
void Cloth_UnpinParticle(clothHandle_t handle, int particleIndex);
void Cloth_MovePin(clothHandle_t handle, int particleIndex, const vec3_t position);
void Cloth_PinEdge(clothHandle_t handle, int edge, const vec3_t offset);

void Cloth_ApplyForce(clothHandle_t handle, const vec3_t force);
void Cloth_ApplyWindGust(clothHandle_t handle, const vec3_t direction, float strength, float duration);
void Cloth_ApplyImpact(clothHandle_t handle, const vec3_t point, const vec3_t force, float radius);

int  Cloth_GetParticleCount(clothHandle_t handle);
void Cloth_GetParticlePositions(clothHandle_t handle, float *positions, int maxParticles);
void Cloth_GetParticleNormals(clothHandle_t handle, float *normals, int maxParticles);
void Cloth_GetParticleTexCoords(clothHandle_t handle, float *texCoords, int maxParticles);

void Cloth_SetWind(clothHandle_t handle, const vec3_t direction, float strength);

/* Collide free particles against Phys_* world (ray along motion). */
void Cloth_CollideWorld(clothHandle_t handle);
void Cloth_CollideWorldAll(void);
/* Emit stretch edges into PhysDebug line buffer when phys_debugDraw is on. */
void Cloth_DebugDraw(clothHandle_t handle);
void Cloth_DebugDrawAll(void);

void Cloth_Init(void);
void Cloth_Shutdown(void);
int  Cloth_GetActiveCount(void);

#ifdef __cplusplus
}
#endif
