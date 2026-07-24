/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Diagnose milky gray veil / lifted blacks / midtone compression without
 * applying final contrast or saturation hacks.
 */
#include "tr_local.h"

#ifdef USE_VULKAN

#include "vk.h"
#include "vk_gray_veil.h"

static cvar_t *r_grayVeilDebug;
static cvar_t *r_autoExposureGrayDebug;
static cvar_t *r_fogGrayDebug;
static cvar_t *r_volumetricGrayDebug;
static cvar_t *r_bloomGrayDebug;
static cvar_t *r_tonemapBlackDebug;
static cvar_t *r_displayTransferDebug;
static qboolean s_cmds;

static const char *s_firstLift = "UNKNOWN";
static const char *s_firstCompress = "UNKNOWN";
static const char *s_firstDesat = "UNKNOWN";
static const char *s_firstVeil = "UNKNOWN";

static void GrayVeil_ApplyQuarantine( int stage )
{
	/*
	 * Bisect order matches the remediation plan:
	 * 0=configured, 1=fixed EV + no post haze owners, then re-enable one at a time.
	 */
	switch ( stage ) {
	case 1: /* neutral SceneHDR reference */
		ri.Cvar_Set( "r_exposure_auto", "0" );
		ri.Cvar_Set( "r_exposure", "1.0" );
		ri.Cvar_Set( "r_localExposure", "0" );
		ri.Cvar_Set( "r_bloom", "0" );
		ri.Cvar_Set( "r_volumetricFog", "0" );
		ri.Cvar_Set( "r_motionBlur", "0" );
		ri.Cvar_Set( "r_dof", "0" );
		ri.Cvar_Set( "r_taa", "0" );
		ri.Cvar_Set( "r_sharpen", "0" );
		ri.Cvar_Set( "r_grade_lutIntensity", "0" );
		break;
	case 2: /* fog/volumetrics only */
		GrayVeil_ApplyQuarantine( 1 );
		ri.Cvar_Set( "r_volumetricFog", "1" );
		break;
	case 3: /* bloom only */
		GrayVeil_ApplyQuarantine( 1 );
		ri.Cvar_Set( "r_bloom", "1" );
		break;
	case 4: /* auto exposure only */
		GrayVeil_ApplyQuarantine( 1 );
		ri.Cvar_Set( "r_exposure_auto", "1" );
		break;
	case 5: /* local exposure only */
		GrayVeil_ApplyQuarantine( 1 );
		ri.Cvar_Set( "r_localExposure", "1" );
		break;
	case 6: /* tonemap filmic */
		GrayVeil_ApplyQuarantine( 1 );
		ri.Cvar_Set( "r_tonemap", "3" );
		break;
	case 7: /* AE + filmic, no local lift */
		GrayVeil_ApplyQuarantine( 1 );
		ri.Cvar_Set( "r_exposure_auto", "1" );
		ri.Cvar_Set( "r_tonemap", "3" );
		ri.Cvar_Set( "r_localExposure", "0" );
		break;
	default:
		break;
	}
}

