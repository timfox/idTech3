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
#include "vk.h"
#include "vk_postfx.h"
#include "vk_util.h"

static cvar_t *r_ssr;
static cvar_t *r_ssr_maxDistance;
static cvar_t *r_ssr_stepSize;
static cvar_t *r_ssr_thickness;
static cvar_t *r_ssr_fadeEdge;
static cvar_t *r_ssr_roughnessThreshold;
static cvar_t *r_ssr_fresnelExponent;
static cvar_t *r_ssr_intensity;
static cvar_t *r_ssr_maxDepthGradient;

static cvar_t *r_atmosphere;
static cvar_t *r_atmosphere_sunDirX;
static cvar_t *r_atmosphere_sunDirY;
static cvar_t *r_atmosphere_sunDirZ;
static cvar_t *r_atmosphere_sunIntensity;
static cvar_t *r_atmosphere_scale;
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
static cvar_t *r_motionBlur;
static cvar_t *r_motionBlurStrength;
static cvar_t *r_motionBlurMaxRadius;
static cvar_t *r_motionBlurSamples;
static cvar_t *r_depthOfField;
static cvar_t *r_dofFocusDistance;
static cvar_t *r_dofFocusRange;
static cvar_t *r_dofAperture;
static cvar_t *r_dofMaxBlur;
static cvar_t *r_sharpen;
static cvar_t *r_grade_toe;
static cvar_t *r_grade_shoulder;
static cvar_t *r_grade_whitePoint;
static cvar_t *r_grade_blackClip;
static cvar_t *r_grade_highlightDesat;
static cvar_t *r_grade_temperature;
static cvar_t *r_grade_tint;
static cvar_t *r_grade_exposureBias;
static cvar_t *r_grade_contrast;
static cvar_t *r_grade_contrastPivot;
static cvar_t *r_grade_saturation;
static cvar_t *r_grade_vibrance;
static cvar_t *r_grade_hue;
static cvar_t *r_grade_shadowLift;
static cvar_t *r_grade_midGamma;
static cvar_t *r_grade_highlightGain;
static cvar_t *r_grade_splitShadow;
static cvar_t *r_grade_splitHighlight;
static cvar_t *r_grade_splitBalance;
static cvar_t *r_grade_splitStrength;
static cvar_t *r_grade_lut;
static cvar_t *r_grade_lutIntensity;

static image_t *s_postfx_lut_image;
static char s_postfx_lut_path[MAX_QPATH];

static void PostFX_ParseRGBOrDefault( const cvar_t *cvar, vec3_t out, float defR, float defG, float defB )
{
	if ( cvar && cvar->string && vk_parse_rgb_string( cvar->string, out ) ) {
		return;
	}

	out[0] = defR;
	out[1] = defG;
	out[2] = defB;
}

static void PostFX_UpdateLUTImage( void )
{
	const char *path = ( r_grade_lut && r_grade_lut->string ) ? r_grade_lut->string : "";

	if ( !Q_stricmp( s_postfx_lut_path, path ) ) {
		return;
	}

	Q_strncpyz( s_postfx_lut_path, path, sizeof( s_postfx_lut_path ) );
	s_postfx_lut_image = tr.whiteImage;

	if ( !path[0] ) {
		return;
	}

	s_postfx_lut_image = R_FindImageFile( path, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOSCALE, 0 );
	if ( s_postfx_lut_image == NULL ) {
		ri.Printf( PRINT_WARNING, "PostFX: failed to load LUT image '%s', using neutral LUT\n", path );
		s_postfx_lut_image = tr.whiteImage;
	}
}

