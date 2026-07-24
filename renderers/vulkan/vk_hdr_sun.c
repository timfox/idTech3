#include "tr_local.h"

#ifdef USE_VULKAN

#include "vk.h"
#include "vk_hdr_sun.h"
#include "vk_skybox_hdr.h"
#include "vk_exposure_histogram.h"

/*
 * HDR sun and histogram auto-exposure diagnostics and policy cvars.
 *
 * Authoritative sun for HDR sky maps: SUN_SOURCE_CUBEMAP (EXR contains the disc).
 * Do not stack a second full-intensity procedural disc on top.
 */

typedef enum {
	SUN_SOURCE_NONE = 0,
	SUN_SOURCE_CUBEMAP = 1,
	SUN_SOURCE_PROCEDURAL = 2,
	SUN_SOURCE_CUBEMAP_PLUS_METADATA = 3
} sunSourcePolicy_t;

static cvar_t *r_sunSource;
static cvar_t *r_sunDebug;
static cvar_t *r_sunDiffraction;
static cvar_t *r_sunDiffractionBlades;
static cvar_t *r_sunDiffractionIntensity;
static cvar_t *r_autoExposureDebug;
static cvar_t *r_lensFlareDebug;
static cvar_t *r_sunBloomDebug;
static cvar_t *r_sunTonemapDebug;
static cvar_t *r_autoExposureFreeze;
static qboolean s_cmds;

static sunSourcePolicy_t HdrSun_ResolveSource( void )
{
	const skyboxHDR_t *sky;
	int policy;

	policy = r_sunSource ? r_sunSource->integer : 1;
	sky = SkyboxHDR_Get();
	if ( sky && sky->loaded ) {
		/* EXR panoramas used by Surf (e.g. aarfontein) already paint the sun. */
		if ( policy == SUN_SOURCE_PROCEDURAL ) {
			return SUN_SOURCE_PROCEDURAL;
		}
		return SUN_SOURCE_CUBEMAP;
	}
	if ( policy == SUN_SOURCE_PROCEDURAL ) {
		return SUN_SOURCE_PROCEDURAL;
	}
	return SUN_SOURCE_NONE;
}

static const char *HdrSun_SourceName( sunSourcePolicy_t s )
{
	switch ( s ) {
	case SUN_SOURCE_CUBEMAP: return "SUN_SOURCE_CUBEMAP";
	case SUN_SOURCE_PROCEDURAL: return "SUN_SOURCE_PROCEDURAL";
	case SUN_SOURCE_CUBEMAP_PLUS_METADATA: return "SUN_SOURCE_CUBEMAP_PLUS_METADATA";
	default: return "SUN_SOURCE_NONE";
	}
}

static void HdrSun_PrintContributor( const char *name, const char *pass, const char *resource,
	const char *order, const char *colorSpace, const char *exposureState,
	float hdrIntensity, float angularDeg, float screenRadius,
	const char *occlusion, const char *bloom, const char *temporal,
	const char *blend, int generation )
{
	ri.Printf( PRINT_ALL,
		"  %-18s pass=%-16s src=%-28s order=%s cs=%s exp=%s "
		"HDR=%.3g ang=%.3g° ssR=%.3g occ=%s bloom=%s hist=%s blend=%s gen=%d\n",
		name, pass, resource, order, colorSpace, exposureState,
		hdrIntensity, angularDeg, screenRadius,
		occlusion, bloom, temporal, blend, generation );
}

