/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Soft-blob XPBD lattice solver. Particles + distance constraints collide
against the Box3D Soft Step world through Phys_RayCast. Not FEM; good for
jelly props / deformable debris that still "feel" the rigid substrate.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define PHYS_SOFTBLOB_MAX           16
#define PHYS_SOFTBLOB_PARTICLES_MAX 512
#define PHYS_SOFTBLOB_CONSTRAINTS_MAX 2048

typedef int physSoftBlobHandle_t;

typedef struct physSoftBlobConfig_s {
	float gravity;
	float damping;
	float compliance;
	float thickness;
	float particleMass;
	int   solverIterations;
} physSoftBlobConfig_t;

void                 SoftBlob_DefaultConfig( physSoftBlobConfig_t *cfg );
void                 SoftBlob_Init( void );
void                 SoftBlob_Shutdown( void );
void                 SoftBlob_Step( float dt );
void                 SoftBlob_DebugDraw( void );
int                  SoftBlob_GetActiveCount( void );

/* axis-aligned lattice: res^3 particles, spacing in Quake units */
physSoftBlobHandle_t SoftBlob_CreateLattice( const vec3_t origin, int res, float spacing,
	const physSoftBlobConfig_t *cfg );
void                 SoftBlob_Destroy( physSoftBlobHandle_t handle );
void                 SoftBlob_ApplyImpulse( physSoftBlobHandle_t handle, const vec3_t point,
	const vec3_t impulse, float radius );
void                 SoftBlob_PinCorner( physSoftBlobHandle_t handle, int cornerIndex );

#ifdef __cplusplus
}
#endif
