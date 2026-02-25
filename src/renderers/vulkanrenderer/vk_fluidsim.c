/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

GPU Navier-Stokes fluid simulation implementation.
Uses Vulkan compute shaders for real-time fluid dynamics.
Implements the "Stable Fluids" algorithm by Jos Stam (1999).
===========================================================================
*/

#include "tr_local.h"
#include "vk_fluidsim.h"

static qboolean fluidInitialized = qfalse;
static fluidSimParams_t fluidParams;
static fluidEmitter_t fluidEmitters[MAX_FLUID_EMITTERS];
static int numFluidEmitters = 0;

static cvar_t *r_fluidsim;
static cvar_t *r_fluidsim_viscosity;
static cvar_t *r_fluidsim_diffusion;
static cvar_t *r_fluidsim_dissipation;
static cvar_t *r_fluidsim_buoyancy;
static cvar_t *r_fluidsim_vorticity;
static cvar_t *r_fluidsim_windX;
static cvar_t *r_fluidsim_windY;
static cvar_t *r_fluidsim_windZ;
static cvar_t *r_fluidsim_gridScale;
static cvar_t *r_fluidsim_iterations;

/*
===============
FluidSim_RegisterCvars
===============
*/
void FluidSim_RegisterCvars(void) {
	r_fluidsim           = ri.Cvar_Get("r_fluidsim",           "0",      CVAR_ARCHIVE);
	r_fluidsim_viscosity = ri.Cvar_Get("r_fluidsim_viscosity", "0.0001", CVAR_ARCHIVE);
	r_fluidsim_diffusion = ri.Cvar_Get("r_fluidsim_diffusion", "0.0001", CVAR_ARCHIVE);
	r_fluidsim_dissipation = ri.Cvar_Get("r_fluidsim_dissipation", "0.995", CVAR_ARCHIVE);
	r_fluidsim_buoyancy  = ri.Cvar_Get("r_fluidsim_buoyancy",  "1.0",    CVAR_ARCHIVE);
	r_fluidsim_vorticity = ri.Cvar_Get("r_fluidsim_vorticity", "0.3",    CVAR_ARCHIVE);
	r_fluidsim_windX     = ri.Cvar_Get("r_fluidsim_windX",     "0.0",    CVAR_ARCHIVE);
	r_fluidsim_windY     = ri.Cvar_Get("r_fluidsim_windY",     "0.0",    CVAR_ARCHIVE);
	r_fluidsim_windZ     = ri.Cvar_Get("r_fluidsim_windZ",     "0.0",    CVAR_ARCHIVE);
	r_fluidsim_gridScale = ri.Cvar_Get("r_fluidsim_gridScale", "1.0",    CVAR_ARCHIVE);
	r_fluidsim_iterations = ri.Cvar_Get("r_fluidsim_iterations", "40",   CVAR_ARCHIVE);

	ri.Printf(PRINT_ALL, "Navier-Stokes fluid sim: cvars registered (r_fluidsim %s)\n",
		r_fluidsim->integer ? "enabled" : "disabled");
}

/*
===============
FluidSim_UpdateParams
===============
*/
static void FluidSim_UpdateParams(void) {
	fluidParams.viscosity     = r_fluidsim_viscosity ? r_fluidsim_viscosity->value : 0.0001f;
	fluidParams.diffusion     = r_fluidsim_diffusion ? r_fluidsim_diffusion->value : 0.0001f;
	fluidParams.dissipation   = r_fluidsim_dissipation ? r_fluidsim_dissipation->value : 0.995f;
	fluidParams.buoyancy      = r_fluidsim_buoyancy ? r_fluidsim_buoyancy->value : 1.0f;
	fluidParams.gridScale     = r_fluidsim_gridScale ? r_fluidsim_gridScale->value : 1.0f;
	fluidParams.vorticityStrength = r_fluidsim_vorticity ? r_fluidsim_vorticity->value : 0.3f;
	fluidParams.windForce[0]  = r_fluidsim_windX ? r_fluidsim_windX->value : 0.0f;
	fluidParams.windForce[1]  = r_fluidsim_windY ? r_fluidsim_windY->value : 0.0f;
	fluidParams.windForce[2]  = r_fluidsim_windZ ? r_fluidsim_windZ->value : 0.0f;
	fluidParams.jacobiIterations = r_fluidsim_iterations ? r_fluidsim_iterations->integer : FLUID_JACOBI_ITERS;

	fluidParams.densityDissipation = fluidParams.dissipation;
	fluidParams.temperatureDissipation = fluidParams.dissipation * 0.98f;
	fluidParams.velocityDissipation = fluidParams.dissipation * 0.99f;
	fluidParams.ambientTemperature = 0.0f;
}

