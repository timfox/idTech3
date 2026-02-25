/*
===========================================================================
Copyright (C) 2026 id Tech 3 Contributors

GPU-based Navier-Stokes fluid simulation using Vulkan compute shaders.
Implements Jos Stam's "Stable Fluids" algorithm on the GPU for
real-time smoke, fog, and fire density simulation.

Pipeline:
  1. External forces injection (emitters, wind, buoyancy)
  2. Advection (semi-Lagrangian, unconditionally stable)
  3. Diffusion (Jacobi iteration)
  4. Pressure projection (Helmholtz-Hodge decomposition)
     a. Compute divergence
     b. Pressure solve (Jacobi iteration)
     c. Subtract pressure gradient
  5. Density advection (for smoke/fog visualization)
===========================================================================
*/

#pragma once

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLUID_GRID_SIZE     64
#define FLUID_JACOBI_ITERS  40
#define FLUID_WORKGROUP_SIZE 8

typedef enum {
	FLUID_PASS_ADVECTION = 0,
	FLUID_PASS_DIFFUSION,
	FLUID_PASS_DIVERGENCE,
	FLUID_PASS_PRESSURE,
	FLUID_PASS_GRADIENT_SUB,
	FLUID_PASS_FORCE_INJECT,
	FLUID_PASS_DENSITY_ADVECT,
	FLUID_PASS_COUNT
} fluidPass_t;

typedef struct fluidEmitter_s {
	float position[3];
	float radius;
	float density;
	float temperature;
	float velocity[3];
	float pad;
} fluidEmitter_t;

#define MAX_FLUID_EMITTERS 16

typedef struct fluidSimParams_s {
	float viscosity;
	float diffusion;
	float dissipation;
	float buoyancy;

	float ambientTemperature;
	float dt;
	float gridScale;
	float vorticityStrength;

	float windForce[3];
	int   jacobiIterations;

	float densityDissipation;
	float temperatureDissipation;
	float velocityDissipation;
	float padding;
} fluidSimParams_t;

typedef struct fluidPushConstants_s {
	float params[4];
	float gridSize[4];
	float dt;
	float viscosity;
	float diffusion;
	float dissipation;
} fluidPushConstants_t;

void FluidSim_Init(void);
void FluidSim_Shutdown(void);
void FluidSim_RegisterCvars(void);
qboolean FluidSim_IsEnabled(void);
void FluidSim_Step(float dt);
void FluidSim_AddEmitter(const fluidEmitter_t *emitter);
void FluidSim_ClearEmitters(void);
void FluidSim_Reset(void);

#ifdef __cplusplus
}
#endif
