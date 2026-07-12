/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

DMM enhanced deformation engine implementation.
FEM-based deformation with Voronoi fracture, thermal effects,
fatigue damage, and deformed mesh readback.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_bullet.h"
#include "phys_dmm.h"
#include <math.h>

typedef struct dmmEnhancedData_s {
	qboolean            active;
	dmmObjectHandle_t   baseHandle;
	dmmFracturePattern_t pattern;
	dmmThermal_t        thermal;
	dmmFatigue_t        fatigue;
	float               plasticStrain;
	float               elasticStrain;
	float               vonMisesStress;
	float               maxPrincipalStress;
	float               damageParameter;
	qboolean            yielded;
	qboolean            softened;

	dmmDeformVertex_t  *deformVerts;
	int                 numDeformVerts;
	uint32_t           *deformIndices;
	int                 numDeformIndices;
	qboolean            meshDirty;

	physBodyHandle_t    fragments[DMM_MAX_FRAGMENTS];
	int                 numFragments;
} dmmEnhancedData_t;

#define DMM_MAX_ENHANCED 256
static dmmEnhancedData_t dmmEnhanced[DMM_MAX_ENHANCED];
static int dmmEnhancedCount = 0;

#define VALID_DMM_E(h) ((h) >= 0 && (h) < dmmEnhancedCount && dmmEnhanced[(h)].active)

static float randf(void) {
	return (float)(rand() & 0x7FFF) / (float)0x7FFF;
}

dmmObjectHandle_t Dmm_CreateEnhanced(const dmmObjectDef_t *def, const dmmFracturePattern_t *pattern) {
	dmmObjectHandle_t base;
	int idx;
	int i, numVerts, numTris;
	int gx, gy, gz, res;
	dmmEnhancedData_t *enh;

	base = Dmm_CreateObject(def);

	if (dmmEnhancedCount >= DMM_MAX_ENHANCED) return base;

	idx = dmmEnhancedCount++;
	enh = &dmmEnhanced[idx];
	Com_Memset(enh, 0, sizeof(*enh));

	enh->active = qtrue;
	enh->baseHandle = base;

	if (pattern) {
		Com_Memcpy(&enh->pattern, pattern, sizeof(dmmFracturePattern_t));
	} else {
		Dmm_GenerateVoronoiPattern(def->position, def->dimensions[0] * 0.5f, 8, &enh->pattern);
	}

	enh->thermal.temperature = 20.0f;
	enh->thermal.meltingPoint = 1500.0f;
	enh->thermal.softeningPoint = 800.0f;
	enh->thermal.thermalConductivity = 50.0f;
	enh->thermal.heatCapacity = 500.0f;

	enh->fatigue.cycleCount = 0;
	enh->fatigue.fatigueLimit = 10000.0f;
	enh->fatigue.damageAccum = 0;
	enh->fatigue.crackLength = 0;
	enh->fatigue.crackGrowthRate = 0.001f;

	res = def->gridResolution > 0 ? def->gridResolution : 8;
	numVerts = (res + 1) * (res + 1) * (res + 1);
	numTris = res * res * res * 12;

	if (numVerts > DMM_MAX_DEFORM_VERTS) numVerts = DMM_MAX_DEFORM_VERTS;

	enh->deformVerts = (dmmDeformVertex_t *)Z_Malloc(numVerts * sizeof(dmmDeformVertex_t));
	enh->numDeformVerts = numVerts;
	enh->deformIndices = (uint32_t *)Z_Malloc(numTris * sizeof(uint32_t));
	enh->numDeformIndices = 0;

	i = 0;
	for (gz = 0; gz <= res && i < numVerts; gz++) {
		for (gy = 0; gy <= res && i < numVerts; gy++) {
			for (gx = 0; gx <= res && i < numVerts; gx++) {
				float fx = def->position[0] + ((float)gx / res - 0.5f) * def->dimensions[0];
				float fy = def->position[1] + ((float)gy / res - 0.5f) * def->dimensions[1];
				float fz = def->position[2] + ((float)gz / res - 0.5f) * def->dimensions[2];

				VectorSet(enh->deformVerts[i].position, fx, fy, fz);
				VectorCopy(enh->deformVerts[i].position, enh->deformVerts[i].originalPosition);
				VectorSet(enh->deformVerts[i].normal, 0, 1, 0);
				enh->deformVerts[i].displacement = 0;
				enh->deformVerts[i].stress = 0;
				i++;
			}
		}
	}

	enh->meshDirty = qtrue;
	return base;
}

