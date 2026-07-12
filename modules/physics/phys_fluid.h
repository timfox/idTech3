/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Lightweight SPH-ish fluid companion. Particles interact with each other
and collide against the Box3D Soft Step world via Phys_RayCast / overlap
impulses. Not a production CFD solver — demo / VFX scale.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define PHYS_FLUID_MAX_EMITTERS   8
#define PHYS_FLUID_MAX_PARTICLES  768

typedef int physFluidHandle_t;

typedef struct physFluidConfig_s {
	float restDensity;
	float gasConstant;   /* pressure stiffness */
	float viscosity;
	float smoothingRadius;
	float particleMass;
	float gravity;
	float damping;
	float worldBounce;
	float rigidCoupling; /* impulse scale onto Soft Step bodies */
} physFluidConfig_t;

void              PhysFluid_DefaultConfig( physFluidConfig_t *cfg );
void              PhysFluid_Init( void );
void              PhysFluid_Shutdown( void );
void              PhysFluid_Step( float dt );
void              PhysFluid_DebugDraw( void );
int               PhysFluid_GetActiveCount( void );

/* Spawn a packed blob of fluid particles (count clamped). Returns emitter handle. */
physFluidHandle_t PhysFluid_CreateBlob( const vec3_t origin, int count, float spacing,
	const physFluidConfig_t *cfg );
void              PhysFluid_Destroy( physFluidHandle_t handle );
void              PhysFluid_Clear( void );

#ifdef __cplusplus
}
#endif
