/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Particle / debris solver — Verlet integration with Box3D Soft Step collision
via Phys_RayCast / Phys_OverlapSphere. Does not own the rigid world.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define PHYS_PARTICLE_MAX 2048

typedef int physParticleSystemHandle_t;

typedef struct physParticleSpawn_s {
	vec3_t  origin;
	vec3_t  velocity;
	float   radius;
	float   lifetime;
	float   bounce;
	float   friction;
	float   gravityScale;
} physParticleSpawn_t;

void                        PhysParticles_Init( void );
void                        PhysParticles_Shutdown( void );
void                        PhysParticles_Step( float dt );
void                        PhysParticles_DebugDraw( void );
int                         PhysParticles_GetActiveCount( void );

physParticleSystemHandle_t  PhysParticles_CreateBurst( const vec3_t origin, int count,
	float speed, float lifetime );
int                         PhysParticles_Emit( const physParticleSpawn_t *spawn );
void                        PhysParticles_Clear( void );

#ifdef __cplusplus
}
#endif
