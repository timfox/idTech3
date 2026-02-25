/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Projected texture / flashlight implementation.
Manages projected spotlights with shadow and volumetric support.
===========================================================================
*/

#include "tr_local.h"
#include "vk_flashlight.h"
#include <math.h>

static projectedLight_t projLights[MAX_PROJECTED_LIGHTS];
static int projLightCount = 0;

void ProjLight_Init(void) {
	Com_Memset(projLights, 0, sizeof(projLights));
	projLightCount = 0;
	ri.Printf(PRINT_ALL, "Projected light system initialized (%d max)\n", MAX_PROJECTED_LIGHTS);
}

void ProjLight_Shutdown(void) {
	Com_Memset(projLights, 0, sizeof(projLights));
	projLightCount = 0;
}

int ProjLight_Add(const vec3_t origin, const vec3_t dir, const vec3_t color,
                  float intensity, float coneAngle, float range) {
	int i;
	for (i = 0; i < MAX_PROJECTED_LIGHTS; i++) {
		if (!projLights[i].active) {
			projectedLight_t *pl = &projLights[i];
			pl->active = qtrue;
			VectorCopy(origin, pl->origin);
			VectorCopy(dir, pl->direction);
			VectorNormalize(pl->direction);
			VectorCopy(color, pl->color);
			pl->intensity = intensity;
			pl->coneAngle = coneAngle;
			pl->range = range;
			pl->falloffExponent = 2.0f;
			pl->castShadows = qfalse;
			pl->volumetric = qfalse;
			pl->volumetricDensity = 0.1f;
			pl->shadowBias = 0.005f;
			pl->ownerEntity = -1;
			pl->cookieTexture = 0;
			projLightCount++;
			ProjLight_BuildMatrices(i);
			return i;
		}
	}
	return -1;
}

void ProjLight_Remove(int h) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS || !projLights[h].active) return;
	projLights[h].active = qfalse;
	projLightCount--;
}

void ProjLight_Update(int h, const vec3_t origin, const vec3_t dir) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS || !projLights[h].active) return;
	VectorCopy(origin, projLights[h].origin);
	VectorCopy(dir, projLights[h].direction);
	VectorNormalize(projLights[h].direction);
	ProjLight_BuildMatrices(h);
}

void ProjLight_SetCookie(int h, qhandle_t shader) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS) return;
	projLights[h].cookieTexture = shader;
}

void ProjLight_SetShadows(int h, qboolean enabled, float bias) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS) return;
	projLights[h].castShadows = enabled;
	projLights[h].shadowBias = bias;
}

void ProjLight_SetVolumetric(int h, qboolean enabled, float density) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS) return;
	projLights[h].volumetric = enabled;
	projLights[h].volumetricDensity = density;
}

void ProjLight_SetOwner(int h, int entityNum) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS) return;
	projLights[h].ownerEntity = entityNum;
}

int ProjLight_GetCount(void) { return projLightCount; }

const projectedLight_t *ProjLight_Get(int h) {
	if (h < 0 || h >= MAX_PROJECTED_LIGHTS || !projLights[h].active) return NULL;
	return &projLights[h];
}

void ProjLight_BuildMatrices(int h) {
	projectedLight_t *pl;
	vec3_t up, right, forward;
	float halfAngle, nearP, farP;
	int i;

	if (h < 0 || h >= MAX_PROJECTED_LIGHTS || !projLights[h].active) return;
	pl = &projLights[h];

	VectorCopy(pl->direction, forward);
	if (fabsf(forward[2]) < 0.999f) {
		vec3_t worldUp = {0, 0, 1};
		CrossProduct(worldUp, forward, right);
		VectorNormalize(right);
		CrossProduct(forward, right, up);
	} else {
		vec3_t worldRight = {1, 0, 0};
		CrossProduct(forward, worldRight, up);
		VectorNormalize(up);
		CrossProduct(up, forward, right);
	}

	pl->viewMatrix[0] = right[0];   pl->viewMatrix[4] = right[1];   pl->viewMatrix[8]  = right[2];
	pl->viewMatrix[1] = up[0];      pl->viewMatrix[5] = up[1];      pl->viewMatrix[9]  = up[2];
	pl->viewMatrix[2] = forward[0]; pl->viewMatrix[6] = forward[1]; pl->viewMatrix[10] = forward[2];
	pl->viewMatrix[3] = 0; pl->viewMatrix[7] = 0; pl->viewMatrix[11] = 0;
	pl->viewMatrix[12] = -DotProduct(right, pl->origin);
	pl->viewMatrix[13] = -DotProduct(up, pl->origin);
	pl->viewMatrix[14] = -DotProduct(forward, pl->origin);
	pl->viewMatrix[15] = 1.0f;

	halfAngle = pl->coneAngle * 0.5f * (3.14159265f / 180.0f);
	nearP = 1.0f;
	farP = pl->range;

	float f = 1.0f / tanf(halfAngle);
	Com_Memset(pl->projMatrix, 0, sizeof(pl->projMatrix));
	pl->projMatrix[0] = f;
	pl->projMatrix[5] = f;
	pl->projMatrix[10] = farP / (farP - nearP);
	pl->projMatrix[11] = 1.0f;
	pl->projMatrix[14] = -(nearP * farP) / (farP - nearP);

	(void)i;
}
