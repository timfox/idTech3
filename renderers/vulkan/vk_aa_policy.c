/*
===========================================================================
Layered antialiasing policy: r_aaMode drives SMAA / FXAA / Temporal Reconstruction.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_aa_policy.h"

/* Declared in tr_local.h / tr_render_mode_vk.c — ensure non-null before latch apply. */
extern cvar_t *r_presentAdaptiveRecon;
extern cvar_t *r_taa_feedbackStationary;
extern cvar_t *r_taa_feedbackMotion;
extern cvar_t *r_taaMotionVectors;

cvar_t *r_aaMode;
cvar_t *r_temporalHistoryWeight;
cvar_t *r_temporalVarianceClip;
cvar_t *r_temporalDisocclusion;
cvar_t *r_temporalReactiveMask;
cvar_t *r_reactiveMaskForce;
cvar_t *r_dynamicObjectHistoryMax;
cvar_t *r_dynamicObjectDepthThreshold;
cvar_t *r_dynamicObjectReactiveScale;
cvar_t *r_dynamicObjectRejectDilation;
cvar_t *r_dynamicObjectVelocityLimit;
cvar_t *r_temporalObjectDebug;
cvar_t *r_temporalWeaponAfterTaa;
cvar_t *r_weaponSsrIsolation;
cvar_t *r_weaponTemporalMode;
cvar_t *r_weaponTemporalHistoryWeight;
cvar_t *r_weaponTemporalVarianceGamma;
cvar_t *r_weaponTemporalDepthThreshold;
cvar_t *r_weaponTemporalReactiveScale;
cvar_t *r_temporalDebugVectorScale;
cvar_t *r_weaponBloomMode;
cvar_t *r_temporalDropClassDescriptor;
cvar_t *r_temporalSmaaCleanup;
cvar_t *r_debugMotionVectors;
cvar_t *r_debugHistoryRejection;
cvar_t *r_weaponAnalyticFog;
cvar_t *r_weaponLocalReflection;
cvar_t *r_weaponLocalAO;
cvar_t *r_weaponReadabilityLight;
cvar_t *r_weaponReadabilityLightIntensity;
cvar_t *r_weaponThinSightReject;
cvar_t *r_weaponTemporalCompare;

static qboolean s_want_temporal_cleanup_smaa;
static qboolean s_want_smaa_t2x;
static int s_last_applied_mode = -1;

