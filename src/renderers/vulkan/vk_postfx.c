/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Post-processing effects cvar management and pipeline integration.
Owns cvars for SSR, atmospheric scattering, vegetation wind,
and color grading lens effects.
===========================================================================
*/

#include "tr_local.h"
#include "vk_postfx.h"

static cvar_t *r_ssr;
static cvar_t *r_ssr_maxDistance;
static cvar_t *r_ssr_stepSize;
static cvar_t *r_ssr_thickness;
static cvar_t *r_ssr_fadeEdge;
static cvar_t *r_ssr_roughnessThreshold;
static cvar_t *r_ssr_intensity;

static cvar_t *r_atmosphere;
static cvar_t *r_atmosphere_sunDirX;
static cvar_t *r_atmosphere_sunDirY;
static cvar_t *r_atmosphere_sunDirZ;
static cvar_t *r_atmosphere_sunIntensity;
static cvar_t *r_atmosphere_rayleighHeight;
static cvar_t *r_atmosphere_mieHeight;
static cvar_t *r_atmosphere_mieG;

static cvar_t *r_vegWind;
static cvar_t *r_vegWind_primaryFreq;
static cvar_t *r_vegWind_primaryAmp;
static cvar_t *r_vegWind_detailFreq;
static cvar_t *r_vegWind_detailAmp;
static cvar_t *r_vegWind_gustFreq;
static cvar_t *r_vegWind_gustAmp;
static cvar_t *r_vegWind_dirX;
static cvar_t *r_vegWind_dirY;
static cvar_t *r_vegWind_dirZ;
static cvar_t *r_vegWind_strength;

static cvar_t *r_vignette;
static cvar_t *r_vignette_radius;
static cvar_t *r_chromaticAberration;
static cvar_t *r_filmGrain;
static cvar_t *r_filmLook;