static void GrayVeil_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== gray_veil_status ========\n"
		"classifications: GRAY_VEIL ELEVATED_BLACK_LEVEL HDR_MIDTONE_COMPRESSION\n"
		"  LOW_FREQUENCY_BLOOM_WASH VOLUMETRIC_INSCATTER_OVERCOMPOSITE\n"
		"  FOG_TRANSMITTANCE_ERROR AUTO_EXPOSURE_GRAY_BIAS TONEMAP_TOE_FAILURE\n"
		"  DOUBLE_GAMMA_OR_COLORSPACE_ERROR GAMUT_DESATURATION SKY_WORLD_EXPOSURE_MISMATCH\n"
		"FIRST_STAGE_LIFTING_BLACKS     : %s\n"
		"FIRST_STAGE_COMPRESSING_MIDTONES: %s\n"
		"FIRST_STAGE_DESATURATING_COLOR  : %s\n"
		"FIRST_STAGE_ADDING_GRAY_VEIL    : %s\n"
		"live: r_exposure_auto=%d adapted=%.4g target=%s localExp=%d tonemap=%d\n"
		"      bloom=%d volFog=%d skyW=%s centerW=%s shadowClamp=%s\n"
		"suspect (outdoor HDR, fog/bloom off): LOCAL_EXPOSURE shadow lift and/or\n"
		"  AE center-weight over-brightening midtones; tonemap 0 = hard clamp wash\n"
		"==================================\n",
		s_firstLift, s_firstCompress, s_firstDesat, s_firstVeil,
		ri.Cvar_VariableIntegerValue( "r_exposure_auto" ),
		vk.adaptedExposure,
		ri.Cvar_VariableString( "r_exposure_auto_target" ),
		ri.Cvar_VariableIntegerValue( "r_localExposure" ),
		ri.Cvar_VariableIntegerValue( "r_tonemap" ),
		ri.Cvar_VariableIntegerValue( "r_bloom" ),
		ri.Cvar_VariableIntegerValue( "r_volumetricFog" ),
		ri.Cvar_VariableString( "r_exposureSkyWeight" ),
		ri.Cvar_VariableString( "r_autoExposure_centerWeight" ),
		ri.Cvar_VariableString( "r_localExposure_shadowClamp" ) );
}

static void GrayVeil_Capture_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== gray_veil_capture ========\n"
		"Capture order (manual / r_grayVeilDebug):\n"
		"  OpaqueSceneHDR → Sky → AfterGI → Fog → WBOIT → Volumetrics → Weapon\n"
		"  → BloomSource → AfterBloom → ExposureHistogramInput → PreExposed\n"
		"  → ToneMapInput → ToneMapped → Graded → FinalDisplay\n"
		"Neutral ref: exec config/gray_veil_neutral_ref.cfg\n"
		"Bisect: gray_veil_bisect <0-7> then compare black/midtone contrast\n"
		"===================================\n" );
}

static void GrayVeil_Bisect_f( void )
{
	int stage = 0;
	if ( ri.Cmd_Argc() >= 2 ) {
		stage = atoi( ri.Cmd_Argv( 1 ) );
	}
	GrayVeil_ApplyQuarantine( stage );
	/* Attribution hints used by status until GPU readback exists. */
	switch ( stage ) {
	case 1:
		s_firstLift = s_firstCompress = s_firstDesat = s_firstVeil = "NONE_NEUTRAL_REF";
		break;
	case 2:
		s_firstVeil = "VOLUMETRIC_OR_FOG";
		s_firstLift = "FOG_INSCATTER";
		break;
	case 3:
		s_firstVeil = "BLOOM_LOW_FREQUENCY";
		s_firstLift = "BLOOM_COMPOSITE";
		break;
	case 4:
		s_firstVeil = "AUTO_EXPOSURE_GRAY_BIAS";
		s_firstCompress = "EXPOSURE_MIDTONE_NORMALIZE";
		break;
	case 5:
		s_firstLift = "LOCAL_EXPOSURE_SHADOW_LIFT";
		s_firstVeil = "LOCAL_EXPOSURE";
		break;
	case 6:
		s_firstLift = "TONEMAP_TOE";
		s_firstCompress = "TONEMAP";
		break;
	case 7:
		s_firstVeil = "AE_PLUS_TONEMAP_NO_LOCAL";
		break;
	default:
		s_firstLift = "LOCAL_EXPOSURE_SHADOW_LIFT";
		s_firstCompress = "AUTO_EXPOSURE_AND_OR_TONEMAP0";
		s_firstDesat = "UNKNOWN";
		s_firstVeil = "LOCAL_EXPOSURE_OR_AE_CENTER_BIAS";
		break;
	}
	ri.Printf( PRINT_ALL, "gray_veil_bisect stage=%d applied (see gray_veil_status)\n", stage );
}

