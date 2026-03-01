/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Volumetric fog cvar management and parameter interface.
This module owns all volumetric fog cvars and exposes parameter
accessors used by the GPU dispatch code in vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_vfog.h"

static cvar_t *r_vfog;
static cvar_t *r_vfog_density;
static cvar_t *r_vfog_heightFalloff;
static cvar_t *r_vfog_heightOffset;
static cvar_t *r_vfog_maxDistance;
static cvar_t *r_vfog_scatter;
static cvar_t *r_vfog_anisotropy;
static cvar_t *r_vfog_absorption;
static cvar_t *r_vfog_noiseScale;
static cvar_t *r_vfog_noiseSpeed;
static cvar_t *r_vfog_noiseOctaves;
static cvar_t *r_vfog_windX;
static cvar_t *r_vfog_windY;
static cvar_t *r_vfog_windZ;
static cvar_t *r_vfog_colorR;
static cvar_t *r_vfog_colorG;
static cvar_t *r_vfog_colorB;
static cvar_t *r_vfog_ambient;
static cvar_t *r_vfog_temporal;
static cvar_t *r_vfog_phase;
static cvar_t *r_vfog_sliceDist;
static cvar_t *r_vfog_near;
static cvar_t *r_vfog_far;
static cvar_t *r_vfog_froxelW;
static cvar_t *r_vfog_froxelH;
static cvar_t *r_vfog_froxelD;

/*
===============
VFog_RegisterCvars
===============
*/
void VFog_RegisterCvars(void) {
	r_vfog               = ri.Cvar_Get("r_vfog",               "0",     CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_vfog, "Legacy/unused. Volumetric fog uses r_volumetricFog instead." );
	r_vfog_density       = ri.Cvar_Get("r_vfog_density",       "0.02",  CVAR_ARCHIVE);
	r_vfog_heightFalloff = ri.Cvar_Get("r_vfog_heightFalloff", "0.04",  CVAR_ARCHIVE);
	r_vfog_heightOffset  = ri.Cvar_Get("r_vfog_heightOffset",  "0.0",   CVAR_ARCHIVE);
	r_vfog_maxDistance   = ri.Cvar_Get("r_vfog_maxDistance",    "4000",  CVAR_ARCHIVE);
	r_vfog_scatter       = ri.Cvar_Get("r_vfog_scatter",       "1.0",   CVAR_ARCHIVE);
	r_vfog_anisotropy    = ri.Cvar_Get("r_vfog_anisotropy",    "0.6",   CVAR_ARCHIVE);
	r_vfog_absorption    = ri.Cvar_Get("r_vfog_absorption",    "0.01",  CVAR_ARCHIVE);
	r_vfog_noiseScale    = ri.Cvar_Get("r_vfog_noiseScale",    "0.003", CVAR_ARCHIVE);
	r_vfog_noiseSpeed    = ri.Cvar_Get("r_vfog_noiseSpeed",    "0.5",   CVAR_ARCHIVE);
	r_vfog_noiseOctaves  = ri.Cvar_Get("r_vfog_noiseOctaves",  "4",     CVAR_ARCHIVE);
	r_vfog_windX         = ri.Cvar_Get("r_vfog_windX",         "1.0",   CVAR_ARCHIVE);
	r_vfog_windY         = ri.Cvar_Get("r_vfog_windY",         "0.0",   CVAR_ARCHIVE);
	r_vfog_windZ         = ri.Cvar_Get("r_vfog_windZ",         "0.2",   CVAR_ARCHIVE);
	r_vfog_colorR        = ri.Cvar_Get("r_vfog_colorR",        "0.5",   CVAR_ARCHIVE);
	r_vfog_colorG        = ri.Cvar_Get("r_vfog_colorG",        "0.55",  CVAR_ARCHIVE);
	r_vfog_colorB        = ri.Cvar_Get("r_vfog_colorB",        "0.65",  CVAR_ARCHIVE);
	r_vfog_ambient       = ri.Cvar_Get("r_vfog_ambient",       "0.3",   CVAR_ARCHIVE);
	r_vfog_temporal      = ri.Cvar_Get("r_vfog_temporal",       "0.9",   CVAR_ARCHIVE);
	r_vfog_phase         = ri.Cvar_Get("r_vfog_phase",         "0.8",   CVAR_ARCHIVE);
	r_vfog_sliceDist     = ri.Cvar_Get("r_vfog_sliceDist",     "4.0",   CVAR_ARCHIVE);
	r_vfog_near          = ri.Cvar_Get("r_vfog_near",          "1.0",   CVAR_ARCHIVE);
	r_vfog_far           = ri.Cvar_Get("r_vfog_far",           "4000",  CVAR_ARCHIVE);
	r_vfog_froxelW       = ri.Cvar_Get("r_vfog_froxelW",       "160",   CVAR_ARCHIVE | CVAR_LATCH);
	r_vfog_froxelH       = ri.Cvar_Get("r_vfog_froxelH",       "90",    CVAR_ARCHIVE | CVAR_LATCH);
	r_vfog_froxelD       = ri.Cvar_Get("r_vfog_froxelD",       "64",    CVAR_ARCHIVE | CVAR_LATCH);

	ri.Printf(PRINT_ALL, "Volumetric fog: legacy r_vfog* cvars registered (unused; use r_volumetricFog)\n" );
}