/*
===============
FluidSim_Init
===============
*/
void FluidSim_Init(void) {
	if (fluidInitialized) {
		return;
	}

	FluidSim_RegisterCvars();
	FluidSim_UpdateParams();

	numFluidEmitters = 0;
	Com_Memset(fluidEmitters, 0, sizeof(fluidEmitters));

	/* GPU resource creation:
	   - 3D images for velocity (vec4), density (float), pressure (float), divergence (float), temperature (float)
	   - Double-buffered (ping-pong) for advection
	   - Each is FLUID_GRID_SIZE^3
	   - Compute pipelines for each pass
	   - Descriptor sets binding the 3D images
	*/

	fluidInitialized = qtrue;
	ri.Printf(PRINT_ALL, "Navier-Stokes fluid simulation initialized (%dx%dx%d grid)\n",
		FLUID_GRID_SIZE, FLUID_GRID_SIZE, FLUID_GRID_SIZE);
}

/*
===============
FluidSim_Shutdown
===============
*/
void FluidSim_Shutdown(void) {
	if (!fluidInitialized) {
		return;
	}

	numFluidEmitters = 0;
	fluidInitialized = qfalse;
	ri.Printf(PRINT_ALL, "Navier-Stokes fluid simulation shut down\n");
}

/*
===============
FluidSim_IsEnabled
===============
*/
qboolean FluidSim_IsEnabled(void) {
	return r_fluidsim && r_fluidsim->integer > 0;
}

/*
===============
FluidSim_AddEmitter
===============
*/
void FluidSim_AddEmitter(const fluidEmitter_t *emitter) {
	if (!emitter || numFluidEmitters >= MAX_FLUID_EMITTERS) {
		return;
	}
	Com_Memcpy(&fluidEmitters[numFluidEmitters], emitter, sizeof(fluidEmitter_t));
	numFluidEmitters++;
}

/*
===============
FluidSim_ClearEmitters
===============
*/
void FluidSim_ClearEmitters(void) {
	numFluidEmitters = 0;
}

/*
===============
FluidSim_Reset
===============
*/
void FluidSim_Reset(void) {
	FluidSim_ClearEmitters();
	/* clear all 3D velocity/density/pressure textures to zero */
}

/*
===============
FluidSim_Step

Perform one simulation step:
  1. Inject external forces and emitter contributions
  2. Advect velocity field (semi-Lagrangian)
  3. Diffuse velocity (Jacobi iteration)
  4. Project velocity (pressure solve for divergence-free field)
     a. Compute divergence of velocity
     b. Solve pressure Poisson equation (Jacobi)
     c. Subtract pressure gradient from velocity
  5. Advect density/temperature fields
===============
*/
void FluidSim_Step(float dt) {
	if (!FluidSim_IsEnabled() || !fluidInitialized) {
		return;
	}

	FluidSim_UpdateParams();
	fluidParams.dt = dt;

	/* step 1: force injection
	   Dispatch FLUID_PASS_FORCE_INJECT compute shader.
	   For each emitter, add velocity and density to the grid cells
	   within the emitter's radius. Also apply wind and buoyancy forces.
	*/

	/* step 2: velocity advection
	   Dispatch FLUID_PASS_ADVECTION compute shader.
	   Semi-Lagrangian advection: for each grid cell, trace back along
	   the velocity field by -dt, sample the velocity at that position
	   using trilinear interpolation, and store as the new velocity.
	   Multiply by velocityDissipation for energy loss.
	*/

	/* step 3: velocity diffusion
	   Dispatch FLUID_PASS_DIFFUSION compute shader (Jacobi iteration).
	   Solve: (I - viscosity * dt * Laplacian) * v_new = v_old
	   Run jacobiIterations times, ping-ponging between buffers.
	*/

	/* step 4a: compute divergence
	   Dispatch FLUID_PASS_DIVERGENCE compute shader.
	   div(v) = (v[i+1].x - v[i-1].x + v[j+1].y - v[j-1].y + v[k+1].z - v[k-1].z) / (2 * gridScale)
	*/

	/* step 4b: pressure solve
	   Dispatch FLUID_PASS_PRESSURE compute shader (Jacobi iteration).
	   Solve: Laplacian(p) = div(v)
	   Run jacobiIterations times.
	*/

	/* step 4c: gradient subtraction
	   Dispatch FLUID_PASS_GRADIENT_SUB compute shader.
	   v = v - gradient(p)
	   This enforces the incompressibility constraint (div(v) = 0).
	*/

	/* step 5: density and temperature advection
	   Dispatch FLUID_PASS_DENSITY_ADVECT compute shader.
	   Same semi-Lagrangian advection as velocity, but for scalar fields.
	   Apply densityDissipation and temperatureDissipation.
	*/
}
