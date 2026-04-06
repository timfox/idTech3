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
#include "tr_local.h"

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
float    PostFX_SSR_GetMaxDepthGradient(void);

qboolean PostFX_Atmosphere_IsEnabled(void);
void     PostFX_Atmosphere_GetSunDirection(float *x, float *y, float *z);
float    PostFX_Atmosphere_GetSunIntensity(void);
float    PostFX_Atmosphere_GetScale(void);
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
qboolean PostFX_PostPipelinesNeedUpdate(void);
float    PostFX_GetVignetteIntensity(void);
float    PostFX_GetVignetteRadius(void);
float    PostFX_GetChromaticAberration(void);
float    PostFX_GetFilmGrain(void);
int      PostFX_GetFilmLook(void);
qboolean PostFX_MotionBlur_IsEnabled(void);
float    PostFX_MotionBlur_GetStrength(void);
float    PostFX_MotionBlur_GetMaxRadius(void);
int      PostFX_MotionBlur_GetSamples(void);
qboolean PostFX_DepthOfField_IsEnabled(void);
float    PostFX_DepthOfField_GetFocusDistance(void);
float    PostFX_DepthOfField_GetFocusRange(void);
float    PostFX_DepthOfField_GetAperture(void);
float    PostFX_DepthOfField_GetMaxBlur(void);
float    PostFX_GetSharpen(void);
float    PostFX_GetGradeToe(void);
float    PostFX_GetGradeShoulder(void);
float    PostFX_GetGradeWhitePoint(void);
float    PostFX_GetGradeBlackClip(void);
float    PostFX_GetGradeHighlightDesat(void);
float    PostFX_GetGradeTemperature(void);
float    PostFX_GetGradeTint(void);
float    PostFX_GetGradeExposureBias(void);
float    PostFX_GetGradeContrast(void);
float    PostFX_GetGradeContrastPivot(void);
float    PostFX_GetGradeSaturation(void);
float    PostFX_GetGradeVibrance(void);
void     PostFX_GetShadowLift(float *rgb);
void     PostFX_GetMidGamma(float *rgb);
void     PostFX_GetHighlightGain(float *rgb);
void     PostFX_GetSplitShadow(float *rgb);
void     PostFX_GetSplitHighlight(float *rgb);
float    PostFX_GetSplitBalance(void);
float    PostFX_GetSplitStrength(void);
float    PostFX_GetLUTIntensity(void);
image_t *PostFX_GetLUTImage(void);

#ifdef __cplusplus
}
#endif