static void vk_aa_set_int( cvar_t *cv, const char *name, int value )
{
	char buf[16];

	if ( !cv || !name ) {
		return;
	}
	if ( cv->integer == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%d", value );
	ri.Cvar_Set( name, buf );
	cv->integer = value;
	cv->modified = qtrue;
}

static void vk_aa_set_float( cvar_t *cv, const char *name, float value )
{
	char buf[32];

	if ( !cv || !name ) {
		return;
	}
	if ( cv->value == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%g", value );
	ri.Cvar_Set( name, buf );
	cv->value = value;
	cv->modified = qtrue;
}

void vk_aa_policy_register_cvars( void )
{
	r_aaMode = ri.Cvar_Get( "r_aaMode", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_aaMode, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_aaMode,
		"Presentation AA policy:\n"
		" 0=none\n"
		" 1=FXAA\n"
		" 2=SMAA 1x (default, certified zero-history)\n"
		" 3=Present-Time Adaptive Reconstruction (current-frame-first; migrated from SMAA T2x)\n"
		" 4=Temporal Reconstruction (native; upscale when render scale < 1)\n"
		" 5=Temporal Reconstruction + SMAA cleanup\n"
		" 6=spatial supersampled reference (r_ext_supersample)\n"
		"Applies r_ext_smaa / r_ext_fxaa / r_taa. Latched; vid_restart for SMAA/FXAA.\n"
		"Migration: former mode 3 (SMAA T2x) is now adaptive recon — use mode 2 for SMAA-only." );
	ri.Cvar_SetGroup( r_aaMode, CVG_RENDERER );

	r_temporalHistoryWeight = ri.Cvar_Get( "r_temporalHistoryWeight", "0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalHistoryWeight, "0", "0.95", CV_FLOAT );
	ri.Cvar_SetDescription( r_temporalHistoryWeight,
		"Max history weight for Temporal Reconstruction (scales stationary feedback). "
		"Default 0.72 limits dark-scene silhouette / banner trails; raise toward 0.85 only if stable." );
	ri.Cvar_SetGroup( r_temporalHistoryWeight, CVG_RENDERER );

	r_temporalVarianceClip = ri.Cvar_Get( "r_temporalVarianceClip", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalVarianceClip, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalVarianceClip,
		"YCoCg variance clipping in Temporal Reconstruction (1=on)." );
	ri.Cvar_SetGroup( r_temporalVarianceClip, CVG_RENDERER );

	r_temporalDisocclusion = ri.Cvar_Get( "r_temporalDisocclusion", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalDisocclusion, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalDisocclusion,
		"Depth-based disocclusion rejection for Temporal Reconstruction (1=on)." );
	ri.Cvar_SetGroup( r_temporalDisocclusion, CVG_RENDERER );

	r_dynamicObjectHistoryMax = ri.Cvar_Get( "r_dynamicObjectHistoryMax", "0.48", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dynamicObjectHistoryMax, "0", "0.85", CV_FLOAT );
	ri.Cvar_SetDescription( r_dynamicObjectHistoryMax,
		"Max TAA history weight for pixels with significant object/screen motion "
		"(below static-world r_temporalHistoryWeight)." );
	ri.Cvar_SetGroup( r_dynamicObjectHistoryMax, CVG_RENDERER );

	r_dynamicObjectDepthThreshold = ri.Cvar_Get( "r_dynamicObjectDepthThreshold", "0.012", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dynamicObjectDepthThreshold, "0.001", "0.1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dynamicObjectDepthThreshold,
		"Relative linear-depth error threshold for dynamic-object silhouette rejection." );
	ri.Cvar_SetGroup( r_dynamicObjectDepthThreshold, CVG_RENDERER );

	r_dynamicObjectReactiveScale = ri.Cvar_Get( "r_dynamicObjectReactiveScale", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dynamicObjectReactiveScale, "0", "2", CV_FLOAT );
	ri.Cvar_SetGroup( r_dynamicObjectReactiveScale, CVG_RENDERER );

	r_dynamicObjectRejectDilation = ri.Cvar_Get( "r_dynamicObjectRejectDilation", "1.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dynamicObjectRejectDilation, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_dynamicObjectRejectDilation,
		"Trailing-edge disocclusion dilation in pixels for moving dynamic objects." );
	ri.Cvar_SetGroup( r_dynamicObjectRejectDilation, CVG_RENDERER );

	r_dynamicObjectVelocityLimit = ri.Cvar_Get( "r_dynamicObjectVelocityLimit", "48", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dynamicObjectVelocityLimit, "4", "256", CV_FLOAT );
	ri.Cvar_SetDescription( r_dynamicObjectVelocityLimit,
		"Screen-space velocity (pixels) above which history is rejected as invalid/teleport." );
	ri.Cvar_SetGroup( r_dynamicObjectVelocityLimit, CVG_RENDERER );

	r_temporalObjectDebug = ri.Cvar_Get( "r_temporalObjectDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_temporalObjectDebug, "0", "12", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalObjectDebug,
		"Dynamic-object temporal debug:\n"
		" 0 final\n"
		" 1 object velocity (magenta=NaN/Inf, yellow=large)\n"
		" 2 previous transform validity\n"
		" 3 current object ID proxy\n"
		" 4 reprojected previous object ID proxy\n"
		" 5 object-ID rejection\n"
		" 6 current depth\n"
		" 7 previous depth\n"
		" 8 depth rejection\n"
		" 9 history weight\n"
		" 10 current versus history difference\n"
		" 11 reactive mask\n"
		" 12 disocclusion mask" );
	ri.Cvar_SetGroup( r_temporalObjectDebug, CVG_RENDERER );

	r_temporalReactiveMask = ri.Cvar_Get( "r_temporalReactiveMask", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalReactiveMask, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalReactiveMask,
		"Allocate/stamp a transparency reactive mask for Temporal Reconstruction "
		"(OIT/Forward+ transparent/stochastic). Heuristic reject (weapon depth, luma flash, "
		"history bleed) always runs when TAA is on; this cvar only enables the stamped mask." );
	ri.Cvar_SetGroup( r_temporalReactiveMask, CVG_RENDERER );

	r_reactiveMaskForce = ri.Cvar_Get( "r_reactiveMaskForce", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_reactiveMaskForce, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_reactiveMaskForce,
		"Force full-res temporal reactive mask allocation even when TAA/upscale is off." );
	ri.Cvar_SetGroup( r_reactiveMaskForce, CVG_RENDERER );

	r_temporalWeaponAfterTaa = ri.Cvar_Get( "r_temporalWeaponAfterTaa", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalWeaponAfterTaa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalWeaponAfterTaa,
		"Defer RDF_NOWORLDMODEL weapon/view-model draws until after world Temporal Reconstruction "
		"so weapon pixels never enter TAA history (fixes dark offset silhouettes)." );
	ri.Cvar_SetGroup( r_temporalWeaponAfterTaa, CVG_RENDERER );

	r_weaponSsrIsolation = ri.Cvar_Get( "r_weaponSsrIsolation", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponSsrIsolation, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponSsrIsolation,
		"Defer RDF_NOWORLDMODEL weapon/view-model draws until after world SSR/SSAO. "
		"Prevents DEPTH_RANGE_WEAPON color/depth from contaminating those passes; 0 restores legacy ordering." );
	ri.Cvar_SetGroup( r_weaponSsrIsolation, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][weapon] SSR isolation %s (r_weaponSsrIsolation=%d)\n",
		r_weaponSsrIsolation->integer ? "enabled" : "disabled",
		r_weaponSsrIsolation->integer );

	r_weaponTemporalMode = ri.Cvar_Get( "r_weaponTemporalMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponTemporalMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponTemporalMode,
		"Weapon Temporal Reconstruction: 0=no weapon history, 1=classified shared history (default), "
		"2=independent weapon color/depth/coverage history." );
	ri.Cvar_SetGroup( r_weaponTemporalMode, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][weapon] temporal mode %d (r_weaponTemporalMode)\n",
		r_weaponTemporalMode->integer );

	r_weaponTemporalHistoryWeight = ri.Cvar_Get( "r_weaponTemporalHistoryWeight", "0.58", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponTemporalHistoryWeight, "0", "0.9", CV_FLOAT );
	r_weaponTemporalVarianceGamma = ri.Cvar_Get( "r_weaponTemporalVarianceGamma", "0.75", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponTemporalVarianceGamma, "0.1", "3", CV_FLOAT );
	/* Relative positive view-depth tolerance (was device-Z 0.025). */
	r_weaponTemporalDepthThreshold = ri.Cvar_Get( "r_weaponTemporalDepthThreshold", "0.04", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponTemporalDepthThreshold, "0.001", "0.5", CV_FLOAT );
	r_weaponTemporalReactiveScale = ri.Cvar_Get( "r_weaponTemporalReactiveScale", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponTemporalReactiveScale, "0", "4", CV_FLOAT );
	ri.Cvar_SetGroup( r_weaponTemporalHistoryWeight, CVG_RENDERER );
	ri.Cvar_SetGroup( r_weaponTemporalVarianceGamma, CVG_RENDERER );
	ri.Cvar_SetGroup( r_weaponTemporalDepthThreshold, CVG_RENDERER );
	ri.Cvar_SetGroup( r_weaponTemporalReactiveScale, CVG_RENDERER );
	r_temporalDebugVectorScale = ri.Cvar_Get( "r_temporalDebugVectorScale", "80", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalDebugVectorScale, "1", "1000", CV_FLOAT );
	ri.Cvar_SetGroup( r_temporalDebugVectorScale, CVG_RENDERER );
	r_weaponBloomMode = ri.Cvar_Get( "r_weaponBloomMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponBloomMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponBloomMode,
		"Architecture B weapon bloom: 0=world bloom before weapon (no weapon bloom), "
		"1=combined HDR global bloom after weapon (default), "
		"2=legacy world bloom + dedicated class-gated weapon bloom after weapon flush." );
	ri.Cvar_SetGroup( r_weaponBloomMode, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][weapon] bloom mode %d (r_weaponBloomMode)\n",
		r_weaponBloomMode->integer );

	r_weaponAnalyticFog = ri.Cvar_Get( "r_weaponAnalyticFog", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponAnalyticFog, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponAnalyticFog,
		"Analytic camera-space fog for first-person weapon pixels only. "
		"Never samples world volumetric temporal history (Architecture B)." );
	ri.Cvar_SetGroup( r_weaponAnalyticFog, CVG_RENDERER );

	r_weaponLocalReflection = ri.Cvar_Get( "r_weaponLocalReflection", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponLocalReflection, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponLocalReflection,
		"Weapon-local probe/IBL reflection approximation. World SSR stays disabled for weapon pixels." );
	ri.Cvar_SetGroup( r_weaponLocalReflection, CVG_RENDERER );

	r_weaponLocalAO = ri.Cvar_Get( "r_weaponLocalAO", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponLocalAO, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponLocalAO,
		"Subtle non-temporal weapon-local contact/crease AO. Never samples world temporal AO." );
	ri.Cvar_SetGroup( r_weaponLocalAO, CVG_RENDERER );

	r_weaponReadabilityLight = ri.Cvar_Get( "r_weaponReadabilityLight", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponReadabilityLight, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponReadabilityLight,
		"Optional authored first-person readability light (restrained; off by default)." );
	ri.Cvar_SetGroup( r_weaponReadabilityLight, CVG_RENDERER );

	r_weaponReadabilityLightIntensity = ri.Cvar_Get( "r_weaponReadabilityLightIntensity", "0.15", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponReadabilityLightIntensity, "0", "1", CV_FLOAT );
	ri.Cvar_SetGroup( r_weaponReadabilityLightIntensity, CVG_RENDERER );

	r_weaponThinSightReject = ri.Cvar_Get( "r_weaponThinSightReject", "0.65", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weaponThinSightReject, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_weaponThinSightReject,
		"Responsive rejection scale for thin sights / fast silhouettes in weapon TAA "
		"(coverage + MVP velocity + class transition + reactive; not world depth)." );
	ri.Cvar_SetGroup( r_weaponThinSightReject, CVG_RENDERER );

	r_weaponTemporalCompare = ri.Cvar_Get( "r_weaponTemporalCompare", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_weaponTemporalCompare, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponTemporalCompare,
		"Mode comparison debug: 0=off, 1=split-screen (left mode0 / right mode2), "
		"2=alternate frames between modes 0/1/2." );
	ri.Cvar_SetGroup( r_weaponTemporalCompare, CVG_RENDERER );

	ri.Printf( PRINT_ALL,
		"[VK][weapon] presentation: fog=%d reflection=%d ao=%d readability=%d thinSight=%.2f\n",
		r_weaponAnalyticFog->integer,
		r_weaponLocalReflection->integer,
		r_weaponLocalAO->integer,
		r_weaponReadabilityLight->integer,
		r_weaponThinSightReject->value );

	r_temporalDropClassDescriptor = ri.Cvar_Get( "r_temporalDropClassDescriptor", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_temporalDropClassDescriptor, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalDropClassDescriptor,
		"Fault injection: drop the temporal class descriptor for one or more frames. "
		"The renderer must bind TemporalUnclassifiedR8 and reject all history safely." );

	r_temporalSmaaCleanup = ri.Cvar_Get( "r_temporalSmaaCleanup", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalSmaaCleanup, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalSmaaCleanup,
		"When 1 (or r_aaMode 5), run light SMAA after Temporal Reconstruction." );
	ri.Cvar_SetGroup( r_temporalSmaaCleanup, CVG_RENDERER );

	r_debugMotionVectors = ri.Cvar_Get( "r_debugMotionVectors", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_debugMotionVectors, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_debugMotionVectors,
		"Overlay motion vectors in Temporal Reconstruction resolve (debug)." );
	ri.Cvar_SetGroup( r_debugMotionVectors, CVG_RENDERER );

	r_debugHistoryRejection = ri.Cvar_Get( "r_debugHistoryRejection", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_debugHistoryRejection, "0", "12", CV_INTEGER );
	ri.Cvar_SetDescription( r_debugHistoryRejection,
		"Temporal / Present Adaptive Reconstruction debug:\n"
		" 0 off\n"
		" 1 motion vectors (also r_debugMotionVectors)\n"
		" 2 rejection reasons (legacy color codes)\n"
		" 3 reactive mask\n"
		" 4 history confidence/weight\n"
		" 5 disocclusion\n"
		" 6 reprojected history UV\n"
		" 7 near-weapon heuristic\n"
		" 8 world vs reactive ownership\n"
		" 9 adaptive current-frame sample mask\n"
		" 10 current vs history contribution\n"
		" 11 neighborhood variance (Y)\n"
		" 12 unclipped vs clipped history delta" );
	ri.Cvar_SetGroup( r_debugHistoryRejection, CVG_RENDERER );

	if ( !r_presentAdaptiveRecon ) {
		r_presentAdaptiveRecon = ri.Cvar_Get( "r_presentAdaptiveRecon", "0", CVAR_ARCHIVE_ND );
	}
}