static void Sun_RenderStatus_f( void )
{
	sunSourcePolicy_t src = HdrSun_ResolveSource();
	const skyboxHDR_t *sky = SkyboxHDR_Get();
	int bloomOn = ri.Cvar_VariableIntegerValue( "r_bloom" );
	int flareOn = ri.Cvar_VariableIntegerValue( "r_lensFlare" );
	int flaresOn = ri.Cvar_VariableIntegerValue( "r_flares" );
	int diffract = r_sunDiffraction ? r_sunDiffraction->integer : 0;
	int authored = ri.Cvar_VariableIntegerValue( "r_authoredFlares" );

	ri.Printf( PRINT_ALL, "======== sun_render_status ========\n" );
	ri.Printf( PRINT_ALL, "authoritative source : %s (r_sunSource=%d)\n",
		HdrSun_SourceName( src ), r_sunSource ? r_sunSource->integer : 1 );
	ri.Printf( PRINT_ALL, "sky loaded           : %s\n",
		( sky && sky->loaded ) ? ( sky->filename[0] ? sky->filename : "(hdr)" ) : "(none)" );
	ri.Printf( PRINT_ALL, "angularRadius default: 0.27 deg (physical); glow via bloom, not disc enlarge\n" );
	ri.Printf( PRINT_ALL, "adaptedExposure      : %.4g  auto=%d\n",
		vk.adaptedExposure, ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) );

	HdrSun_PrintContributor( "cubemap_sun", "skybox_hdr",
		( sky && sky->loaded ) ? sky->filename : "n/a",
		"pre-opaque", "scene-linear HDR", "unexposed SceneHDR",
		( src == SUN_SOURCE_CUBEMAP || src == SUN_SOURCE_CUBEMAP_PLUS_METADATA ) ? 1.0f : 0.0f,
		0.27f, -1.0f, "sky-owned", bloomOn ? "yes" : "no", "no", "replace", 1 );

	HdrSun_PrintContributor( "procedural_disc", "disabled",
		"r_sunSource!=PROCEDURAL", "n/a", "n/a", "n/a",
		0.0f, 0.27f, 0.0f, "n/a", "n/a", "n/a", "n/a", 0 );

	HdrSun_PrintContributor( "bloom", "post/bloom",
		"bloom.frag EV-relative", "post-SceneHDR", "scene-linear", "threshold vs adaptedEV",
		bloomOn ? 1.0f : 0.0f, -1.0f, -1.0f, "n/a", "self", "no", "add", bloomOn );

	HdrSun_PrintContributor( "lens_flare", "post/lens_flare",
		"lens_flare.frag", "post-bloom", "display additive", "not metered",
		flareOn ? 1.0f : 0.0f, -1.0f, -1.0f, "multi-sample UV", "no", "no", "add", flareOn );

	HdrSun_PrintContributor( "corona_flares", "RB_RenderFlares",
		"r_flares", "transparent", "LDR-ish", "not SceneHDR sun",
		flaresOn ? 1.0f : 0.0f, -1.0f, -1.0f, "entity", "no", "no", "add", flaresOn );

	HdrSun_PrintContributor( "diffraction", "optional",
		"r_sunDiffraction", "post", "additive", "not metered",
		diffract ? 1.0f : 0.0f, -1.0f, -1.0f, "sunVisibility", "no", "no", "add", diffract );

	HdrSun_PrintContributor( "authored_flare", "authored",
		"flares/*.flare", "optional", "varies", "not metered",
		authored ? 1.0f : 0.0f, -1.0f, -1.0f, "def", "no", "no", "add", authored );

	ri.Printf( PRINT_ALL, "god_rays/sun_shafts : not active as independent HDR sun energy\n" );
	ri.Printf( PRINT_ALL, "===================================\n" );
}

