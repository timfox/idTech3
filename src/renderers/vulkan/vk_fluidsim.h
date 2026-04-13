/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Navier-Stokes fluid simulation public API.
Manages cvars and parameter interface for the GPU-based fluid
simulation. Compute dispatch is in vk_volumetric_internal.c (fluid passes).
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLUID_GRID_SIZE      64
#define FLUID_WORKGROUP_SIZE  8
#define MAX_FLUID_EMITTERS   16

typedef struct fluidEmitter_s {
	float position[3];
	float radius;
	float density;
	float temperature;
	float velocity[3];
	float pad;
} fluidEmitter_t;

float     FluidSim_GetBuoyancy(void);
float     FluidSim_GetVorticity(void);

void      FluidSim_AddEmitter(const fluidEmitter_t *emitter);
void      FluidSim_ClearEmitters(void);
int       FluidSim_GetEmitterCount(void);
const fluidEmitter_t *FluidSim_GetEmitter(int index);

#ifdef __cplusplus
}
#endif