void vk_aa_policy_apply( void )
{
	int mode;

	if ( !r_aaMode ) {
		return;
	}

	mode = r_aaMode->integer;
	if ( mode < 0 || mode > 6 ) {
		mode = 2;
	}

	s_want_temporal_cleanup_smaa = qfalse;
	s_want_smaa_t2x = qfalse;

	switch ( mode ) {
	case 0: /* none */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 0 );
		break;
	case 1: /* FXAA */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 1 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 1 );
		break;
	case 2: /* SMAA 1x — shipping default / certified zero-history */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 1 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 1 );
		if ( r_smaa_preset && r_smaa_preset->integer <= 0 ) {
			vk_aa_set_int( r_smaa_preset, "r_smaa_preset", 3 );
		}
		break;
	case 3: /* Present-Time Adaptive Reconstruction (migrated from SMAA T2x) */
		/*
		 * Current-frame-first: no full-frame SMAA over temporal (double-blur).
		 * SMAA remains certified via mode 2. Rejected pixels use spatial fallback in taa.frag.
		 */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 1 );
		vk_aa_set_int( r_taaMotionVectors, "r_taaMotionVectors", 1 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 0 );
		vk_aa_set_int( r_temporalVarianceClip, "r_temporalVarianceClip", 1 );
		vk_aa_set_int( r_temporalDisocclusion, "r_temporalDisocclusion", 1 );
		vk_aa_set_int( r_temporalReactiveMask, "r_temporalReactiveMask", 1 );
		vk_aa_set_int( r_temporalWeaponAfterTaa, "r_temporalWeaponAfterTaa", 1 );
		vk_aa_set_int( r_presentAdaptiveRecon, "r_presentAdaptiveRecon", 1 );
		vk_aa_set_float( r_temporalHistoryWeight, "r_temporalHistoryWeight", 0.42f );
		vk_aa_set_float( r_taa_feedbackStationary, "r_taa_feedbackStationary", 0.55f );
		vk_aa_set_float( r_taa_feedbackMotion, "r_taa_feedbackMotion", 0.28f );
		s_want_smaa_t2x = qtrue; /* legacy flag == present adaptive */
		break;
	case 4: /* Temporal Reconstruction */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 1 );
		vk_aa_set_int( r_taaMotionVectors, "r_taaMotionVectors", 1 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 0 );
		break;
	case 5: /* Temporal Reconstruction + SMAA cleanup */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 1 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 1 );
		vk_aa_set_int( r_taaMotionVectors, "r_taaMotionVectors", 1 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		/* Spatial AA after bloom/fog, then temporal; cleanup flag for post-TAA light pass later. */
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 1 );
		s_want_temporal_cleanup_smaa = qtrue;
		break;
	case 6: /* spatial supersampled reference */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 1 );
		break;
	default:
		break;
	}

	if ( mode != s_last_applied_mode ) {
		static const char *names[] = {
			"none", "FXAA", "SMAA 1x", "Present-Time Adaptive Reconstruction",
			"Temporal Reconstruction", "Temporal Reconstruction + SMAA cleanup",
			"spatial supersample"
		};
		ri.Printf( PRINT_ALL, "[VK][AA] r_aaMode %d (%s)\n", mode, names[mode] );
		if ( mode == 3 ) {
			ri.Printf( PRINT_ALL,
				"[VK][AA] migration: mode 3 was SMAA T2x; now current-frame-first adaptive recon "
				"(SMAA certified path remains r_aaMode 2). frame_generation=off\n" );
		}
		s_last_applied_mode = mode;
	}
}

qboolean vk_aa_policy_wants_temporal_cleanup_smaa( void )
{
	return ( s_want_temporal_cleanup_smaa ||
		( r_temporalSmaaCleanup && r_temporalSmaaCleanup->integer ) ) ? qtrue : qfalse;
}

qboolean vk_aa_policy_wants_smaa_t2x( void )
{
	/* Legacy name: now means Present-Time Adaptive Reconstruction (aaMode 3). */
	return s_want_smaa_t2x;
}

qboolean vk_aa_policy_wants_present_adaptive( void )
{
	return ( s_want_smaa_t2x || ( r_aaMode && r_aaMode->integer == 3 ) ) ? qtrue : qfalse;
}