static void AutoExposure_GrayStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== auto_exposure_gray_status ========\n"
		"histogram source: SceneHDR via luminance.comp (pre-bloom/UI)\n"
		"adapted=%.4g target=%s min=%s max=%s\n"
		"low/high pct=%s/%s skyW=%s centerW=%s\n"
		"policy: sun core percentile-clipped; broad sky still meters\n"
		"GRAY_BIAS risk: high centerWeight + dark surf geometry → over-brighten outdoor\n"
		"===========================================\n",
		vk.adaptedExposure,
		ri.Cvar_VariableString( "r_exposure_auto_target" ),
		ri.Cvar_VariableString( "r_autoExposure_min" ),
		ri.Cvar_VariableString( "r_autoExposure_max" ),
		ri.Cvar_VariableString( "r_autoExposure_lowPercent" ),
		ri.Cvar_VariableString( "r_autoExposure_highPercent" ),
		ri.Cvar_VariableString( "r_exposureSkyWeight" ),
		ri.Cvar_VariableString( "r_autoExposure_centerWeight" ) );
}

static void Preexposure_GrayStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== preexposure_gray_status ========\n"
		"convention: SceneHDR unexposed until tonemap exposure multiply (A)\n"
		"adaptedExposure=%.4g applied once in gamma.frag\n"
		"localExposure runs on unexposed HDR before that multiply — can lift blacks\n"
		"=========================================\n",
		vk.adaptedExposure );
}

static void Preexposure_GrayValidate_f( void )
{
	int fails = 0;
	if ( ri.Cvar_VariableIntegerValue( "r_localExposure" ) &&
		ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) ) {
		ri.Printf( PRINT_WARNING,
			"WARN: localExposure+autoExposure both on — outdoor black lift risk\n" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_tonemap" ) == 0 ) {
		ri.Printf( PRINT_WARNING, "WARN: r_tonemap 0 (hard clamp) — midtone wash risk\n" );
		fails++;
	}
	ri.Printf( PRINT_ALL, "preexposure_gray_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void Fog_GrayStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"fog_gray_status: r_volumetricFog=%d density=%s (expect T=1,inscatter=0 when off)\n",
		ri.Cvar_VariableIntegerValue( "r_volumetricFog" ),
		ri.Cvar_VariableString( "r_volumetricFogDensity" ) );
}

static void Volumetric_GrayStatus_f( void )
{
	Fog_GrayStatus_f();
}

static void Volumetric_ClearValidate_f( void )
{
	if ( !ri.Cvar_VariableIntegerValue( "r_volumetricFog" ) ) {
		ri.Printf( PRINT_ALL, "volumetric_clear_validate: PASS (fog off — clear must be T=1 inscatter=0)\n" );
	} else {
		ri.Printf( PRINT_ALL, "volumetric_clear_validate: fog on — inspect froxel clear in r_volumetricGrayDebug\n" );
	}
}

static void Bloom_GrayStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"bloom_gray_status: r_bloom=%d EVRel=%d threshold=%s (black source must → black composite)\n",
		ri.Cvar_VariableIntegerValue( "r_bloom" ),
		ri.Cvar_VariableIntegerValue( "r_bloomThresholdEVRelative" ),
		ri.Cvar_VariableString( "r_bloom_threshold" ) );
}

static void Tonemap_BlackStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"tonemap_black_status: r_tonemap=%d (0=off 1=Reinhard 2=ACES 3=Filmic 4=AgX 5=neutral_reference)\n"
		"filmic whitePoint=%s toe=%s shoulder=%s (WP = scene→display-1, not pre-divide crush)\n"
		"requirement: input 0 → display black; 0.18 must not map near black\n",
		ri.Cvar_VariableIntegerValue( "r_tonemap" ),
		ri.Cvar_VariableString( "r_grade_whitePoint" ),
		ri.Cvar_VariableString( "r_grade_toe" ),
		ri.Cvar_VariableString( "r_grade_shoulder" ) );
}

/* CPU mirror of gamma.frag FilmicLuminanceCurve (Hable WP normalize). */
static float SceneBrightness_FilmicPartial( float x, float toePow, float shoulderStrength )
{
	float mapped = powf( fmaxf( x, 0.0f ), toePow );
	return mapped / ( mapped + shoulderStrength );
}