/*
===============
PostFX_RegisterCvars
===============
*/
void PostFX_RegisterCvars(void) {
	r_ssr                    = ri.Cvar_Get("r_ssr",                    "0",    CVAR_ARCHIVE);
	r_ssr_maxDistance        = ri.Cvar_Get("r_ssr_maxDistance",        "100",  CVAR_ARCHIVE);
	r_ssr_stepSize           = ri.Cvar_Get("r_ssr_stepSize",           "1.0",  CVAR_ARCHIVE);
	r_ssr_thickness          = ri.Cvar_Get("r_ssr_thickness",          "0.5",  CVAR_ARCHIVE);
	r_ssr_fadeEdge           = ri.Cvar_Get("r_ssr_fadeEdge",           "0.2",  CVAR_ARCHIVE);
	r_ssr_roughnessThreshold = ri.Cvar_Get("r_ssr_roughnessThreshold", "0.5",  CVAR_ARCHIVE);
	r_ssr_intensity          = ri.Cvar_Get("r_ssr_intensity",          "0.8",  CVAR_ARCHIVE);

	r_atmosphere             = ri.Cvar_Get("r_atmosphere",             "0",    CVAR_ARCHIVE);
	r_atmosphere_sunDirX     = ri.Cvar_Get("r_atmosphere_sunDirX",     "0.3",  CVAR_ARCHIVE);
	r_atmosphere_sunDirY     = ri.Cvar_Get("r_atmosphere_sunDirY",     "0.8",  CVAR_ARCHIVE);
	r_atmosphere_sunDirZ     = ri.Cvar_Get("r_atmosphere_sunDirZ",     "0.5",  CVAR_ARCHIVE);
	r_atmosphere_sunIntensity = ri.Cvar_Get("r_atmosphere_sunIntensity", "22.0", CVAR_ARCHIVE);
	r_atmosphere_rayleighHeight = ri.Cvar_Get("r_atmosphere_rayleighHeight", "8000", CVAR_ARCHIVE);
	r_atmosphere_mieHeight   = ri.Cvar_Get("r_atmosphere_mieHeight",   "1200", CVAR_ARCHIVE);
	r_atmosphere_mieG        = ri.Cvar_Get("r_atmosphere_mieG",        "0.76", CVAR_ARCHIVE);

	r_vegWind                = ri.Cvar_Get("r_vegWind",                "0",    CVAR_ARCHIVE);
	r_vegWind_primaryFreq    = ri.Cvar_Get("r_vegWind_primaryFreq",    "1.2",  CVAR_ARCHIVE);
	r_vegWind_primaryAmp     = ri.Cvar_Get("r_vegWind_primaryAmp",     "0.06", CVAR_ARCHIVE);
	r_vegWind_detailFreq     = ri.Cvar_Get("r_vegWind_detailFreq",     "0.15", CVAR_ARCHIVE);
	r_vegWind_detailAmp      = ri.Cvar_Get("r_vegWind_detailAmp",      "0.02", CVAR_ARCHIVE);
	r_vegWind_gustFreq       = ri.Cvar_Get("r_vegWind_gustFreq",       "0.3",  CVAR_ARCHIVE);
	r_vegWind_gustAmp        = ri.Cvar_Get("r_vegWind_gustAmp",        "0.15", CVAR_ARCHIVE);
	r_vegWind_dirX           = ri.Cvar_Get("r_vegWind_dirX",           "1.0",  CVAR_ARCHIVE);
	r_vegWind_dirY           = ri.Cvar_Get("r_vegWind_dirY",           "0.0",  CVAR_ARCHIVE);
	r_vegWind_dirZ           = ri.Cvar_Get("r_vegWind_dirZ",           "0.3",  CVAR_ARCHIVE);
	r_vegWind_strength       = ri.Cvar_Get("r_vegWind_strength",       "1.0",  CVAR_ARCHIVE);

	r_vignette               = ri.Cvar_Get("r_vignette",               "0.55", CVAR_ARCHIVE);
	r_vignette_radius        = ri.Cvar_Get("r_vignette_radius",        "0.60", CVAR_ARCHIVE);
	r_chromaticAberration    = ri.Cvar_Get("r_chromaticAberration",    "0.22", CVAR_ARCHIVE);
	r_filmGrain              = ri.Cvar_Get("r_filmGrain",              "0.75", CVAR_ARCHIVE);
	r_filmLook               = ri.Cvar_Get("r_filmLook",               "0",    CVAR_ARCHIVE);

	ri.Cvar_SetDescription( r_vignette, "Vignette strength for post-processing lens darkening." );
	ri.Cvar_SetDescription( r_vignette_radius, "Vignette inner radius before edge darkening starts." );
	ri.Cvar_SetDescription( r_chromaticAberration, "Chromatic aberration strength for lens separation." );
	ri.Cvar_SetDescription( r_filmGrain, "Film grain intensity for post-process pass." );
	ri.Cvar_SetDescription( r_filmLook, "Source Engine–style film grain: luminance-dependent, fine-grained, soft-light blend (DoD:S, L4D quality)." );

	ri.Printf(PRINT_ALL, "PostFX: cvars registered (SSR %s, Atmosphere %s, VegWind %s)\n",
		r_ssr->integer ? "on" : "off",
		r_atmosphere->integer ? "on" : "off",
		r_vegWind->integer ? "on" : "off");
}

qboolean PostFX_SSR_IsEnabled(void) { return r_ssr && r_ssr->integer > 0; }
float PostFX_SSR_GetMaxDistance(void) { return r_ssr_maxDistance ? r_ssr_maxDistance->value : 100.0f; }
float PostFX_SSR_GetStepSize(void) { return r_ssr_stepSize ? r_ssr_stepSize->value : 1.0f; }
float PostFX_SSR_GetThickness(void) { return r_ssr_thickness ? r_ssr_thickness->value : 0.5f; }
float PostFX_SSR_GetFadeEdge(void) { return r_ssr_fadeEdge ? r_ssr_fadeEdge->value : 0.2f; }
float PostFX_SSR_GetRoughnessThreshold(void) { return r_ssr_roughnessThreshold ? r_ssr_roughnessThreshold->value : 0.5f; }
float PostFX_SSR_GetIntensity(void) { return r_ssr_intensity ? r_ssr_intensity->value : 0.8f; }

