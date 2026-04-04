/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Navier-Stokes fluid simulation emitter API for volumetric fog.
Enable and tuning use r_fogFluid* cvars in tr_init.c; GPU compute dispatch is in vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_fluidsim.h"

static fluidEmitter_t fluidEmitters[MAX_FLUID_EMITTERS];
static int numFluidEmitters = 0;

float FluidSim_GetBuoyancy(void) {
	return r_fogFluidBuoyancy ? r_fogFluidBuoyancy->value : 1.0f;
}
float FluidSim_GetVorticity(void) {
	return r_fogFluidVorticity ? r_fogFluidVorticity->value : 0.3f;
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