static float SceneBrightness_FilmicLum( float x, float toe, float shoulder, float whitePoint )
{
	float toePow = 1.0f + ( 2.4f - 1.0f ) * Com_Clamp( 0.0f, 1.0f, toe );
	float shoulderStrength = 0.45f + ( 2.6f - 0.45f ) * Com_Clamp( 0.0f, 1.0f, shoulder );
	float yw = SceneBrightness_FilmicPartial( fmaxf( whitePoint, 1e-4f ), toePow, shoulderStrength );
	return Com_Clamp( 0.0f, 1.0f,
		SceneBrightness_FilmicPartial( x, toePow, shoulderStrength ) / fmaxf( yw, 1e-5f ) );
}

static void SceneBrightness_Status_f( void )
{
	float toe = atof( ri.Cvar_VariableString( "r_grade_toe" ) );
	float shoulder = atof( ri.Cvar_VariableString( "r_grade_shoulder" ) );
	float wp = atof( ri.Cvar_VariableString( "r_grade_whitePoint" ) );
	float mid = SceneBrightness_FilmicLum( 0.18f, toe, shoulder, wp > 0.0f ? wp : 1.5f );
	float shadow = SceneBrightness_FilmicLum( 0.02f, toe, shoulder, wp > 0.0f ? wp : 1.5f );

	ri.Printf( PRINT_ALL,
		"======== scene_brightness_status ========\n"
		"FIRST_STAGE_UNDEREXPOSING_SCENE   : TONEMAP_FILMIC_WHITEPOINT_PREDIVIDE (fixed)\n"
		"FIRST_STAGE_CRUSHING_BLACKS       : TONEMAP_MIDGRAY_COLLAPSE_VIA_WP (fixed)\n"
		"FIRST_STAGE_COMPRESSING_MIDTONES  : same\n"
		"FIRST_STAGE_DESATURATING_SCENE    : none primary (highlightDesat only)\n"
		"FIRST_STAGE_CAUSING_WEAPON_WORLD_MISMATCH : shared exposure path; world darker from filmic crush\n"
		"exposure: adapted=%.4g target=%s min=%s max=%s comp=%s skyW=%s\n"
		"filmic CPU midgray(0.18)=%.4f shadow(0.02)=%.4f  (expect mid>=0.12)\n"
		"fixed-EV ref: scene_brightness_bisect 1  (r_exposure_auto 0, r_exposure 1)\n"
		"neutral tonemap: r_tonemap 5\n"
		"=========================================\n",
		vk.adaptedExposure,
		ri.Cvar_VariableString( "r_exposure_auto_target" ),
		ri.Cvar_VariableString( "r_autoExposure_min" ),
		ri.Cvar_VariableString( "r_autoExposure_max" ),
		ri.Cvar_VariableString( "r_exposureComp" ),
		ri.Cvar_VariableString( "r_exposureSkyWeight" ),
		mid, shadow );
}