qboolean PostFX_Atmosphere_IsEnabled(void) { return r_atmosphere && r_atmosphere->integer > 0; }
void PostFX_Atmosphere_GetSunDirection(float *x, float *y, float *z) {
	if (x) *x = r_atmosphere_sunDirX ? r_atmosphere_sunDirX->value : 0.3f;
	if (y) *y = r_atmosphere_sunDirY ? r_atmosphere_sunDirY->value : 0.8f;
	if (z) *z = r_atmosphere_sunDirZ ? r_atmosphere_sunDirZ->value : 0.5f;
}
float PostFX_Atmosphere_GetSunIntensity(void) { return r_atmosphere_sunIntensity ? r_atmosphere_sunIntensity->value : 22.0f; }
float PostFX_Atmosphere_GetRayleighHeight(void) { return r_atmosphere_rayleighHeight ? r_atmosphere_rayleighHeight->value : 8000.0f; }
float PostFX_Atmosphere_GetMieHeight(void) { return r_atmosphere_mieHeight ? r_atmosphere_mieHeight->value : 1200.0f; }
float PostFX_Atmosphere_GetMieG(void) { return r_atmosphere_mieG ? r_atmosphere_mieG->value : 0.76f; }

qboolean PostFX_VegWind_IsEnabled(void) { return r_vegWind && r_vegWind->integer > 0; }
float PostFX_VegWind_GetPrimaryFreq(void) { return r_vegWind_primaryFreq ? r_vegWind_primaryFreq->value : 1.2f; }
float PostFX_VegWind_GetPrimaryAmp(void) { return r_vegWind_primaryAmp ? r_vegWind_primaryAmp->value : 0.06f; }
float PostFX_VegWind_GetDetailFreq(void) { return r_vegWind_detailFreq ? r_vegWind_detailFreq->value : 0.15f; }
float PostFX_VegWind_GetDetailAmp(void) { return r_vegWind_detailAmp ? r_vegWind_detailAmp->value : 0.02f; }
float PostFX_VegWind_GetGustFreq(void) { return r_vegWind_gustFreq ? r_vegWind_gustFreq->value : 0.3f; }
float PostFX_VegWind_GetGustAmp(void) { return r_vegWind_gustAmp ? r_vegWind_gustAmp->value : 0.15f; }
void PostFX_VegWind_GetWindDir(float *x, float *y, float *z) {
	if (x) *x = r_vegWind_dirX ? r_vegWind_dirX->value : 1.0f;
	if (y) *y = r_vegWind_dirY ? r_vegWind_dirY->value : 0.0f;
	if (z) *z = r_vegWind_dirZ ? r_vegWind_dirZ->value : 0.3f;
}
float PostFX_VegWind_GetWindStrength(void) { return r_vegWind_strength ? r_vegWind_strength->value : 1.0f; }

static float lastVignette = 0, lastVigRadius = 0, lastChromAb = 0, lastGrain = 0;
static int lastFilmLook = 0;

qboolean PostFX_NeedsPipelineUpdate(void) {
	float v = r_vignette ? r_vignette->value : 0;
	float vr = r_vignette_radius ? r_vignette_radius->value : 0;
	float c = r_chromaticAberration ? r_chromaticAberration->value : 0;
	float g = r_filmGrain ? r_filmGrain->value : 0;
	int filmLook = ( r_filmLook && r_filmLook->integer ) ? 1 : 0;
	if (v != lastVignette || vr != lastVigRadius || c != lastChromAb || g != lastGrain || filmLook != lastFilmLook) {
		lastVignette = v;
		lastVigRadius = vr;
		lastChromAb = c;
		lastGrain = g;
		lastFilmLook = filmLook;
		return qtrue;
	}
	return qfalse;
}

float PostFX_GetVignetteIntensity(void) { return r_vignette ? r_vignette->value : 0.0f; }
float PostFX_GetVignetteRadius(void) { return r_vignette_radius ? r_vignette_radius->value : 0.75f; }
float PostFX_GetChromaticAberration(void) { return r_chromaticAberration ? r_chromaticAberration->value : 0.0f; }
float PostFX_GetFilmGrain(void) { return r_filmGrain ? r_filmGrain->value : 0.0f; }
int PostFX_GetFilmLook(void) { return ( r_filmLook && r_filmLook->integer ) ? 1 : 0; }
