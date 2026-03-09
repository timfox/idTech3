/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Navier-Stokes fluid simulation cvar management and emitter system.
This module owns fluid sim cvars and the emitter array.
GPU compute dispatch is handled by vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_fluidsim.h"

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

static fluidEmitter_t fluidEmitters[MAX_FLUID_EMITTERS];
static int numFluidEmitters = 0;

/*
===============
FluidSim_RegisterCvars
===============
*/
void FluidSim_RegisterCvars(void) {
	r_fluidsim            = ri.Cvar_Get("r_fluidsim",            "0",      CVAR_ARCHIVE);
	r_fluidsim_viscosity  = ri.Cvar_Get("r_fluidsim_viscosity",  "0.0001", CVAR_ARCHIVE);
	r_fluidsim_diffusion  = ri.Cvar_Get("r_fluidsim_diffusion",  "0.0001", CVAR_ARCHIVE);
	r_fluidsim_dissipation= ri.Cvar_Get("r_fluidsim_dissipation","0.995",  CVAR_ARCHIVE);
	r_fluidsim_buoyancy   = ri.Cvar_Get("r_fluidsim_buoyancy",   "1.0",    CVAR_ARCHIVE);
	r_fluidsim_vorticity  = ri.Cvar_Get("r_fluidsim_vorticity",  "0.3",    CVAR_ARCHIVE);
	r_fluidsim_windX      = ri.Cvar_Get("r_fluidsim_windX",      "0.0",    CVAR_ARCHIVE);
	r_fluidsim_windY      = ri.Cvar_Get("r_fluidsim_windY",      "0.0",    CVAR_ARCHIVE);
	r_fluidsim_windZ      = ri.Cvar_Get("r_fluidsim_windZ",      "0.0",    CVAR_ARCHIVE);
	r_fluidsim_gridScale  = ri.Cvar_Get("r_fluidsim_gridScale",  "1.0",    CVAR_ARCHIVE);
	r_fluidsim_iterations = ri.Cvar_Get("r_fluidsim_iterations", "40",     CVAR_ARCHIVE);
	ri.Cvar_CheckRange( r_fluidsim_iterations, "1", "64", CV_INTEGER );

	ri.Cvar_SetDescription(r_fluidsim, "Deprecated: use r_fogFluid to enable fluid-driven volumetric fog.");
	ri.Printf(PRINT_ALL, "Navier-Stokes fluid sim: cvars registered (r_fogFluid controls enable)\n");
}

qboolean FluidSim_IsEnabled(void) {
	return r_fogFluid && r_fogFluid->integer > 0;
}

float FluidSim_GetViscosity(void) { return r_fluidsim_viscosity ? r_fluidsim_viscosity->value : 0.0001f; }
float FluidSim_GetDiffusion(void) { return r_fluidsim_diffusion ? r_fluidsim_diffusion->value : 0.0001f; }
float FluidSim_GetDissipation(void) { return r_fluidsim_dissipation ? r_fluidsim_dissipation->value : 0.995f; }
float FluidSim_GetBuoyancy(void) {
	return r_fogFluidBuoyancy ? r_fogFluidBuoyancy->value : (r_fluidsim_buoyancy ? r_fluidsim_buoyancy->value : 1.0f);
}
float FluidSim_GetVorticity(void) {
	return r_fogFluidVorticity ? r_fogFluidVorticity->value : (r_fluidsim_vorticity ? r_fluidsim_vorticity->value : 0.3f);
}
float FluidSim_GetGridScale(void) { return r_fluidsim_gridScale ? r_fluidsim_gridScale->value : 1.0f; }
int   FluidSim_GetJacobiIterations(void) { return r_fluidsim_iterations ? r_fluidsim_iterations->integer : FLUID_JACOBI_ITERS; }

void FluidSim_GetWindForce(float *x, float *y, float *z) {
	if (x) *x = r_fluidsim_windX ? r_fluidsim_windX->value : 0.0f;
	if (y) *y = r_fluidsim_windY ? r_fluidsim_windY->value : 0.0f;
	if (z) *z = r_fluidsim_windZ ? r_fluidsim_windZ->value : 0.0f;
}

void FluidSim_AddEmitter(const fluidEmitter_t *emitter) {
	if (!emitter || numFluidEmitters >= MAX_FLUID_EMITTERS) {
		return;
	}
	Com_Memcpy(&fluidEmitters[numFluidEmitters], emitter, sizeof(fluidEmitter_t));
	/* Clamp radius to avoid division issues in shader; shader uses max(radius, 0.001) */
	if ( fluidEmitters[numFluidEmitters].radius < 0.0f ) {
		fluidEmitters[numFluidEmitters].radius = 0.0f;
	}
	numFluidEmitters++;
}

void FluidSim_ClearEmitters(void) {
	numFluidEmitters = 0;
}

int FluidSim_GetEmitterCount(void) {
	return numFluidEmitters;
}

const fluidEmitter_t *FluidSim_GetEmitter(int index) {
	if (index < 0 || index >= numFluidEmitters) {
		return NULL;
	}
	return &fluidEmitters[index];
}