qboolean VFog_IsEnabled(void) {
	return r_vfog && r_vfog->integer > 0;
}

int VFog_GetMode(void) {
	return r_vfog ? r_vfog->integer : 0;
}

float VFog_GetDensity(void) { return r_vfog_density ? r_vfog_density->value : 0.02f; }
float VFog_GetHeightFalloff(void) { return r_vfog_heightFalloff ? r_vfog_heightFalloff->value : 0.04f; }
float VFog_GetHeightOffset(void) { return r_vfog_heightOffset ? r_vfog_heightOffset->value : 0.0f; }
float VFog_GetMaxDistance(void) { return r_vfog_maxDistance ? r_vfog_maxDistance->value : 4000.0f; }
float VFog_GetScatterIntensity(void) { return r_vfog_scatter ? r_vfog_scatter->value : 1.0f; }
float VFog_GetAnisotropy(void) { return r_vfog_anisotropy ? r_vfog_anisotropy->value : 0.6f; }
float VFog_GetAbsorption(void) { return r_vfog_absorption ? r_vfog_absorption->value : 0.01f; }
float VFog_GetNoiseScale(void) { return r_vfog_noiseScale ? r_vfog_noiseScale->value : 0.003f; }
float VFog_GetNoiseSpeed(void) { return r_vfog_noiseSpeed ? r_vfog_noiseSpeed->value : 0.5f; }
float VFog_GetNoiseOctaves(void) { return r_vfog_noiseOctaves ? r_vfog_noiseOctaves->value : 4.0f; }
float VFog_GetAmbientIntensity(void) { return r_vfog_ambient ? r_vfog_ambient->value : 0.3f; }
float VFog_GetTemporalBlend(void) { return r_vfog_temporal ? r_vfog_temporal->value : 0.9f; }
float VFog_GetPhaseG(void) { return r_vfog_phase ? r_vfog_phase->value : 0.8f; }
float VFog_GetSliceDistribution(void) { return r_vfog_sliceDist ? r_vfog_sliceDist->value : 4.0f; }
float VFog_GetNearPlane(void) { return r_vfog_near ? r_vfog_near->value : 1.0f; }
float VFog_GetFarPlane(void) { return r_vfog_far ? r_vfog_far->value : 4000.0f; }

int VFog_GetFroxelWidth(void) { return r_vfog_froxelW ? r_vfog_froxelW->integer : VFOG_FROXEL_WIDTH; }
int VFog_GetFroxelHeight(void) { return r_vfog_froxelH ? r_vfog_froxelH->integer : VFOG_FROXEL_HEIGHT; }
int VFog_GetFroxelDepth(void) { return r_vfog_froxelD ? r_vfog_froxelD->integer : VFOG_FROXEL_DEPTH; }

void VFog_GetWindDirection(float *x, float *y, float *z) {
	if (x) *x = r_vfog_windX ? r_vfog_windX->value : 1.0f;
	if (y) *y = r_vfog_windY ? r_vfog_windY->value : 0.0f;
	if (z) *z = r_vfog_windZ ? r_vfog_windZ->value : 0.2f;
}

void VFog_GetFogColor(float *r, float *g, float *b) {
	if (r) *r = r_vfog_colorR ? r_vfog_colorR->value : 0.5f;
	if (g) *g = r_vfog_colorG ? r_vfog_colorG->value : 0.55f;
	if (b) *b = r_vfog_colorB ? r_vfog_colorB->value : 0.65f;
}