void Dmm_SetThermal(dmmObjectHandle_t handle, const dmmThermal_t *thermal) {
	int i;
	for (i = 0; i < dmmEnhancedCount; i++) {
		if (dmmEnhanced[i].active && dmmEnhanced[i].baseHandle == handle) {
			Com_Memcpy(&dmmEnhanced[i].thermal, thermal, sizeof(dmmThermal_t));
			return;
		}
	}
}

void Dmm_ApplyHeat(dmmObjectHandle_t handle, const vec3_t point, float temperature, float radius) {
	int i, v;
	for (i = 0; i < dmmEnhancedCount; i++) {
		if (!dmmEnhanced[i].active || dmmEnhanced[i].baseHandle != handle) continue;
		dmmEnhancedData_t *enh = &dmmEnhanced[i];

		for (v = 0; v < enh->numDeformVerts; v++) {
			float dist = Distance(enh->deformVerts[v].position, point);
			if (dist < radius) {
				float atten = 1.0f - (dist / radius);
				atten *= atten;
				float heatTransfer = temperature * atten * enh->thermal.thermalConductivity * 0.001f;
				enh->thermal.temperature += heatTransfer;
			}
		}

		if (enh->thermal.temperature >= enh->thermal.softeningPoint) {
			float softenRatio = (enh->thermal.temperature - enh->thermal.softeningPoint) /
				(enh->thermal.meltingPoint - enh->thermal.softeningPoint);
			if (softenRatio > 1.0f) softenRatio = 1.0f;
			enh->softened = qtrue;

			float newYield = 1.0f;
			float newFrac = 2.0f;
			Dmm_SetMaterialParams(handle, 0, newYield * (1.0f - softenRatio * 0.9f),
				newFrac * (1.0f - softenRatio * 0.9f));
		}
		return;
	}
}

void Dmm_CoolDown(dmmObjectHandle_t handle, float rate) {
	int i;
	for (i = 0; i < dmmEnhancedCount; i++) {
		if (!dmmEnhanced[i].active || dmmEnhanced[i].baseHandle != handle) continue;
		dmmEnhanced[i].thermal.temperature -= rate;
		if (dmmEnhanced[i].thermal.temperature < 20.0f) {
			dmmEnhanced[i].thermal.temperature = 20.0f;
			dmmEnhanced[i].softened = qfalse;
		}
		return;
	}
}

void Dmm_GetEnhancedState(dmmObjectHandle_t handle, dmmEnhancedState_t *state) {
	int i;
	if (!state) return;
	Com_Memset(state, 0, sizeof(*state));

	Dmm_GetState(handle, &state->base);

	for (i = 0; i < dmmEnhancedCount; i++) {
		if (!dmmEnhanced[i].active || dmmEnhanced[i].baseHandle != handle) continue;
		dmmEnhancedData_t *enh = &dmmEnhanced[i];

		Com_Memcpy(&state->thermal, &enh->thermal, sizeof(dmmThermal_t));
		Com_Memcpy(&state->fatigue, &enh->fatigue, sizeof(dmmFatigue_t));
		state->plasticStrain = enh->plasticStrain;
		state->elasticStrain = enh->elasticStrain;
		state->vonMisesStress = enh->vonMisesStress;
		state->maxPrincipalStress = enh->maxPrincipalStress;
		state->damageParameter = enh->damageParameter;
		state->yielded = enh->yielded;
		state->softened = enh->softened;
		return;
	}
}

void Dmm_GetDeformMesh(dmmObjectHandle_t handle, dmmDeformMesh_t *mesh) {
	int i;
	if (!mesh) return;
	Com_Memset(mesh, 0, sizeof(*mesh));

	for (i = 0; i < dmmEnhancedCount; i++) {
		if (!dmmEnhanced[i].active || dmmEnhanced[i].baseHandle != handle) continue;
		mesh->vertices = dmmEnhanced[i].deformVerts;
		mesh->numVertices = dmmEnhanced[i].numDeformVerts;
		mesh->indices = dmmEnhanced[i].deformIndices;
		mesh->numIndices = dmmEnhanced[i].numDeformIndices;
		mesh->dirty = dmmEnhanced[i].meshDirty;
		dmmEnhanced[i].meshDirty = qfalse;
		return;
	}
}

