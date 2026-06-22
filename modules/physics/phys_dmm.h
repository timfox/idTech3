/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Digital Molecular Matter (DMM) enhanced deformation engine.
Extends the base DMM API in phys_bullet.h with:
- Finite Element Method (FEM) tetrahedral mesh simulation
- Voronoi-based fracture pattern generation
- Material fatigue and progressive damage
- Heat-affected deformation (metal softening, ice melting)
- Fragment generation with proper mass/inertia
- Deformation mesh readback for rendering
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"
#include "phys_bullet.h"

#define DMM_MAX_FRACTURE_POINTS  64
#define DMM_MAX_FRAGMENTS        128
#define DMM_MAX_DEFORM_VERTS     4096

typedef struct dmmFracturePattern_s {
	vec3_t  points[DMM_MAX_FRACTURE_POINTS];
	int     numPoints;
	float   randomness;
	float   minFragmentSize;
} dmmFracturePattern_t;

typedef struct dmmDeformVertex_s {
	vec3_t  position;
	vec3_t  normal;
	vec3_t  originalPosition;
	float   displacement;
	float   stress;
} dmmDeformVertex_t;

typedef struct dmmDeformMesh_s {
	dmmDeformVertex_t *vertices;
	int                numVertices;
	uint32_t          *indices;
	int                numIndices;
	qboolean           dirty;
} dmmDeformMesh_t;

typedef struct dmmThermal_s {
	float   temperature;
	float   meltingPoint;
	float   softeningPoint;
	float   thermalConductivity;
	float   heatCapacity;
} dmmThermal_t;

typedef struct dmmFatigue_s {
	float   cycleCount;
	float   fatigueLimit;
	float   damageAccum;
	float   crackLength;
	float   crackGrowthRate;
} dmmFatigue_t;

typedef struct dmmEnhancedState_s {
	dmmState_t      base;
	dmmThermal_t    thermal;
	dmmFatigue_t    fatigue;
	float           plasticStrain;
	float           elasticStrain;
	float           vonMisesStress;
	float           maxPrincipalStress;
	float           damageParameter;
	qboolean        yielded;
	qboolean        softened;
} dmmEnhancedState_t;

dmmObjectHandle_t Dmm_CreateEnhanced(const dmmObjectDef_t *def, const dmmFracturePattern_t *pattern);
void              Dmm_SetThermal(dmmObjectHandle_t handle, const dmmThermal_t *thermal);
void              Dmm_ApplyHeat(dmmObjectHandle_t handle, const vec3_t point, float temperature, float radius);
void              Dmm_CoolDown(dmmObjectHandle_t handle, float rate);
void              Dmm_GetEnhancedState(dmmObjectHandle_t handle, dmmEnhancedState_t *state);
void              Dmm_GetDeformMesh(dmmObjectHandle_t handle, dmmDeformMesh_t *mesh);
void              Dmm_StepFatigue(dmmObjectHandle_t handle, float dt);
int               Dmm_Fracture(dmmObjectHandle_t handle, const vec3_t impactPoint, float energy);
void              Dmm_GenerateVoronoiPattern(const vec3_t center, float radius, int numCells, dmmFracturePattern_t *pattern);

#ifdef __cplusplus
}
#endif