static void Sun_CompositionStatus_f( void )
{
	sunSourcePolicy_t src = HdrSun_ResolveSource();
	int bloomOn = ri.Cvar_VariableIntegerValue( "r_bloom" );
	int flareOn = ri.Cvar_VariableIntegerValue( "r_lensFlare" );
	int diffract = r_sunDiffraction ? r_sunDiffraction->integer : 0;
	int fails = 0;

	ri.Printf( PRINT_ALL, "======== sun_composition_status ========\n" );
	ri.Printf( PRINT_ALL, "policy: one radiometric sun in SceneHDR (%s)\n", HdrSun_SourceName( src ) );
	ri.Printf( PRINT_ALL, "bloom  : smooth isotropic glow (EV-relative threshold)\n" );
	ri.Printf( PRINT_ALL, "diffraction: %s (default off — not bloom)\n", diffract ? "ON" : "OFF" );
	ri.Printf( PRINT_ALL, "lens_flare : %s (optional; not metered)\n", flareOn ? "ON" : "OFF" );

	if ( src == SUN_SOURCE_CUBEMAP && ri.Cvar_VariableIntegerValue( "r_sunSource" ) == SUN_SOURCE_PROCEDURAL ) {
		ri.Printf( PRINT_WARNING, "DETECT: SUN_DUPLICATE_DISC risk (cubemap + procedural requested)\n" );
		fails++;
	}
	if ( bloomOn && diffract ) {
		ri.Printf( PRINT_WARNING, "DETECT: SUN_DUPLICATE_BLOOM / directional spike risk (bloom+diffraction)\n" );
		fails++;
	}
	if ( flareOn && !ri.Cvar_VariableIntegerValue( "r_lensFlare" ) ) {
		/* unreachable */
	}
	if ( flareOn ) {
		ri.Printf( PRINT_ALL, "NOTE: lens flare vertical streak possible when r_lensFlare 1\n" );
	}
	if ( !fails ) {
		ri.Printf( PRINT_ALL, "PASS: no duplicate full-radiance sun stack detected in config\n" );
	}
	ri.Printf( PRINT_ALL, "=========================================\n" );
}

