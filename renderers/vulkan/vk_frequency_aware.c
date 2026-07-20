/*
===========================================================================
Raster Ultra 1.12 — Frequency-Aware Rendering + Moiré Suppression.
Validation / policy controller. No global blur, no forced TAA, RT off.
===========================================================================
*/

#include "tr_local.h"
#include "vk_frequency_aware.h"
#include "vk.h"

static cvar_t *r_frequencyAware;
static cvar_t *r_frequencyTier;
static cvar_t *r_frequencyDebug;
static cvar_t *r_frequencySpecularAA;
static cvar_t *r_frequencyAlphaCoverage;
static cvar_t *r_frequencyProceduralCutoff;
static cvar_t *r_frequencyWaterCutoff;
static cvar_t *r_frequencyShadowDecorrelate;
static cvar_t *r_frequencySelectiveSS;
static cvar_t *r_frequencyStochastic;
static cvar_t *r_frequencyMipBiasFloor;
static cvar_t *r_materialAnisotropyMax;
static cvar_t *r_normalMapAnisotropy;
static qboolean s_cmds;
static vkFreqState_t s_freq;

typedef struct {
	const char *name;
	const char *hint;
	vkFreqAliasSource_t primarySource;
} vkFreqSceneDesc_t;

/* Deterministic moiré classification catalog (map/geometry may be external). */
static const vkFreqSceneDesc_t s_scenes[] = {
	{ "checkerboard_floor", "high-frequency tiled albedo", VK_FREQ_SRC_TEXTURE },
	{ "thin_parallel_lines", "1D periodic pattern", VK_FREQ_SRC_PROCEDURAL },
	{ "concentric_rings", "radial frequency", VK_FREQ_SRC_PROCEDURAL },
	{ "metal_grid", "repeating metal mesh", VK_FREQ_SRC_TEXTURE },
	{ "chain_link_fence", "alpha-tested fence", VK_FREQ_SRC_ALPHA },
	{ "alpha_grate", "coverage-sensitive cutout", VK_FREQ_SRC_ALPHA },
	{ "thin_wires", "subpixel geometry", VK_FREQ_SRC_GEOMETRY },
	{ "distant_stairs", "geometric specular", VK_FREQ_SRC_SPECULAR },
	{ "roof_tiles", "meso pattern + grazing", VK_FREQ_SRC_TEXTURE },
	{ "brushed_metal", "aniso normals", VK_FREQ_SRC_NORMAL },
	{ "hf_roughness", "roughness map aliasing", VK_FREQ_SRC_SPECULAR },
	{ "metallic_flakes", "nonlinear flake lobe", VK_FREQ_SRC_SPECULAR },
	{ "triplanar_concrete", "cross-axis projection", VK_FREQ_SRC_PROCEDURAL },
	{ "layered_decals", "layer frequency", VK_FREQ_SRC_TEXTURE },
	{ "procedural_stripes", "analytic pattern", VK_FREQ_SRC_PROCEDURAL },
	{ "fine_water_waves", "water normal spectrum", VK_FREQ_SRC_NORMAL },
	{ "transparent_grids", "OIT interference", VK_FREQ_SRC_TRANSPARENCY },
	{ "dense_shadows", "CSM/PCF interference", VK_FREQ_SRC_SHADOW },
	{ "oblique_ground", "anisotropy stress", VK_FREQ_SRC_TEXTURE },
	{ "moving_camera", "temporal lock-in risk", VK_FREQ_SRC_RECONSTRUCTION },
};

#define VK_FREQ_SCENE_COUNT ( (int)( sizeof( s_scenes ) / sizeof( s_scenes[0] ) ) )

