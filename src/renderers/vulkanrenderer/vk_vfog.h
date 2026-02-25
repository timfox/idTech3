/*
===========================================================================
Copyright (C) 2026 id Tech 3 Contributors

Volumetric fog system for Vulkan renderer.
Implements three phases:
  Phase 1: Analytical height fog (exponential height + distance fog)
  Phase 2: Ray-marched volumetric fog with noise-driven density
  Phase 3: Froxel-based volumetric lighting with compute scatter
===========================================================================
*/

#pragma once

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VFOG_FROXEL_WIDTH   160
#define VFOG_FROXEL_HEIGHT   90
#define VFOG_FROXEL_DEPTH    64

#define VFOG_RAY_STEPS       48

typedef struct vfogParams_s {
	float   density;
	float   heightFalloff;
	float   heightOffset;
	float   maxDistance;

	float   scatterIntensity;
	float   scatterAnisotropy;
	float   absorptionCoeff;
	float   noiseScale;

	float   noiseSpeed;
	float   windDirX;
	float   windDirY;
	float   windDirZ;

	float   fogColorR;
	float   fogColorG;
	float   fogColorB;
	float   ambientIntensity;

	float   temporalBlend;
	float   lightScatterStrength;
	float   phaseG;
	float   enabled;
} vfogParams_t;

typedef struct vfogPushConstants_s {
	float viewOrigin[4];
	float viewForward[4];
	float viewRight[4];
	float viewUp[4];

	float invProjection[16];

	float fogParams[4];
	float fogColor[4];
	float noiseParams[4];
	float windParams[4];
	float scatterParams[4];

	float time;
	float nearPlane;
	float farPlane;
	float padding;
} vfogPushConstants_t;

void VFog_Init(void);
void VFog_Shutdown(void);
void VFog_RegisterCvars(void);
void VFog_UpdateParams(vfogParams_t *params);
qboolean VFog_IsEnabled(void);

void VFog_RenderRayMarch(void);
void VFog_ComputeFroxels(void);
void VFog_ApplyFog(void);

#ifdef __cplusplus
}
#endif