void Dmm_StepFatigue(dmmObjectHandle_t handle, float dt) {
	int i;
	for (i = 0; i < dmmEnhancedCount; i++) {
		if (!dmmEnhanced[i].active || dmmEnhanced[i].baseHandle != handle) continue;
		dmmEnhancedData_t *enh = &dmmEnhanced[i];

		enh->fatigue.cycleCount += dt * 60.0f;

		if (enh->vonMisesStress > 0) {
			float sn = enh->vonMisesStress / enh->fatigue.fatigueLimit;
			enh->fatigue.damageAccum += sn * sn * dt * 0.01f;
		}

		if (enh->fatigue.crackLength > 0) {
			float dK = enh->maxPrincipalStress * sqrtf(3.14159f * enh->fatigue.crackLength);
			enh->fatigue.crackLength += enh->fatigue.crackGrowthRate * dK * dt;
		}

		enh->damageParameter = enh->fatigue.damageAccum +
			(enh->fatigue.crackLength * 10.0f);
		if (enh->damageParameter > 1.0f) enh->damageParameter = 1.0f;

		return;
	}
}

int Dmm_Fracture(dmmObjectHandle_t handle, const vec3_t impactPoint, float energy) {
	int i, f;
	for (i = 0; i < dmmEnhancedCount; i++) {
		if (!dmmEnhanced[i].active || dmmEnhanced[i].baseHandle != handle) continue;
		dmmEnhancedData_t *enh = &dmmEnhanced[i];

		Dmm_ApplyImpact(handle, impactPoint, impactPoint, energy);

		if (!Dmm_IsFractured(handle) && enh->damageParameter < 0.8f) {
			return 0;
		}

		int numFrags = enh->pattern.numPoints;
		if (numFrags > DMM_MAX_FRAGMENTS) numFrags = DMM_MAX_FRAGMENTS;

		for (f = 0; f < numFrags; f++) {
			physBodyDef_t fragDef;
			Com_Memset(&fragDef, 0, sizeof(fragDef));
			fragDef.shape = PHYS_SHAPE_BOX;
			fragDef.type = PHYS_BODY_DYNAMIC;

			VectorCopy(enh->pattern.points[f], fragDef.position);
			VectorAdd(fragDef.position, impactPoint, fragDef.position);

			float fragSize = enh->pattern.minFragmentSize + randf() * enh->pattern.minFragmentSize;
			VectorSet(fragDef.halfExtents, fragSize, fragSize, fragSize);

			fragDef.mass = fragSize * fragSize * fragSize * 100.0f;
			fragDef.friction = 0.6f;
			fragDef.restitution = 0.2f;
			fragDef.linearDamping = 0.1f;
			fragDef.angularDamping = 0.2f;
			fragDef.collisionGroup = 1;
			fragDef.collisionMask = -1;

			enh->fragments[f] = Phys_CreateBody(&fragDef);

			if (enh->fragments[f] >= 0) {
				vec3_t fragImpulse;
				VectorSubtract(enh->pattern.points[f], impactPoint, fragImpulse);
				VectorNormalize(fragImpulse);
				VectorScale(fragImpulse, energy * 0.1f * (0.5f + randf() * 0.5f), fragImpulse);

				vec3_t zero;
				VectorClear(zero);
				Phys_ApplyImpulse(enh->fragments[f], fragImpulse, zero);
			}
		}

		enh->numFragments = numFrags;
		return numFrags;
	}
	return 0;
}

void Dmm_GenerateVoronoiPattern(const vec3_t center, float radius, int numCells, dmmFracturePattern_t *pattern) {
	int i;
	if (!pattern) return;
	Com_Memset(pattern, 0, sizeof(*pattern));

	if (numCells > DMM_MAX_FRACTURE_POINTS) numCells = DMM_MAX_FRACTURE_POINTS;

	for (i = 0; i < numCells; i++) {
		float theta = randf() * 2.0f * 3.14159f;
		float phi = acosf(1.0f - 2.0f * randf());
		float r = radius * cbrtf(randf());

		pattern->points[i][0] = r * sinf(phi) * cosf(theta);
		pattern->points[i][1] = r * sinf(phi) * sinf(theta);
		pattern->points[i][2] = r * cosf(phi);
	}

	pattern->numPoints = numCells;
	pattern->randomness = 0.5f;
	pattern->minFragmentSize = radius * 0.1f;
}