void vk_frequency_aware_register_cvars( void )
{
	if ( r_frequencyAware ) {
		return;
	}

	r_frequencyAware = ri.Cvar_Get( "r_frequencyAware", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_frequencyAware, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_frequencyAware,
		"Raster Ultra 1.12 Frequency-Aware Rendering (latched).\n"
		"Classifies moiré sources and applies minimal correct filters.\n"
		"Does not force TAA, global blur, or ray tracing." );
	ri.Cvar_SetGroup( r_frequencyAware, CVG_RENDERER );

	r_frequencyTier = ri.Cvar_Get( "r_frequencyTier", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyTier, "0", "5", CV_INTEGER );
	ri.Cvar_SetDescription( r_frequencyTier,
		"0 off, 1 low, 2 medium, 3 high, 4 ultra, 5 reference (spatial SS lab)." );

	r_frequencyDebug = ri.Cvar_Get( "r_frequencyDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyDebug, "0", "20", CV_INTEGER );
	ri.Cvar_SetDescription( r_frequencyDebug,
		"Frequency diagnostics (dev): 0 off, 1 mip, 2 aniso, 3 variance, 4 alpha, "
		"5 classification, 6 Nyquist risk (see frequency_aware_status)." );

	r_frequencySpecularAA = ri.Cvar_Get( "r_frequencySpecularAA", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencySpecularAA, "0", "1", CV_INTEGER );

	r_frequencyAlphaCoverage = ri.Cvar_Get( "r_frequencyAlphaCoverage", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyAlphaCoverage, "0", "1", CV_INTEGER );

	r_frequencyProceduralCutoff = ri.Cvar_Get( "r_frequencyProceduralCutoff", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyProceduralCutoff, "0", "1", CV_INTEGER );

	r_frequencyWaterCutoff = ri.Cvar_Get( "r_frequencyWaterCutoff", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyWaterCutoff, "0", "1", CV_INTEGER );

	r_frequencyShadowDecorrelate = ri.Cvar_Get( "r_frequencyShadowDecorrelate", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyShadowDecorrelate, "0", "1", CV_INTEGER );

	r_frequencySelectiveSS = ri.Cvar_Get( "r_frequencySelectiveSS", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencySelectiveSS, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_frequencySelectiveSS,
		"Experimental: selective current-frame supersampling for high-risk pixels (default off)." );

	r_frequencyStochastic = ri.Cvar_Get( "r_frequencyStochastic", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyStochastic, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_frequencyStochastic,
		"Experimental: stochastic complete-material filtering for eligible materials (default off)." );

	r_frequencyMipBiasFloor = ri.Cvar_Get( "r_frequencyMipBiasFloor", "-0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_frequencyMipBiasFloor, "-2.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_frequencyMipBiasFloor,
		"While frequency-aware is active, do not apply mip LOD bias more negative than this "
		"(curbs sharpen-induced moiré). Does not rewrite r_mipLodBias permanently." );

	r_materialAnisotropyMax = ri.Cvar_Get( "r_materialAnisotropyMax", "16", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_materialAnisotropyMax, "1", "16", CV_INTEGER );

	r_normalMapAnisotropy = ri.Cvar_Get( "r_normalMapAnisotropy", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_normalMapAnisotropy, "0", "1", CV_INTEGER );
}

void vk_frequency_aware_init( void )
{
	vk_frequency_aware_register_cvars();
	Com_Memset( &s_freq, 0, sizeof( s_freq ) );
	s_freq.tier = VK_FREQ_TIER_MEDIUM;
	s_freq.specularAaStrength = 0.85f;
	s_freq.mipLodBiasClamp = -0.25f;
	s_freq.samplerAnisotropyCap = 16;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "frequency_aware_status", vk_frequency_aware_status_f );
		ri.Cmd_AddCommand( "renderer_sampler_status", vk_frequency_aware_sampler_status_f );
		ri.Cmd_AddCommand( "frequency_aware_scenes", vk_frequency_aware_scenes_f );
		s_cmds = qtrue;
	}

	ri.Printf( PRINT_ALL,
		"[VK][FrequencyAware] %s tier=%s scenes=%d (moiré classification; no TAA/blur/RT)\n",
		( r_frequencyAware && r_frequencyAware->integer ) ? "enabled" : "off",
		vk_frequency_aware_tier_name( s_freq.tier ),
		VK_FREQ_SCENE_COUNT );
}

void vk_frequency_aware_shutdown( void )
{
	Com_Memset( &s_freq, 0, sizeof( s_freq ) );
}

qboolean vk_frequency_aware_active( void )
{
	return ( r_frequencyAware && r_frequencyAware->integer ) ? qtrue : qfalse;
}

const vkFreqState_t *vk_frequency_aware_state( void )
{
	return &s_freq;
}

float vk_frequency_aware_specular_aa_strength( void )
{
	if ( !vk_frequency_aware_active() || !r_frequencySpecularAA || !r_frequencySpecularAA->integer ) {
		return 0.0f;
	}
	return s_freq.specularAaStrength;
}