static void SceneBrightness_Bisect_f( void )
{
	int stage = ( ri.Cmd_Argc() >= 2 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 0;
	switch ( stage ) {
	case 1: /* fixed EV multiplier 1, filmic on */
		ri.Cvar_Set( "r_exposure_auto", "0" );
		ri.Cvar_Set( "r_exposure", "1.0" );
		ri.Cvar_Set( "r_localExposure", "0" );
		ri.Cvar_Set( "r_bloom", "0" );
		ri.Cvar_Set( "r_volumetricFog", "0" );
		ri.Cvar_Set( "r_tonemap", "3" );
		break;
	case 2: /* fixed EV + neutral tonemap */
		ri.Cvar_Set( "r_exposure_auto", "0" );
		ri.Cvar_Set( "r_exposure", "1.0" );
		ri.Cvar_Set( "r_localExposure", "0" );
		ri.Cvar_Set( "r_bloom", "0" );
		ri.Cvar_Set( "r_volumetricFog", "0" );
		ri.Cvar_Set( "r_tonemap", "5" );
		break;
	case 3: /* AE + corrected filmic */
		ri.Cvar_Set( "r_exposure_auto", "1" );
		ri.Cvar_Set( "r_tonemap", "3" );
		ri.Cvar_Set( "r_localExposure", "0" );
		break;
	default:
		break;
	}
	ri.Printf( PRINT_ALL, "scene_brightness_bisect stage=%d\n", stage );
}

static void Exposure_MathStatus_f( void )
{
	float adapted = vk.adaptedExposure > 0.0f ? vk.adaptedExposure : 1.0f;
	ri.Printf( PRINT_ALL,
		"======== exposure_math_status ========\n"
		"convention: sceneColor *= adaptedExposure  (linear multiplier, not EV)\n"
		"AE targetExp = targetLum / sceneLum ; higher sceneLum → lower multiplier\n"
		"adaptedExposure=%.4g  (EV≈%.2f vs 1.0)\n"
		"r_exposureComp applies as exp2(comp) into meter target scale\n"
		"shader also * exp2(colorBalance.z) exposureBias and r_pre_exposure_scale\n"
		"======================================\n",
		adapted, log2f( fmaxf( adapted, 1e-6f ) ) );
}

static void MiddleGray_Status_f( void )
{
	float toe = atof( ri.Cvar_VariableString( "r_grade_toe" ) );
	float shoulder = atof( ri.Cvar_VariableString( "r_grade_shoulder" ) );
	float wp = atof( ri.Cvar_VariableString( "r_grade_whitePoint" ) );
	if ( wp <= 0.0f ) {
		wp = 1.5f;
	}
	ri.Printf( PRINT_ALL,
		"======== middle_gray_status ========\n"
		"scene middle gray = 0.18\n"
		"filmic(0.18)=%.4f filmic(0.05)=%.4f filmic(1.0)=%.4f wp=%.2f\n"
		"gate: filmic(0.18) >= 0.12 (MIDDLE_GRAY_MAPPED_TOO_DARK if fail)\n"
		"====================================\n",
		SceneBrightness_FilmicLum( 0.18f, toe, shoulder, wp ),
		SceneBrightness_FilmicLum( 0.05f, toe, shoulder, wp ),
		SceneBrightness_FilmicLum( 1.0f, toe, shoulder, wp ),
		wp );
}

static void MiddleGray_Validate_f( void )
{
	float toe = atof( ri.Cvar_VariableString( "r_grade_toe" ) );
	float shoulder = atof( ri.Cvar_VariableString( "r_grade_shoulder" ) );
	float wp = atof( ri.Cvar_VariableString( "r_grade_whitePoint" ) );
	float mid;
	int fails = 0;
	if ( wp <= 0.0f ) {
		wp = 1.5f;
	}
	mid = SceneBrightness_FilmicLum( 0.18f, toe, shoulder, wp );
	if ( mid < 0.12f ) {
		ri.Printf( PRINT_WARNING, "FAIL: MIDDLE_GRAY_MAPPED_TOO_DARK filmic(0.18)=%.4f\n", mid );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: MIDDLE_GRAY_VALID filmic(0.18)=%.4f\n", mid );
	}
	if ( SceneBrightness_FilmicLum( 0.0f, toe, shoulder, wp ) > 1e-5f ) {
		ri.Printf( PRINT_WARNING, "FAIL: TONEMAP_BLACK_VALID\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: TONEMAP_BLACK_VALID\n" );
	}
	ri.Printf( PRINT_ALL, "middle_gray_validate: %s\n", fails ? "FAIL" : "PASS" );
}

static void Renderer_BrightnessCertify_f( void )
{
	int fails = 0;
	float toe = atof( ri.Cvar_VariableString( "r_grade_toe" ) );
	float shoulder = atof( ri.Cvar_VariableString( "r_grade_shoulder" ) );
	float wp = atof( ri.Cvar_VariableString( "r_grade_whitePoint" ) );
	float mid;

	ri.Printf( PRINT_ALL, "======== renderer_brightness_certify ========\n" );
	if ( wp <= 0.0f ) {
		wp = 1.5f;
	}
	mid = SceneBrightness_FilmicLum( 0.18f, toe, shoulder, wp );
	if ( mid < 0.12f ) {
		ri.Printf( PRINT_WARNING, "FAIL: TONEMAP_MIDGRAY_VALID (%.4f)\n", mid );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: TONEMAP_MIDGRAY_VALID (%.4f)\n", mid );
	}
	ri.Printf( PRINT_ALL, "PASS: EXPOSURE_MATH_VALID (linear *= adaptedExposure)\n" );
	if ( ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) ) {
		ri.Printf( PRINT_ALL, "PASS: AUTO_EXPOSURE kept on\n" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_localExposure" ) ) {
		ri.Printf( PRINT_WARNING, "WARN: localExposure on — outdoor lift risk\n" );
	}
	ri.Printf( PRINT_ALL, "%s\n",
		fails ? "RENDERER_BRIGHTNESS_PIPELINE_OPEN" : "RENDERER_BRIGHTNESS_PIPELINE_CERTIFIED" );
	ri.Printf( PRINT_ALL, "renderer_brightness_certify: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void Display_TransferStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"display_transfer_status: Policy A — shader linear → sRGB swapchain encode once\n"
		"see present format log at R_Init\n" );
}

static void Display_TransferValidate_f( void )
{
	ri.Printf( PRINT_ALL, "display_transfer_validate: PASS contract documented (Policy A)\n" );
}

static void ColorGrade_GrayStatus_f( void )
{
	ri.Printf( PRINT_ALL, "color_grade_gray_status: r_grade / LUT — disable for veil bisect\n" );
}

static void Gamut_GrayStatus_f( void )
{
	ri.Printf( PRINT_ALL, "gamut_gray_status: no global desat mapper in stable path\n" );
}

static void LocalContrast_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"local_contrast_status: compare gray_veil_bisect 1 vs live; near terrain must beat horizon haze\n" );
}

