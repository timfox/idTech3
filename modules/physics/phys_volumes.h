/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Interaction volumes: buoyancy, fluid drag, one-shot impact, motion enter/exit.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "phys_bullet.h"

#define PHYS_VOLUME_MAX 128

typedef enum {
	PHYS_VOLUME_BUOYANCY = 0,
	PHYS_VOLUME_DRAG,
	PHYS_VOLUME_IMPACT,   /* apply radial impulse when a dynamic body enters */
	PHYS_VOLUME_MOTION    /* post enter/exit events (no solid response) */
} physVolumeType_t;

typedef int physVolumeHandle_t;

typedef struct physVolumeDef_s {
	physVolumeType_t type;
	vec3_t           center;
	vec3_t           halfExtents; /* AABB; if radius > 0, treated as sphere */
	float            radius;
	float            density;     /* buoyancy fluid density scale */
	float            linearDrag;
	float            angularDrag;
	float            impulseMagnitude; /* IMPACT */
	float            impulseRadius;
	int              entityNum;
} physVolumeDef_t;

void               PhysVolume_Init( void );
void               PhysVolume_Shutdown( void );
void               PhysVolume_Clear( void );
void               PhysVolume_Frame( float dt );

physVolumeHandle_t PhysVolume_Create( const physVolumeDef_t *def );
void               PhysVolume_Destroy( physVolumeHandle_t handle );
int                PhysVolume_GetActiveCount( void );

#ifdef __cplusplus
}
#endif