/*
===============
PostFX_RegisterCvars
===============
*/
void PostFX_RegisterCvars(void) {
	r_ssr                    = ri.Cvar_Get("r_ssr",                    "1",    CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_ssr, "Screen-space reflections. Requires r_fbo 1." );
	r_ssr_maxDistance        = ri.Cvar_Get("r_ssr_maxDistance",        "100",  CVAR_ARCHIVE);
	r_ssr_stepSize           = ri.Cvar_Get("r_ssr_stepSize",           "1.0",  CVAR_ARCHIVE);
	r_ssr_thickness          = ri.Cvar_Get("r_ssr_thickness",          "0.5",  CVAR_ARCHIVE);
	r_ssr_fadeEdge           = ri.Cvar_Get("r_ssr_fadeEdge",           "0.2",  CVAR_ARCHIVE);
	r_ssr_roughnessThreshold = ri.Cvar_Get("r_ssr_roughnessThreshold", "0",  CVAR_ARCHIVE);
	r_ssr_intensity          = ri.Cvar_Get("r_ssr_intensity",          "0.8",  CVAR_ARCHIVE);
	r_ssr_maxDepthGradient   = ri.Cvar_Get("r_ssr_maxDepthGradient",   "0.08", CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_ssr_maxDepthGradient, "Skip SSR at depth edges (object silhouettes, horizon) to reduce thin line artifacts. Lower = stricter." );
	ri.Cvar_SetDescription( r_ssr_roughnessThreshold,
		"SSR view-dependent blend (no roughness buffer): 0=full intensity at all angles (legacy). "
		"1=strongest at grazing angles only (Fresnel-style falloff). Intermediate values blend." );
	ri.Cvar_CheckRange( r_ssr_roughnessThreshold, "0", "1", CV_FLOAT );

	r_ssr_fresnelExponent = ri.Cvar_Get( "r_ssr_fresnelExponent", "2.5", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_ssr_fresnelExponent, "0.5", "8.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssr_fresnelExponent,
		"When r_ssr_roughnessThreshold > 0: exponent on (1 - N·V) for grazing SSR weight. Higher = narrower grazing band." );

	r_atmosphere             = ri.Cvar_Get("r_atmosphere",             "0",    CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_atmosphere, "Atmospheric scattering for sky and fog. Requires r_fbo 1." );
	r_atmosphere_sunDirX     = ri.Cvar_Get("r_atmosphere_sunDirX",     "0.3",  CVAR_ARCHIVE);
	r_atmosphere_sunDirY     = ri.Cvar_Get("r_atmosphere_sunDirY",     "0.8",  CVAR_ARCHIVE);
	r_atmosphere_sunDirZ     = ri.Cvar_Get("r_atmosphere_sunDirZ",     "0.5",  CVAR_ARCHIVE);
	r_atmosphere_sunIntensity = ri.Cvar_Get("r_atmosphere_sunIntensity", "22.0", CVAR_ARCHIVE);
	r_atmosphere_scale = ri.Cvar_Get("r_atmosphere_scale", "4.0", CVAR_ARCHIVE);
	r_atmosphere_rayleighHeight = ri.Cvar_Get("r_atmosphere_rayleighHeight", "8000", CVAR_ARCHIVE);
	r_atmosphere_mieHeight   = ri.Cvar_Get("r_atmosphere_mieHeight",   "1200", CVAR_ARCHIVE);
	r_atmosphere_mieG        = ri.Cvar_Get("r_atmosphere_mieG",        "0.76", CVAR_ARCHIVE);

	r_vegWind                = ri.Cvar_Get("r_vegWind",                "0",    CVAR_ARCHIVE);
	ri.Cvar_SetDescription( r_vegWind, "Vegetation wind animation for foliage shaders." );
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

	r_vignette               = ri.Cvar_Get("r_vignette",               "0.0",  CVAR_ARCHIVE);
	r_vignette_radius        = ri.Cvar_Get("r_vignette_radius",        "0.60", CVAR_ARCHIVE);
	r_chromaticAberration    = ri.Cvar_Get("r_chromaticAberration",    "0.0",  CVAR_ARCHIVE);
	r_filmGrain              = ri.Cvar_Get("r_filmGrain",              "0.0",  CVAR_ARCHIVE);
	r_filmLook               = ri.Cvar_Get("r_filmLook",               "0",    CVAR_ARCHIVE);
	r_motionBlur             = ri.Cvar_Get("r_motionBlur",             "0",    CVAR_ARCHIVE_ND);
	r_motionBlurStrength     = ri.Cvar_Get("r_motionBlurStrength",     "1.0",  CVAR_ARCHIVE_ND);
	r_motionBlurMaxRadius    = ri.Cvar_Get("r_motionBlurMaxRadius",    "24.0", CVAR_ARCHIVE_ND);
	r_motionBlurSamples      = ri.Cvar_Get("r_motionBlurSamples",      "12",   CVAR_ARCHIVE_ND);
	r_depthOfField           = ri.Cvar_Get("r_depthOfField",           "0",    CVAR_ARCHIVE_ND);
	r_dofFocusDistance       = ri.Cvar_Get("r_dofFocusDistance",       "768.0", CVAR_ARCHIVE_ND);
	r_dofFocusRange          = ri.Cvar_Get("r_dofFocusRange",          "192.0", CVAR_ARCHIVE_ND);
	r_dofAperture            = ri.Cvar_Get("r_dofAperture",            "1.4",  CVAR_ARCHIVE_ND);
	r_dofMaxBlur             = ri.Cvar_Get("r_dofMaxBlur",             "18.0", CVAR_ARCHIVE_ND);
	r_sharpen                = ri.Cvar_Get("r_sharpen",                "0.0",  CVAR_ARCHIVE);
	r_grade_toe              = ri.Cvar_Get("r_grade_toe",              "0.35", CVAR_ARCHIVE_ND);
	r_grade_shoulder         = ri.Cvar_Get("r_grade_shoulder",         "0.22", CVAR_ARCHIVE_ND);
	r_grade_whitePoint       = ri.Cvar_Get("r_grade_whitePoint",       "6.5",  CVAR_ARCHIVE_ND);
	r_grade_blackClip        = ri.Cvar_Get("r_grade_blackClip",        "0.0",  CVAR_ARCHIVE_ND);
	r_grade_highlightDesat   = ri.Cvar_Get("r_grade_highlightDesat",   "0.18", CVAR_ARCHIVE_ND);
	r_grade_temperature      = ri.Cvar_Get("r_grade_temperature",      "0.0",  CVAR_ARCHIVE_ND);
	r_grade_tint             = ri.Cvar_Get("r_grade_tint",             "0.0",  CVAR_ARCHIVE_ND);
	r_grade_exposureBias     = ri.Cvar_Get("r_grade_exposureBias",     "0.0",  CVAR_ARCHIVE_ND);
	r_grade_contrast         = ri.Cvar_Get("r_grade_contrast",         "1.0",  CVAR_ARCHIVE_ND);
	r_grade_contrastPivot    = ri.Cvar_Get("r_grade_contrastPivot",    "0.38", CVAR_ARCHIVE_ND);
	r_grade_saturation       = ri.Cvar_Get("r_grade_saturation",       "1.0",  CVAR_ARCHIVE_ND);
	r_grade_vibrance         = ri.Cvar_Get("r_grade_vibrance",         "0.15", CVAR_ARCHIVE_ND);
	r_grade_hue              = ri.Cvar_Get("r_grade_hue",              "0.0",  CVAR_ARCHIVE_ND);
	r_grade_shadowLift       = ri.Cvar_Get("r_grade_shadowLift",       "0 0 0", CVAR_ARCHIVE_ND);
	r_grade_midGamma         = ri.Cvar_Get("r_grade_midGamma",         "1 1 1", CVAR_ARCHIVE_ND);
	r_grade_highlightGain    = ri.Cvar_Get("r_grade_highlightGain",    "1 1 1", CVAR_ARCHIVE_ND);
	r_grade_splitShadow      = ri.Cvar_Get("r_grade_splitShadow",      "0.50 0.50 0.55", CVAR_ARCHIVE_ND);
	r_grade_splitHighlight   = ri.Cvar_Get("r_grade_splitHighlight",   "1.0 0.95 0.90", CVAR_ARCHIVE_ND);
	r_grade_splitBalance     = ri.Cvar_Get("r_grade_splitBalance",     "0.50", CVAR_ARCHIVE_ND);
	r_grade_splitStrength    = ri.Cvar_Get("r_grade_splitStrength",    "0.0",  CVAR_ARCHIVE_ND);
	r_grade_lut              = ri.Cvar_Get("r_grade_lut",              "",     CVAR_ARCHIVE_ND);
	r_grade_lutIntensity     = ri.Cvar_Get("r_grade_lutIntensity",     "1.0",  CVAR_ARCHIVE_ND);
	ri.Cvar_CheckRange( r_sharpen, "0.0", "1.5", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_toe, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_shoulder, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_whitePoint, "0.5", "32.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_blackClip, "0.0", "0.25", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_highlightDesat, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_temperature, "-1.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_tint, "-1.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_exposureBias, "-4.0", "4.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_contrast, "0.25", "4.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_contrastPivot, "0.1", "0.9", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_saturation, "0.0", "3.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_vibrance, "-1.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_hue, "-180.0", "180.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_splitBalance, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_splitStrength, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_CheckRange( r_grade_lutIntensity, "0.0", "1.0", CV_FLOAT );

	ri.Cvar_SetDescription( r_vignette, "Vignette strength for post-processing lens darkening." );
	ri.Cvar_SetDescription( r_vignette_radius, "Vignette inner radius before edge darkening starts." );
	ri.Cvar_SetDescription( r_chromaticAberration, "Chromatic aberration strength for lens separation." );
	ri.Cvar_SetDescription( r_filmGrain, "Film grain intensity for post-process pass." );
	ri.Cvar_SetDescription( r_filmLook, "Source Engine–style film grain: luminance-dependent, fine-grained, soft-light blend (DoD:S, L4D quality)." );
	ri.Cvar_SetDescription( r_motionBlur, "Enables camera-reprojection motion blur in the Vulkan post pass." );
	ri.Cvar_SetDescription( r_motionBlurStrength, "Scales camera motion blur strength." );
	ri.Cvar_SetDescription( r_motionBlurMaxRadius, "Maximum camera motion blur radius in pixels." );
	ri.Cvar_SetDescription( r_motionBlurSamples, "Camera motion blur sample count." );
	ri.Cvar_SetDescription( r_depthOfField, "Enables thin-lens depth of field in the Vulkan post pass." );
	ri.Cvar_SetDescription( r_dofFocusDistance, "Depth of field focus distance in world units from the camera." );
	ri.Cvar_SetDescription( r_dofFocusRange, "In-focus band width around the focus distance." );
	ri.Cvar_SetDescription( r_dofAperture, "Depth of field aperture scale; higher values increase blur." );
	ri.Cvar_SetDescription( r_dofMaxBlur, "Maximum depth of field blur radius in pixels." );
	ri.Cvar_SetDescription( r_sharpen, "Post-process sharpen strength (0=off, 0.15=subtle, 0.3+=strong). Recovers detail lost by AA/tonemap." );
	ri.Cvar_SetDescription( r_grade_toe, "Filmic tonemap toe strength for shadow rolloff." );
	ri.Cvar_SetDescription( r_grade_shoulder, "Filmic tonemap shoulder strength for highlight compression." );
	ri.Cvar_SetDescription( r_grade_whitePoint, "Filmic tonemap white point in scene-linear units." );
	ri.Cvar_SetDescription( r_grade_blackClip, "Scene-linear black clip before tonemapping." );
	ri.Cvar_SetDescription( r_grade_highlightDesat, "Highlight desaturation amount after filmic compression." );
	ri.Cvar_SetDescription( r_grade_temperature, "White balance temperature adjustment (-1=cool, +1=warm)." );
	ri.Cvar_SetDescription( r_grade_tint, "White balance tint adjustment (-1=green, +1=magenta)." );
	ri.Cvar_SetDescription( r_grade_exposureBias, "Extra exposure bias in EV applied in the post pipeline." );
	ri.Cvar_SetDescription( r_grade_contrast, "Primary post contrast multiplier." );
	ri.Cvar_SetDescription( r_grade_contrastPivot, "Contrast pivot in display-referred space." );
	ri.Cvar_SetDescription( r_grade_saturation, "Primary post saturation multiplier." );
	ri.Cvar_SetDescription( r_grade_vibrance, "Selective saturation boost for muted colors." );
	ri.Cvar_SetDescription( r_grade_hue, "Display-referred hue rotation in degrees (-180 to 180)." );
	ri.Cvar_SetDescription( r_grade_shadowLift, "Shadow lift RGB vector, formatted as 'r g b'." );
	ri.Cvar_SetDescription( r_grade_midGamma, "Midtone gamma RGB vector, formatted as 'r g b'." );
	ri.Cvar_SetDescription( r_grade_highlightGain, "Highlight gain RGB vector, formatted as 'r g b'." );
	ri.Cvar_SetDescription( r_grade_splitShadow, "Split-tone shadow tint RGB, formatted as 'r g b'." );
	ri.Cvar_SetDescription( r_grade_splitHighlight, "Split-tone highlight tint RGB, formatted as 'r g b'." );
	ri.Cvar_SetDescription( r_grade_splitBalance, "Split-tone balance between shadows and highlights." );
	ri.Cvar_SetDescription( r_grade_splitStrength, "Split-tone blend strength." );
	ri.Cvar_SetDescription( r_grade_lut, "Path to a 32x32x32 LUT packed as a 2D strip texture." );
	ri.Cvar_SetDescription( r_grade_lutIntensity, "Blend strength for the active LUT." );
	ri.Cvar_SetGroup( r_ssr, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_maxDistance, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_stepSize, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_thickness, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_fadeEdge, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_roughnessThreshold, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_fresnelExponent, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_intensity, CVG_RENDERER );
	ri.Cvar_SetGroup( r_ssr_maxDepthGradient, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_sunDirX, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_sunDirY, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_sunDirZ, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_sunIntensity, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_scale, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_rayleighHeight, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_mieHeight, CVG_RENDERER );
	ri.Cvar_SetGroup( r_atmosphere_mieG, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_primaryFreq, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_primaryAmp, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_detailFreq, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_detailAmp, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_gustFreq, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_gustAmp, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_dirX, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_dirY, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_dirZ, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vegWind_strength, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vignette, CVG_RENDERER );
	ri.Cvar_SetGroup( r_vignette_radius, CVG_RENDERER );
	ri.Cvar_SetGroup( r_chromaticAberration, CVG_RENDERER );
	ri.Cvar_SetGroup( r_filmGrain, CVG_RENDERER );
	ri.Cvar_SetGroup( r_filmLook, CVG_RENDERER );
	ri.Cvar_SetGroup( r_motionBlur, CVG_RENDERER );
	ri.Cvar_SetGroup( r_motionBlurStrength, CVG_RENDERER );
	ri.Cvar_SetGroup( r_motionBlurMaxRadius, CVG_RENDERER );
	ri.Cvar_SetGroup( r_motionBlurSamples, CVG_RENDERER );
	ri.Cvar_SetGroup( r_depthOfField, CVG_RENDERER );
	ri.Cvar_SetGroup( r_dofFocusDistance, CVG_RENDERER );
	ri.Cvar_SetGroup( r_dofFocusRange, CVG_RENDERER );
	ri.Cvar_SetGroup( r_dofAperture, CVG_RENDERER );
	ri.Cvar_SetGroup( r_dofMaxBlur, CVG_RENDERER );
	ri.Cvar_SetGroup( r_sharpen, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_toe, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_shoulder, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_whitePoint, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_blackClip, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_highlightDesat, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_temperature, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_tint, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_exposureBias, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_contrast, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_contrastPivot, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_saturation, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_vibrance, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_hue, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_shadowLift, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_midGamma, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_highlightGain, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_splitShadow, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_splitHighlight, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_splitBalance, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_splitStrength, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_lut, CVG_RENDERER );
	ri.Cvar_SetGroup( r_grade_lutIntensity, CVG_RENDERER );

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
float PostFX_SSR_GetMaxDepthGradient(void) { return r_ssr_maxDepthGradient ? r_ssr_maxDepthGradient->value : 0.08f; }
float PostFX_SSR_GetFresnelExponent(void) { return r_ssr_fresnelExponent ? r_ssr_fresnelExponent->value : 2.5f; }

qboolean PostFX_Atmosphere_IsEnabled(void) { return r_atmosphere && r_atmosphere->integer > 0; }
void PostFX_Atmosphere_GetSunDirection(float *x, float *y, float *z) {
	if (x) *x = r_atmosphere_sunDirX ? r_atmosphere_sunDirX->value : 0.3f;
	if (y) *y = r_atmosphere_sunDirY ? r_atmosphere_sunDirY->value : 0.8f;
	if (z) *z = r_atmosphere_sunDirZ ? r_atmosphere_sunDirZ->value : 0.5f;
}
float PostFX_Atmosphere_GetSunIntensity(void) { return r_atmosphere_sunIntensity ? r_atmosphere_sunIntensity->value : 22.0f; }
float PostFX_Atmosphere_GetScale(void) { return r_atmosphere_scale ? r_atmosphere_scale->value : 4.0f; }
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

static int lastBloomState = -1;
static int lastSSAOState = -1;
static int lastSMAAState = -1;
static int lastSSRState = -1;
static int lastOITState = -1;
/* vk_create_post_process_pipeline and atmosphere pick HDR32 vs HDR64 from vk.color_format. */
static uint32_t s_lastPostPipelineVkFboColorFmt = 0u;

qboolean PostFX_NeedsPipelineUpdate(void) {
	/* Same heuristics as PostFX_PostPipelinesNeedUpdate (tr_cmds.c end-of-frame); used from
	 * vk_begin_frame so post/gamma pipelines refresh at frame start when cvars change. */
	return PostFX_PostPipelinesNeedUpdate();
}

qboolean PostFX_PostPipelinesNeedUpdate(void) {
	int bloomState = ( r_bloom && r_bloom->integer ) ? 1 : 0;
	int ssaoState = ( r_ssao && r_ssao->integer ) ? 1 : 0;
	int smaaState = ( r_ext_smaa && r_ext_smaa->integer ) ? 1 : 0;
	int ssrState = ( r_ssr && r_ssr->integer ) ? 1 : 0;
	int oitState = ( r_oit && r_oit->integer ) ? 1 : 0;
	cvar_t *r_bloom_scatter = ri.Cvar_Get( "r_bloom_scatter", "0.72", 0 );
	cvar_t *r_bloom_energy = ri.Cvar_Get( "r_bloom_energyPreserve", "1", 0 );
	const uint32_t fboColorFmt = (uint32_t)vk.color_format;

	/* r_ssr on/off: toggles whether vk_update_post_process_pipelines builds the SSR subpass. */
	if ( ( r_bloom && r_bloom->modified ) ||
		( r_bloom_intensity && r_bloom_intensity->modified ) ||
		( r_bloom_threshold && r_bloom_threshold->modified ) ||
		( r_bloom_threshold_mode && r_bloom_threshold_mode->modified ) ||
		( r_bloom_modulate && r_bloom_modulate->modified ) ||
		( r_bloomKnee && r_bloomKnee->modified ) ||
		( r_bloom_scatter && r_bloom_scatter->modified ) ||
		( r_bloom_energy && r_bloom_energy->modified ) ||
		( r_ssao && r_ssao->modified ) ||
		( r_ext_smaa && r_ext_smaa->modified ) ||
		( r_oit && r_oit->modified ) ||
		( r_ssr && r_ssr->modified ) )
	{
		s_lastPostPipelineVkFboColorFmt = fboColorFmt;
		lastBloomState = bloomState;
		lastSSAOState = ssaoState;
		lastSMAAState = smaaState;
		lastSSRState = ssrState;
		lastOITState = oitState;
		return qtrue;
	}

	if ( bloomState != lastBloomState || ssaoState != lastSSAOState || smaaState != lastSMAAState ||
		ssrState != lastSSRState || oitState != lastOITState || fboColorFmt != s_lastPostPipelineVkFboColorFmt ) {
		s_lastPostPipelineVkFboColorFmt = fboColorFmt;
		lastBloomState = bloomState;
		lastSSAOState = ssaoState;
		lastSMAAState = smaaState;
		lastSSRState = ssrState;
		lastOITState = oitState;
		return qtrue;
	}

	return qfalse;
}

/*
 * Call after vk_update_post_process_pipelines() so PostFX_PostPipelinesNeedUpdate does not
 * stay true for extra frames, and tr_cmds does not rebuild again the same frame.
 */
void PostFX_NotifyPostPipelinesRebuilt( void ) {
	int bloomState = ( r_bloom && r_bloom->integer ) ? 1 : 0;
	int ssaoState = ( r_ssao && r_ssao->integer ) ? 1 : 0;
	int smaaState = ( r_ext_smaa && r_ext_smaa->integer ) ? 1 : 0;
	int ssrState = ( r_ssr && r_ssr->integer ) ? 1 : 0;
	int oitState = ( r_oit && r_oit->integer ) ? 1 : 0;
	cvar_t *r_bloom_scatter = ri.Cvar_Get( "r_bloom_scatter", "0.72", 0 );
	cvar_t *r_bloom_energy = ri.Cvar_Get( "r_bloom_energyPreserve", "1", 0 );

	lastBloomState = bloomState;
	lastSSAOState = ssaoState;
	lastSMAAState = smaaState;
	lastSSRState = ssrState;
	lastOITState = oitState;
	s_lastPostPipelineVkFboColorFmt = (uint32_t)vk.color_format;

	if ( r_bloom ) {
		r_bloom->modified = qfalse;
	}
	if ( r_bloom_intensity ) {
		r_bloom_intensity->modified = qfalse;
	}
	if ( r_bloom_threshold ) {
		r_bloom_threshold->modified = qfalse;
	}
	if ( r_bloom_threshold_mode ) {
		r_bloom_threshold_mode->modified = qfalse;
	}
	if ( r_bloom_modulate ) {
		r_bloom_modulate->modified = qfalse;
	}
	if ( r_bloomKnee ) {
		r_bloomKnee->modified = qfalse;
	}
	if ( r_bloom_scatter ) {
		r_bloom_scatter->modified = qfalse;
	}
	if ( r_bloom_energy ) {
		r_bloom_energy->modified = qfalse;
	}
	if ( r_ssao ) {
		r_ssao->modified = qfalse;
	}
	if ( r_ext_smaa ) {
		r_ext_smaa->modified = qfalse;
	}
	if ( r_ssr ) {
		r_ssr->modified = qfalse;
	}
	if ( r_oit ) {
		r_oit->modified = qfalse;
	}
}

float PostFX_GetVignetteIntensity(void) { return r_vignette ? r_vignette->value : 0.0f; }
float PostFX_GetVignetteRadius(void) { return r_vignette_radius ? r_vignette_radius->value : 0.75f; }
float PostFX_GetChromaticAberration(void) { return r_chromaticAberration ? r_chromaticAberration->value : 0.0f; }
float PostFX_GetFilmGrain(void) { return r_filmGrain ? r_filmGrain->value : 0.0f; }
int PostFX_GetFilmLook(void) { return ( r_filmLook && r_filmLook->integer ) ? 1 : 0; }
qboolean PostFX_MotionBlur_IsEnabled(void) { return r_motionBlur && r_motionBlur->integer > 0; }
float PostFX_MotionBlur_GetStrength(void) { return r_motionBlurStrength ? r_motionBlurStrength->value : 1.0f; }
float PostFX_MotionBlur_GetMaxRadius(void) { return r_motionBlurMaxRadius ? r_motionBlurMaxRadius->value : 24.0f; }
int PostFX_MotionBlur_GetSamples(void) { return r_motionBlurSamples ? r_motionBlurSamples->integer : 12; }
qboolean PostFX_DepthOfField_IsEnabled(void) { return r_depthOfField && r_depthOfField->integer > 0; }
float PostFX_DepthOfField_GetFocusDistance(void) { return r_dofFocusDistance ? r_dofFocusDistance->value : 768.0f; }
float PostFX_DepthOfField_GetFocusRange(void) { return r_dofFocusRange ? r_dofFocusRange->value : 192.0f; }
float PostFX_DepthOfField_GetAperture(void) { return r_dofAperture ? r_dofAperture->value : 1.4f; }
float PostFX_DepthOfField_GetMaxBlur(void) { return r_dofMaxBlur ? r_dofMaxBlur->value : 18.0f; }
float PostFX_GetSharpen(void) { return r_sharpen ? r_sharpen->value : 0.0f; }
float PostFX_GetGradeToe(void) { return r_grade_toe ? r_grade_toe->value : 0.35f; }
float PostFX_GetGradeShoulder(void) { return r_grade_shoulder ? r_grade_shoulder->value : 0.22f; }
float PostFX_GetGradeWhitePoint(void) { return r_grade_whitePoint ? r_grade_whitePoint->value : 6.5f; }
float PostFX_GetGradeBlackClip(void) { return r_grade_blackClip ? r_grade_blackClip->value : 0.0f; }
float PostFX_GetGradeHighlightDesat(void) { return r_grade_highlightDesat ? r_grade_highlightDesat->value : 0.18f; }
float PostFX_GetGradeTemperature(void) { return r_grade_temperature ? r_grade_temperature->value : 0.0f; }
float PostFX_GetGradeTint(void) { return r_grade_tint ? r_grade_tint->value : 0.0f; }
float PostFX_GetGradeExposureBias(void) { return r_grade_exposureBias ? r_grade_exposureBias->value : 0.0f; }
float PostFX_GetGradeContrast(void) { return r_grade_contrast ? r_grade_contrast->value : 1.0f; }
float PostFX_GetGradeContrastPivot(void) { return r_grade_contrastPivot ? r_grade_contrastPivot->value : 0.38f; }
float PostFX_GetGradeSaturation(void) { return r_grade_saturation ? r_grade_saturation->value : 1.0f; }
float PostFX_GetGradeVibrance(void) { return r_grade_vibrance ? r_grade_vibrance->value : 0.15f; }
float PostFX_GetGradeHue(void) { return r_grade_hue ? r_grade_hue->value : 0.0f; }
void PostFX_GetShadowLift(float *rgb) {
	vec3_t parsed;
	PostFX_ParseRGBOrDefault( r_grade_shadowLift, parsed, 0.0f, 0.0f, 0.0f );
	if ( rgb ) {
		rgb[0] = parsed[0];
		rgb[1] = parsed[1];
		rgb[2] = parsed[2];
	}
}
void PostFX_GetMidGamma(float *rgb) {
	vec3_t parsed;
	PostFX_ParseRGBOrDefault( r_grade_midGamma, parsed, 1.0f, 1.0f, 1.0f );
	if ( rgb ) {
		rgb[0] = parsed[0];
		rgb[1] = parsed[1];
		rgb[2] = parsed[2];
	}
}
void PostFX_GetHighlightGain(float *rgb) {
	vec3_t parsed;
	PostFX_ParseRGBOrDefault( r_grade_highlightGain, parsed, 1.0f, 1.0f, 1.0f );
	if ( rgb ) {
		rgb[0] = parsed[0];
		rgb[1] = parsed[1];
		rgb[2] = parsed[2];
	}
}
void PostFX_GetSplitShadow(float *rgb) {
	vec3_t parsed;
	PostFX_ParseRGBOrDefault( r_grade_splitShadow, parsed, 0.50f, 0.50f, 0.55f );
	if ( rgb ) {
		rgb[0] = parsed[0];
		rgb[1] = parsed[1];
		rgb[2] = parsed[2];
	}
}
void PostFX_GetSplitHighlight(float *rgb) {
	vec3_t parsed;
	PostFX_ParseRGBOrDefault( r_grade_splitHighlight, parsed, 1.0f, 0.95f, 0.90f );
	if ( rgb ) {
		rgb[0] = parsed[0];
		rgb[1] = parsed[1];
		rgb[2] = parsed[2];
	}
}
float PostFX_GetSplitBalance(void) { return r_grade_splitBalance ? r_grade_splitBalance->value : 0.5f; }
float PostFX_GetSplitStrength(void) { return r_grade_splitStrength ? r_grade_splitStrength->value : 0.0f; }
float PostFX_GetLUTIntensity(void) { return r_grade_lutIntensity ? r_grade_lutIntensity->value : 1.0f; }
image_t *PostFX_GetLUTImage(void) {
	PostFX_UpdateLUTImage();
	return s_postfx_lut_image ? s_postfx_lut_image : tr.whiteImage;
}
