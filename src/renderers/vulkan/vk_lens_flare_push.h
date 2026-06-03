/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Push constants for lens_flare.frag (must match GLSL layout).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	float sunPos[2];
	float screenSize[2];
	float f1Strength;
	float f2Strength;
	float f3Strength;
	float lensFlareStrength;
	float sunVisible;
	float tint[3];
	float pad;
} VkLensFlarePushConstants;

#ifdef __cplusplus
}
#endif