qboolean vk_frequency_aware_alpha_coverage( void )
{
	return ( vk_frequency_aware_active() &&
		r_frequencyAlphaCoverage && r_frequencyAlphaCoverage->integer &&
		s_freq.alphaCoverage ) ? qtrue : qfalse;
}

float vk_frequency_aware_mip_bias_floor( void )
{
	if ( !vk_frequency_aware_active() ) {
		return -2.0f;
	}
	return s_freq.mipLodBiasClamp;
}

void vk_frequency_aware_begin_frame( void )
{
	int tier;

	if ( !vk_frequency_aware_active() ) {
		return;
	}

	tier = r_frequencyTier ? r_frequencyTier->integer : 2;
	if ( tier < 0 ) {
		tier = 0;
	}
	if ( tier > 5 ) {
		tier = 5;
	}
	s_freq.tier = (vkFreqTier_t)tier;
	s_freq.frameCount++;

	s_freq.anisotropicPolicy = ( tier >= VK_FREQ_TIER_LOW ) ? qtrue : qfalse;
	s_freq.specularNdFilter = ( r_frequencySpecularAA && r_frequencySpecularAA->integer &&
		tier >= VK_FREQ_TIER_LOW ) ? qtrue : qfalse;
	s_freq.alphaCoverage = ( r_frequencyAlphaCoverage && r_frequencyAlphaCoverage->integer &&
		tier >= VK_FREQ_TIER_LOW ) ? qtrue : qfalse;
	s_freq.proceduralCutoff = ( r_frequencyProceduralCutoff && r_frequencyProceduralCutoff->integer &&
		tier >= VK_FREQ_TIER_MEDIUM ) ? qtrue : qfalse;
	s_freq.waterFrequency = ( r_frequencyWaterCutoff && r_frequencyWaterCutoff->integer &&
		tier >= VK_FREQ_TIER_MEDIUM ) ? qtrue : qfalse;
	s_freq.shadowDecorrelate = ( r_frequencyShadowDecorrelate && r_frequencyShadowDecorrelate->integer &&
		tier >= VK_FREQ_TIER_MEDIUM ) ? qtrue : qfalse;
	s_freq.selectiveSS = ( r_frequencySelectiveSS && r_frequencySelectiveSS->integer &&
		tier >= VK_FREQ_TIER_HIGH ) ? qtrue : qfalse;
	s_freq.stochasticFilter = ( r_frequencyStochastic && r_frequencyStochastic->integer &&
		tier >= VK_FREQ_TIER_ULTRA ) ? qtrue : qfalse;

	s_freq.mipLodBiasClamp = r_frequencyMipBiasFloor ? r_frequencyMipBiasFloor->value : -0.25f;
	s_freq.samplerAnisotropyCap = (uint32_t)Com_Clamp( 1, 16,
		(float)( r_materialAnisotropyMax ? r_materialAnisotropyMax->integer : 16 ) );

	/* Strength scales with tier — never a global roughness override. */
	if ( tier <= VK_FREQ_TIER_LOW ) {
		s_freq.specularAaStrength = 0.65f;
	} else if ( tier == VK_FREQ_TIER_MEDIUM ) {
		s_freq.specularAaStrength = 0.85f;
	} else 	if ( tier == VK_FREQ_TIER_HIGH ) {
		s_freq.specularAaStrength = 1.0f;
	} else {
		s_freq.specularAaStrength = 1.15f;
	}
}

const char *vk_frequency_aware_source_name( vkFreqAliasSource_t s )
{
	static const char *names[] = {
		"none", "texture", "normal", "specular", "alpha", "procedural",
		"geometry", "shadow", "transparency", "reconstruction"
	};
	if ( s < 0 || s >= VK_FREQ_SRC_COUNT ) {
		return "invalid";
	}
	return names[s];
}

const char *vk_frequency_aware_tier_name( vkFreqTier_t t )
{
	static const char *names[] = {
		"off", "low", "medium", "high", "ultra", "reference"
	};
	if ( t < 0 || t > VK_FREQ_TIER_REFERENCE ) {
		return "invalid";
	}
	return names[t];
}

void vk_frequency_aware_scenes_f( void )
{
	int i;

	ri.Printf( PRINT_ALL, "=== Frequency-aware moiré scenes (%d) ===\n", VK_FREQ_SCENE_COUNT );
	for ( i = 0; i < VK_FREQ_SCENE_COUNT; i++ ) {
		ri.Printf( PRINT_ALL, " %2d %-22s primary=%-14s %s\n",
			i, s_scenes[i].name,
			vk_frequency_aware_source_name( s_scenes[i].primarySource ),
			s_scenes[i].hint );
	}
}