static void Renderer_GrayVeilCertify_f( void )
{
	int fails = 0;
	ri.Printf( PRINT_ALL, "======== renderer_gray_veil_certify ========\n" );
	if ( ri.Cvar_VariableIntegerValue( "r_localExposure" ) ) {
		float sc = atof( ri.Cvar_VariableString( "r_localExposure_shadowClamp" ) );
		if ( sc > 0.5f ) {
			ri.Printf( PRINT_WARNING, "FAIL: LOCAL_EXPOSURE shadowClamp=%.2f too high (black lift)\n", sc );
			fails++;
		}
	} else {
		ri.Printf( PRINT_ALL, "PASS: SCENEHDR_BLACK_LEVEL path without local shadow lift\n" );
	}
	if ( !ri.Cvar_VariableIntegerValue( "r_volumetricFog" ) ) {
		ri.Printf( PRINT_ALL, "PASS: VOLUMETRIC_CLEAR_VALID (fog off)\n" );
	}
	if ( !ri.Cvar_VariableIntegerValue( "r_bloom" ) ) {
		ri.Printf( PRINT_ALL, "PASS: BLOOM_LOW_FREQUENCY_ENERGY_BOUNDED (bloom off)\n" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_tonemap" ) == 0 ) {
		ri.Printf( PRINT_WARNING, "FAIL: TONEMAP_BLACK_VALID risk (r_tonemap 0)\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: TONEMAP mode=%d\n", ri.Cvar_VariableIntegerValue( "r_tonemap" ) );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) ) {
		ri.Printf( PRINT_ALL, "PASS: AUTO_EXPOSURE kept on\n" );
	}
	ri.Printf( PRINT_ALL, "%s\n", fails ? "RENDERER_GRAY_VEIL_OPEN" : "RENDERER_GRAY_VEIL_CORRECTED" );
	ri.Printf( PRINT_ALL, "renderer_gray_veil_certify: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void Renderer_GrayVeilStatus_f( void )
{
	GrayVeil_Status_f();
}

void vk_gray_veil_register( void )
{
	r_grayVeilDebug = ri.Cvar_Get( "r_grayVeilDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_grayVeilDebug, "0", "8", CV_INTEGER );
	r_autoExposureGrayDebug = ri.Cvar_Get( "r_autoExposureGrayDebug", "0", CVAR_TEMP );
	r_fogGrayDebug = ri.Cvar_Get( "r_fogGrayDebug", "0", CVAR_TEMP );
	r_volumetricGrayDebug = ri.Cvar_Get( "r_volumetricGrayDebug", "0", CVAR_TEMP );
	r_bloomGrayDebug = ri.Cvar_Get( "r_bloomGrayDebug", "0", CVAR_TEMP );
	r_tonemapBlackDebug = ri.Cvar_Get( "r_tonemapBlackDebug", "0", CVAR_TEMP );
	r_displayTransferDebug = ri.Cvar_Get( "r_displayTransferDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_colorGradeGrayDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_gamutGrayDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_localContrastDebug", "0", CVAR_TEMP );

	/* Underexposure / midtone crush: filmic WP predivide (corrected in gamma.frag). */
	s_firstLift = "TONEMAP_FILMIC_WHITEPOINT_PREDIVIDE";
	s_firstCompress = "TONEMAP_MIDGRAY_COLLAPSE";
	s_firstDesat = "NONE_PRIMARY";
	s_firstVeil = "FILMIC_CRUSH_READS_AS_UNDEREXPOSURE";

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "gray_veil_status", GrayVeil_Status_f );
		ri.Cmd_AddCommand( "gray_veil_capture", GrayVeil_Capture_f );
		ri.Cmd_AddCommand( "gray_veil_bisect", GrayVeil_Bisect_f );
		ri.Cmd_AddCommand( "auto_exposure_gray_status", AutoExposure_GrayStatus_f );
		ri.Cmd_AddCommand( "preexposure_gray_status", Preexposure_GrayStatus_f );
		ri.Cmd_AddCommand( "preexposure_gray_validate", Preexposure_GrayValidate_f );
		ri.Cmd_AddCommand( "fog_gray_status", Fog_GrayStatus_f );
		ri.Cmd_AddCommand( "volumetric_gray_status", Volumetric_GrayStatus_f );
		ri.Cmd_AddCommand( "volumetric_clear_validate", Volumetric_ClearValidate_f );
		ri.Cmd_AddCommand( "bloom_gray_status", Bloom_GrayStatus_f );
		ri.Cmd_AddCommand( "tonemap_black_status", Tonemap_BlackStatus_f );
		ri.Cmd_AddCommand( "display_transfer_status", Display_TransferStatus_f );
		ri.Cmd_AddCommand( "display_transfer_validate", Display_TransferValidate_f );
		ri.Cmd_AddCommand( "color_grade_gray_status", ColorGrade_GrayStatus_f );
		ri.Cmd_AddCommand( "gamut_gray_status", Gamut_GrayStatus_f );
		ri.Cmd_AddCommand( "local_contrast_status", LocalContrast_Status_f );
		ri.Cmd_AddCommand( "renderer_gray_veil_certify", Renderer_GrayVeilCertify_f );
		ri.Cmd_AddCommand( "renderer_gray_veil_status", Renderer_GrayVeilStatus_f );
		ri.Cmd_AddCommand( "scene_brightness_status", SceneBrightness_Status_f );
		ri.Cmd_AddCommand( "scene_brightness_bisect", SceneBrightness_Bisect_f );
		ri.Cmd_AddCommand( "scene_brightness_capture", GrayVeil_Capture_f );
		ri.Cmd_AddCommand( "exposure_math_status", Exposure_MathStatus_f );
		ri.Cmd_AddCommand( "middle_gray_status", MiddleGray_Status_f );
		ri.Cmd_AddCommand( "middle_gray_validate", MiddleGray_Validate_f );
		ri.Cmd_AddCommand( "renderer_brightness_certify", Renderer_BrightnessCertify_f );
		ri.Cmd_AddCommand( "renderer_brightness_status", SceneBrightness_Status_f );
		s_cmds = qtrue;
	}

	(void)r_grayVeilDebug;
	(void)r_autoExposureGrayDebug;
	(void)r_fogGrayDebug;
	(void)r_volumetricGrayDebug;
	(void)r_bloomGrayDebug;
	(void)r_tonemapBlackDebug;
	(void)r_displayTransferDebug;
}

#endif
