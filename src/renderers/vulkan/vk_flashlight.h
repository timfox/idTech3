/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Projected texture / flashlight system for Vulkan renderer.
Supports shadow-casting spotlights attached to entities,
volumetric cone rendering, and cookie texture projection.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/tr_types.h"

#define MAX_PROJECTED_LIGHTS 8

typedef struct projectedLight_s {
	qboolean    active;
	vec3_t      origin;
	vec3_t      direction;
	vec3_t      color;
	float       intensity;
	float       coneAngle;
	float       range;
	float       falloffExponent;
	int         ownerEntity;
	qhandle_t   cookieTexture;
	qboolean    castShadows;
	qboolean    volumetric;
	float       volumetricDensity;
	float       shadowBias;
	float       viewMatrix[16];
	float       projMatrix[16];
} projectedLight_t;

void ProjLight_Init(void);
void ProjLight_Shutdown(void);

int  ProjLight_Add(const vec3_t origin, const vec3_t dir, const vec3_t color,
                   float intensity, float coneAngle, float range);
void ProjLight_Remove(int handle);
void ProjLight_Update(int handle, const vec3_t origin, const vec3_t dir);
void ProjLight_SetCookie(int handle, qhandle_t shader);
void ProjLight_SetShadows(int handle, qboolean enabled, float bias);
void ProjLight_SetVolumetric(int handle, qboolean enabled, float density);
void ProjLight_SetOwner(int handle, int entityNum);

int  ProjLight_GetCount(void);
const projectedLight_t *ProjLight_Get(int handle);
void ProjLight_BuildMatrices(int handle);

#ifdef __cplusplus
}
#endif
