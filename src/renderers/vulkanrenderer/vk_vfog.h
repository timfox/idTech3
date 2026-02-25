/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Volumetric fog public API for Vulkan renderer.
Manages cvars, parameter structures, and public interface for the
volumetric fog system. GPU pipeline dispatch is handled by vk.c
which calls into this module for parameter data.
===========================================================================
*/

#pragma once

#include "../rendercommon/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VFOG_FROXEL_WIDTH   160
#define VFOG_FROXEL_HEIGHT   90
#define VFOG_FROXEL_DEPTH    64

void     VFog_RegisterCvars(void);
qboolean VFog_IsEnabled(void);
int      VFog_GetMode(void);

float VFog_GetDensity(void);
float VFog_GetHeightFalloff(void);
float VFog_GetHeightOffset(void);
float VFog_GetMaxDistance(void);
float VFog_GetScatterIntensity(void);
float VFog_GetAnisotropy(void);
float VFog_GetAbsorption(void);
float VFog_GetNoiseScale(void);
float VFog_GetNoiseSpeed(void);
float VFog_GetNoiseOctaves(void);
float VFog_GetAmbientIntensity(void);
float VFog_GetTemporalBlend(void);
float VFog_GetPhaseG(void);
void  VFog_GetWindDirection(float *x, float *y, float *z);
void  VFog_GetFogColor(float *r, float *g, float *b);
float VFog_GetSliceDistribution(void);

int   VFog_GetFroxelWidth(void);
int   VFog_GetFroxelHeight(void);
int   VFog_GetFroxelDepth(void);
float VFog_GetNearPlane(void);
float VFog_GetFarPlane(void);

#ifdef __cplusplus
}
#endif