void vk_frequency_aware_sampler_status_f( void )
{
	float appliedBias;
	float maxAnisoHw;
	float maxAnisoReq;
	qboolean anisoOn;

	ri.Printf( PRINT_ALL, "=== renderer_sampler_status (Raster Ultra 1.12) ===\n" );
	anisoOn = ( r_ext_texture_filter_anisotropic && r_ext_texture_filter_anisotropic->integer &&
		vk.samplerAnisotropy ) ? qtrue : qfalse;
	maxAnisoHw = vk.maxAnisotropy;
	maxAnisoReq = r_ext_max_anisotropy ? (float)r_ext_max_anisotropy->integer : 1.0f;
	appliedBias = r_mipLodBias ? r_mipLodBias->value : 0.0f;

	ri.Printf( PRINT_ALL, "anisotropy     : %s req=%.0f hwMax=%.1f materialCap=%u normalMaps=%s\n",
		anisoOn ? "on" : "off",
		maxAnisoReq, maxAnisoHw,
		s_freq.samplerAnisotropyCap,
		( r_normalMapAnisotropy && r_normalMapAnisotropy->integer ) ? "prefer" : "default" );
	ri.Printf( PRINT_ALL, "mipLodBias     : %.3f (floor while FA active=%.3f)\n",
		appliedBias, s_freq.mipLodBiasClamp );
	ri.Printf( PRINT_ALL, "textureMode    : %s\n",
		r_textureMode ? r_textureMode->string : "?" );
	ri.Printf( PRINT_ALL, "sampler cache  : %d / %d\n", vk.samplers.count, MAX_VK_SAMPLERS );
	ri.Printf( PRINT_ALL, "picmip/nomip   : %d / %d\n",
		r_picmip ? r_picmip->integer : -1,
		r_nomip ? r_nomip->integer : -1 );
	ri.Printf( PRINT_ALL, "simpleMipMaps  : %d (CPU mip path; signal-specific rules documented)\n",
		r_simpleMipMaps ? r_simpleMipMaps->integer : -1 );

	if ( anisoOn && maxAnisoReq < 4.0f ) {
		ri.Printf( PRINT_ALL, "^3suspicious: max anisotropy < 4 on oblique surfaces\n" );
	}
	if ( appliedBias < -1.0f ) {
		ri.Printf( PRINT_ALL, "^3suspicious: strong negative mip bias (%.2f) can recreate moiré\n",
			appliedBias );
	}
	if ( !anisoOn ) {
		ri.Printf( PRINT_ALL, "^3suspicious: anisotropic filtering disabled\n" );
	}
	ri.Printf( PRINT_ALL, "policy         : no global blur; SMAA remains zero-history AA\n" );
}

void vk_frequency_aware_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== Frequency Aware (Raster Ultra 1.12) ===\n" );
	ri.Printf( PRINT_ALL, "active         : %s tier=%s frames=%u\n",
		vk_frequency_aware_active() ? "yes" : "no",
		vk_frequency_aware_tier_name( s_freq.tier ),
		s_freq.frameCount );
	ri.Printf( PRINT_ALL, "responses      : aniso=%s specND=%s alphaCov=%s procCut=%s water=%s shadow=%s\n",
		s_freq.anisotropicPolicy ? "yes" : "no",
		s_freq.specularNdFilter ? "yes" : "no",
		s_freq.alphaCoverage ? "yes" : "no",
		s_freq.proceduralCutoff ? "yes" : "no",
		s_freq.waterFrequency ? "yes" : "no",
		s_freq.shadowDecorrelate ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "experimental   : selectiveSS=%s stochastic=%s (default off)\n",
		s_freq.selectiveSS ? "yes" : "no",
		s_freq.stochasticFilter ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "specularAa     : strength=%.2f (Toksvig-style variance; not global roughness)\n",
		s_freq.specularAaStrength );
	ri.Printf( PRINT_ALL, "debug          : r_frequencyDebug=%d\n",
		r_frequencyDebug ? r_frequencyDebug->integer : 0 );
	ri.Printf( PRINT_ALL, "aa policy      : do not force TAA; SMAA / present recon remain valid\n" );
	ri.Printf( PRINT_ALL, "commands       : frequency_aware_scenes, renderer_sampler_status\n" );
}
