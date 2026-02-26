/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Post-processing effects module for Vulkan renderer.
Manages SSR, atmospheric scattering, vegetation wind compute,
and color grading pipeline integration.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void PostFX_RegisterCvars(void);

qboolean PostFX_SSR_IsEnabled(void);
float    PostFX_SSR_GetMaxDistance(void);
float    PostFX_SSR_GetStepSize(void);
float    PostFX_SSR_GetThickness(void);
float    PostFX_SSR_GetFadeEdge(void);
float    PostFX_SSR_GetRoughnessThreshold(void);
float    PostFX_SSR_GetIntensity(void);

qboolean PostFX_Atmosphere_IsEnabled(void);
void     PostFX_Atmosphere_GetSunDirection(float *x, float *y, float *z);
float    PostFX_Atmosphere_GetSunIntensity(void);
float    PostFX_Atmosphere_GetRayleighHeight(void);
float    PostFX_Atmosphere_GetMieHeight(void);
float    PostFX_Atmosphere_GetMieG(void);

qboolean PostFX_VegWind_IsEnabled(void);
float    PostFX_VegWind_GetPrimaryFreq(void);
float    PostFX_VegWind_GetPrimaryAmp(void);
float    PostFX_VegWind_GetDetailFreq(void);
float    PostFX_VegWind_GetDetailAmp(void);
float    PostFX_VegWind_GetGustFreq(void);
float    PostFX_VegWind_GetGustAmp(void);
void     PostFX_VegWind_GetWindDir(float *x, float *y, float *z);
float    PostFX_VegWind_GetWindStrength(void);

qboolean PostFX_NeedsPipelineUpdate(void);
float    PostFX_GetVignetteIntensity(void);
float    PostFX_GetVignetteRadius(void);
float    PostFX_GetChromaticAberration(void);
float    PostFX_GetFilmGrain(void);

#ifdef __cplusplus
}
#endif