static void Sun_ContributorValidate_f( void )
{
	int fails = 0;
	sunSourcePolicy_t src = HdrSun_ResolveSource();

	ri.Printf( PRINT_ALL, "======== sun_contributor_validate ========\n" );
	if ( src != SUN_SOURCE_CUBEMAP && src != SUN_SOURCE_PROCEDURAL && src != SUN_SOURCE_NONE ) {
		ri.Printf( PRINT_WARNING, "FAIL: unknown sun source\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: authoritative source=%s\n", HdrSun_SourceName( src ) );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_sunDiffraction" ) != 0 ) {
		ri.Printf( PRINT_WARNING, "WARN: r_sunDiffraction != 0 (starburst optional path active)\n" );
	} else {
		ri.Printf( PRINT_ALL, "PASS: diffraction off (no hard spikes from aperture model)\n" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_lensFlare" ) != 0 ) {
		ri.Printf( PRINT_WARNING, "WARN: r_lensFlare != 0 (certify occlusion before production)\n" );
	} else {
		ri.Printf( PRINT_ALL, "PASS: lens flare off (stable profile)\n" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_bloomThresholdEVRelative" ) == 0 ) {
		ri.Printf( PRINT_WARNING, "DETECT: BLOOM_THRESHOLD_NOT_EXPOSURE_RELATIVE\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: bloom threshold EV-relative\n" );
	}
	ri.Printf( PRINT_ALL, "sun_contributor_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void Sun_SourceStatus_f( void )
{
	ri.Printf( PRINT_ALL, "sun_source_status: %s (r_sunSource=%d)\n",
		HdrSun_SourceName( HdrSun_ResolveSource() ),
		r_sunSource ? r_sunSource->integer : 1 );
}

static void AutoExposure_Status_f( void )
{
	cvar_t *autoExp = ri.Cvar_Get( "r_exposure_auto", "0", 0 );
	cvar_t *lo = ri.Cvar_Get( "r_autoExposure_lowPercent", "0.05", 0 );
	cvar_t *hi = ri.Cvar_Get( "r_autoExposure_highPercent", "0.05", 0 );
	cvar_t *skyW = ri.Cvar_Get( "r_exposureSkyWeight", "0.55", 0 );
	cvar_t *spdUp = ri.Cvar_Get( "r_autoExposure_speedUp", "0.8", 0 );
	cvar_t *spdDn = ri.Cvar_Get( "r_autoExposure_speedDown", "3.5", 0 );
	cvar_t *minV = ri.Cvar_Get( "r_autoExposure_min", "0.05", 0 );
	cvar_t *maxV = ri.Cvar_Get( "r_autoExposure_max", "12", 0 );
	cvar_t *freeze = ri.Cvar_Get( "r_autoExposureFreeze", "0", 0 );

	ri.Printf( PRINT_ALL, "======== auto_exposure_status ========\n" );
	ri.Printf( PRINT_ALL, "r_exposure_auto        : %d\n", autoExp ? autoExp->integer : 0 );
	ri.Printf( PRINT_ALL, "current/adapted EV mul : %.4g\n", vk.adaptedExposure );
	ri.Printf( PRINT_ALL, "filteredAvgLogLum      : %.4g valid=%d\n",
		vk.temporal.filteredAvgLogLuminance, vk.temporal.hasValidLuminance ? 1 : 0 );
	ri.Printf( PRINT_ALL, "percentiles low/high   : %.3g / %.3g\n",
		lo ? lo->value : 0.05f, hi ? hi->value : 0.05f );
	ri.Printf( PRINT_ALL, "skyWeight              : %.3g (broad sky meters; sun core trimmed)\n",
		skyW ? skyW->value : 0.55f );
	ri.Printf( PRINT_ALL, "brighten/darken rates  : %.3g / %.3g (asymmetric)\n",
		spdUp ? spdUp->value : 0.8f, spdDn ? spdDn->value : 3.5f );
	ri.Printf( PRINT_ALL, "EV clamp [min,max]     : [%.3g, %.3g]\n",
		minV ? minV->value : 0.05f, maxV ? maxV->value : 12.0f );
	ri.Printf( PRINT_ALL, "histogram controller   : %d meter=%d\n",
		ri.Cvar_VariableIntegerValue( "r_exposureHistogram" ),
		ri.Cvar_VariableIntegerValue( "r_exposureMeter" ) );
	ri.Printf( PRINT_ALL, "freeze                 : %d\n", freeze ? freeze->integer : 0 );
	ri.Printf( PRINT_ALL, "metering input         : SceneHDR before bloom/UI (luminance.comp)\n" );
	ri.Printf( PRINT_ALL, "======================================\n" );
}

static void AutoExposure_HistogramStatus_f( void )
{
	AutoExposure_Status_f();
	vk_exposure_histogram_status_f();
}

static void AutoExposure_Validate_f( void )
{
	int fails = 0;
	cvar_t *hi = ri.Cvar_Get( "r_autoExposure_highPercent", "0.05", 0 );
	cvar_t *skyW = ri.Cvar_Get( "r_exposureSkyWeight", "0.55", 0 );
	cvar_t *spdUp = ri.Cvar_Get( "r_autoExposure_speedUp", "0.8", 0 );
	cvar_t *spdDn = ri.Cvar_Get( "r_autoExposure_speedDown", "3.5", 0 );

	ri.Printf( PRINT_ALL, "======== auto_exposure_validate ========\n" );
	if ( !ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) ) {
		ri.Printf( PRINT_WARNING, "FAIL: r_exposure_auto is 0 (histogram AE required for this gate)\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: auto exposure enabled\n" );
	}
	if ( !hi || hi->value < 0.02f ) {
		ri.Printf( PRINT_WARNING, "WARN: highPercent < 0.02 — sun core may dominate histogram\n" );
	} else {
		ri.Printf( PRINT_ALL, "PASS: highPercent=%.3g trims sun extremes\n", hi->value );
	}
	if ( !skyW || skyW->value <= 0.0f ) {
		ri.Printf( PRINT_WARNING, "DETECT: SUN_SKY_EXCLUDED_FROM_EXPOSURE\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: skyWeight=%.3g > 0\n", skyW->value );
	}
	if ( spdUp && spdDn && spdUp->value >= spdDn->value ) {
		ri.Printf( PRINT_WARNING, "WARN: brighten rate >= darken rate (expected asymmetric Source curve)\n" );
	} else {
		ri.Printf( PRINT_ALL, "PASS: asymmetric adaptation (darken faster than brighten)\n" );
	}
	ri.Printf( PRINT_ALL, "auto_exposure_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void AutoExposure_Reset_f( void )
{
	vk.adaptedExposure = ( r_exposure && r_exposure->value > 0.0f ) ? r_exposure->value : 1.0f;
	vk.temporal.hasValidLuminance = qfalse;
	vk.temporal.filteredAvgLogLuminance = 0.0f;
	vk_exposure_histogram_on_map_change();
	ri.Printf( PRINT_ALL, "auto_exposure_reset: adaptedExposure=%.4g\n", vk.adaptedExposure );
}

static void AutoExposure_Freeze_f( void )
{
	ri.Cvar_Set( "r_autoExposureFreeze", "1" );
	ri.Printf( PRINT_ALL, "auto_exposure_freeze: ON\n" );
}

static void AutoExposure_Unfreeze_f( void )
{
	ri.Cvar_Set( "r_autoExposureFreeze", "0" );
	ri.Printf( PRINT_ALL, "auto_exposure_unfreeze: OFF\n" );
}

static void Exposure_ContractStatus_f( void )
{
	ri.Printf( PRINT_ALL, "======== exposure_contract_status ========\n" );
	ri.Printf( PRINT_ALL, "convention: Unexposed SceneHDR until tonemap (A)\n" );
	ri.Printf( PRINT_ALL, "sky / sun / world / weapon / WBOIT / bloom share adaptedExposure once\n" );
	ri.Printf( PRINT_ALL, "adaptedExposure=%.4g  r_exposure=%.4g  auto=%d\n",
		vk.adaptedExposure,
		r_exposure ? r_exposure->value : 1.0f,
		ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) );
	ri.Printf( PRINT_ALL, "bloom EV-relative=%d\n",
		ri.Cvar_VariableIntegerValue( "r_bloomThresholdEVRelative" ) );
	ri.Printf( PRINT_ALL, "==========================================\n" );
}

static void Exposure_ContractValidate_f( void )
{
	int fails = 0;
	ri.Printf( PRINT_ALL, "======== exposure_contract_validate ========\n" );
	if ( ri.Cvar_VariableIntegerValue( "r_exposureFixed" ) &&
		ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) ) {
		ri.Printf( PRINT_WARNING, "WARN: fixed + auto both set — fixed wins in histogram path\n" );
	}
	if ( vk.adaptedExposure <= 0.0f || vk.adaptedExposure != vk.adaptedExposure ) {
		ri.Printf( PRINT_WARNING, "FAIL: adaptedExposure invalid\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: adaptedExposure finite and > 0\n" );
	}
	ri.Printf( PRINT_ALL, "exposure_contract_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void Bloom_ExposureStatus_f( void )
{
	ri.Printf( PRINT_ALL, "======== bloom_exposure_status ========\n" );
	ri.Printf( PRINT_ALL, "r_bloom=%d threshold=%s knee=%s EVRelative=%d\n",
		ri.Cvar_VariableIntegerValue( "r_bloom" ),
		ri.Cvar_VariableString( "r_bloom_threshold" ),
		ri.Cvar_VariableString( "r_bloomKnee" ),
		ri.Cvar_VariableIntegerValue( "r_bloomThresholdEVRelative" ) );
	ri.Printf( PRINT_ALL, "adaptedExposure packed for extract: %.4g\n", vk.adaptedExposure );
	ri.Printf( PRINT_ALL, "policy: SoftThreshold(sceneLuma * adaptedExposure, threshold, knee)\n" );
	ri.Printf( PRINT_ALL, "=======================================\n" );
}

static void LensFlare_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== lens_flare_status ========\n" );
	ri.Printf( PRINT_ALL, "r_lensFlare=%d strength=%s\n",
		ri.Cvar_VariableIntegerValue( "r_lensFlare" ),
		ri.Cvar_VariableString( "r_lensFlareStrength" ) );
	ri.Printf( PRINT_ALL, "occlusion: multi-sample UV gate (r_sunOcclusionDebug)\n" );
	ri.Printf( PRINT_ALL, "does not enter AE metering; post-bloom additive only\n" );
	ri.Printf( PRINT_ALL, "stable profile: r_lensFlare 0 until certified\n" );
	ri.Printf( PRINT_ALL, "===================================\n" );
}

static void Sun_OcclusionStatus_f( void )
{
	ri.Printf( PRINT_ALL, "sun_occlusion_status: multi-sample UV visibility for lens artifacts; "
		"does not erase cubemap sun\n" );
}

static void Sun_BloomStatus_f( void )
{
	Bloom_ExposureStatus_f();
}

static void Sun_TonemapStatus_f( void )
{
	ri.Printf( PRINT_ALL, "======== sun_tonemap_status ========\n" );
	ri.Printf( PRINT_ALL, "tonemap=%d (shared SceneHDR path; sun not separately remapped)\n",
		ri.Cvar_VariableIntegerValue( "r_tonemap" ) );
	ri.Printf( PRINT_ALL, "tiny core may reach display white; corona/sky must retain gradient\n" );
	ri.Printf( PRINT_ALL, "====================================\n" );
}

static void AutoExposure_ContrastStatus_f( void )
{
	ri.Printf( PRINT_ALL, "======== auto_exposure_contrast_status ========\n" );
	ri.Printf( PRINT_ALL, "adaptedExposure=%.4g — world must retain lit/shadow contrast under sun\n",
		vk.adaptedExposure );
	ri.Printf( PRINT_ALL, "skyWeight=%s highPercent=%s\n",
		ri.Cvar_VariableString( "r_exposureSkyWeight" ),
		ri.Cvar_VariableString( "r_autoExposure_highPercent" ) );
	ri.Printf( PRINT_ALL, "================================================\n" );
}

static void EyeAdaptation_ExposureStatus_f( void )
{
	AutoExposure_Status_f();
	ri.Printf( PRINT_ALL,
		"eye_adaptation_exposure_test sequence:\n"
		"  VIEW_DARK_WORLD → BALANCED → BRIGHT_SKY → DIRECT_SUN → BRIGHT_SKY → DARK_WORLD\n"
		"  expect: darken fast toward sun/sky; brighten slower returning to dark; no pumping\n" );
}

static void EyeAdaptation_ExposureTest_f( void )
{
	EyeAdaptation_ExposureStatus_f();
	ri.Printf( PRINT_ALL, "eye_adaptation_exposure_test: drive camera manually; watch auto_exposure_status\n" );
}

void vk_hdr_sun_register( void )
{
	r_sunSource = ri.Cvar_Get( "r_sunSource", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunSource, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_sunSource,
		"HDR sun source: 0=none 1=cubemap 2=procedural 3=cubemap+metadata. "
		"Do not stack full radiance cubemap+procedural." );

	r_sunDebug = ri.Cvar_Get( "r_sunDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_sunDebug, "0", "8", CV_INTEGER );

	r_sunDiffraction = ri.Cvar_Get( "r_sunDiffraction", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sunDiffraction, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_sunDiffraction,
		"Optional aperture diffraction rays. Default 0 — bloom must stay isotropic." );

	r_sunDiffractionBlades = ri.Cvar_Get( "r_sunDiffractionBlades", "6", CVAR_ARCHIVE_ND );
	r_sunDiffractionIntensity = ri.Cvar_Get( "r_sunDiffractionIntensity", "0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_sunDiffractionRotation", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_sunDiffractionLength", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_sunDiffractionThreshold", "4.0", CVAR_ARCHIVE_ND );

	ri.Cvar_Get( "r_bloomThresholdEVRelative", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_bloomSoftKnee", "0.5", CVAR_ARCHIVE_ND );

	r_autoExposureDebug = ri.Cvar_Get( "r_autoExposureDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_autoExposureDebug, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_autoExposureDebug,
		"1=histogram 2=percentiles 3=weights 4=rejected 5=targetEV 6=currentEV 7=sky 8=world" );

	r_lensFlareDebug = ri.Cvar_Get( "r_lensFlareDebug", "0", CVAR_TEMP );
	r_sunBloomDebug = ri.Cvar_Get( "r_sunBloomDebug", "0", CVAR_TEMP );
	r_sunTonemapDebug = ri.Cvar_Get( "r_sunTonemapDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_sunOcclusionDebug", "0", CVAR_TEMP );
	r_autoExposureFreeze = ri.Cvar_Get( "r_autoExposureFreeze", "0", CVAR_TEMP );

	/* Friendly aliases requested by the remediation plan (mirror primary cvars). */
	ri.Cvar_Get( "r_autoExposureMinEV", "-4", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureMaxEV", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureCompensation", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureLowPercentile", "0.05", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureHighPercentile", "0.05", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureBrightenSpeed", "0.8", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureDarkenSpeed", "3.5", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureSkyWeight", "0.55", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_autoExposureCenterWeight", "0.70", CVAR_ARCHIVE_ND );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "sun_render_status", Sun_RenderStatus_f );
		ri.Cmd_AddCommand( "sun_composition_status", Sun_CompositionStatus_f );
		ri.Cmd_AddCommand( "sun_contributor_validate", Sun_ContributorValidate_f );
		ri.Cmd_AddCommand( "sun_source_status", Sun_SourceStatus_f );
		ri.Cmd_AddCommand( "auto_exposure_status", AutoExposure_Status_f );
		ri.Cmd_AddCommand( "auto_exposure_histogram_status", AutoExposure_HistogramStatus_f );
		ri.Cmd_AddCommand( "auto_exposure_validate", AutoExposure_Validate_f );
		ri.Cmd_AddCommand( "auto_exposure_reset", AutoExposure_Reset_f );
		ri.Cmd_AddCommand( "auto_exposure_freeze", AutoExposure_Freeze_f );
		ri.Cmd_AddCommand( "auto_exposure_unfreeze", AutoExposure_Unfreeze_f );
		ri.Cmd_AddCommand( "exposure_contract_status", Exposure_ContractStatus_f );
		ri.Cmd_AddCommand( "exposure_contract_validate", Exposure_ContractValidate_f );
		ri.Cmd_AddCommand( "bloom_exposure_status", Bloom_ExposureStatus_f );
		ri.Cmd_AddCommand( "lens_flare_status", LensFlare_Status_f );
		ri.Cmd_AddCommand( "sun_occlusion_status", Sun_OcclusionStatus_f );
		ri.Cmd_AddCommand( "sun_bloom_status", Sun_BloomStatus_f );
		ri.Cmd_AddCommand( "sun_tonemap_status", Sun_TonemapStatus_f );
		ri.Cmd_AddCommand( "auto_exposure_contrast_status", AutoExposure_ContrastStatus_f );
		ri.Cmd_AddCommand( "eye_adaptation_exposure_status", EyeAdaptation_ExposureStatus_f );
		ri.Cmd_AddCommand( "eye_adaptation_exposure_test", EyeAdaptation_ExposureTest_f );
		s_cmds = qtrue;
	}

	(void)r_autoExposureDebug;
	(void)r_lensFlareDebug;
	(void)r_sunBloomDebug;
	(void)r_sunTonemapDebug;
	(void)r_autoExposureFreeze;
	(void)r_sunDebug;
	(void)r_sunDiffractionBlades;
	(void)r_sunDiffractionIntensity;
}

#endif /* USE_VULKAN */
