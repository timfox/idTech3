/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

HDR skybox system with IBL lighting integration.
Supports multiple panoramic projection formats for environment
maps that drive both skybox rendering and PBR image-based lighting.

Supported formats:
  - Equirectangular (latlong) -- single 2:1 HDR image
  - Cubemap faces -- 6 individual HDR images
  - Vertical/horizontal cross -- cubemap cross layout
  - Spherical (mirror ball) -- single circular HDR image

Pipeline:
  1. Load HDR EXR/PNG panorama
  2. Convert to cubemap (if not already cubemap faces)
  3. Generate prefiltered environment mip chain for specular IBL
  4. Generate irradiance cubemap for diffuse IBL
  5. Both cubemaps feed the PBR lighting pipeline
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/vulkan/vulkan.h"
#include "../common/tr_types.h"

#define SKYBOX_HDR_CUBEMAP_SIZE  512
#define SKYBOX_HDR_IRRADIANCE_SIZE 32
#define SKYBOX_HDR_PREFILTER_SIZE 256
#define SKYBOX_HDR_PREFILTER_MIPS 8

typedef enum {
	SKYBOX_PROJ_EQUIRECTANGULAR,
	SKYBOX_PROJ_CUBEMAP_FACES,
	SKYBOX_PROJ_VERTICAL_CROSS,
	SKYBOX_PROJ_HORIZONTAL_CROSS,
	SKYBOX_PROJ_SPHERICAL_MIRROR,
	SKYBOX_PROJ_COUNT
} skyboxProjection_t;

typedef struct skyboxHDR_s {
	qboolean            loaded;
	skyboxProjection_t  projection;
	char                filename[MAX_QPATH];

	float              *hdrData;
	int                 srcWidth;
	int                 srcHeight;

	float              *cubeFaces[6];
	int                 cubeSize;

	float              *irradianceFaces[6];
	int                 irradianceSize;

	float              *prefilteredFaces[6];
	int                 prefilteredSize;
	int                 prefilteredMips;

	float               exposure;
	float               rotation;
	float               tintR, tintG, tintB;
	float               intensity;
	vec4_t              shCoeffs[9];
	qboolean            hasSHCoeffs;
} skyboxHDR_t;

void SkyboxHDR_Init(void);
void SkyboxHDR_Shutdown(void);
void SkyboxHDR_RegisterCvars(void);

qboolean SkyboxHDR_Load(const char *filename, skyboxProjection_t projection);
qboolean SkyboxHDR_LoadCubeFaces(const char *baseName);
void     SkyboxHDR_Unload(void);

void SkyboxHDR_GenerateCubemap(void);
void SkyboxHDR_GenerateIrradiance(void);
void SkyboxHDR_GeneratePrefiltered(void);

const skyboxHDR_t *SkyboxHDR_Get(void);
qboolean           SkyboxHDR_IsLoaded(void);
void               SkyboxHDR_UpdateRuntime(void);
qboolean           SkyboxHDR_GetCubemapViews(VkImageView *prefilterOut, VkImageView *irradianceOut);
VkDescriptorSet    SkyboxHDR_GetPrefilteredDescriptor(void);
VkDescriptorSet    SkyboxHDR_GetIrradianceDescriptor(void);
qboolean           SkyboxHDR_CopySHCoeffs(vec4_t out[9]);

void SkyboxHDR_SetExposure(float exposure);
void SkyboxHDR_SetRotation(float degrees);
void SkyboxHDR_SetTint(float r, float g, float b);
void SkyboxHDR_SetIntensity(float intensity);

void SkyboxHDR_SampleDirection(const float *dir, float *outRGB);
void SkyboxHDR_SampleIrradiance(const float *normal, float *outRGB);

/* Scene-linear RGBA32F faces for DrawSkyBox into SceneHDR (Quake Z-up outerbox order).
 * Not tone-mapped; values may exceed 1.0. Specular prefilter is IBL-only. */
qboolean SkyboxHDR_BuildDisplayFaces( void );
image_t *SkyboxHDR_GetDisplayFace( int outerboxIndex );
void SkyboxHDR_ClearDisplayFaces( void );

/* Apply worldspawn / map keys: path + optional exposure/rotation/intensity/projection. */
qboolean SkyboxHDR_ConfigureFromMap( const char *path, float exposure, float rotation,
	float intensity, int projection );

/* Enable histogram eye adaptation for HDR sky (r_exposure_auto + metering). */
void SkyboxHDR_EnableEyeAdaptation( void );

#ifdef __cplusplus
}
#endif
