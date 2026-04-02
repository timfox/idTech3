#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_postfx_sanitize.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include <math.h>

void vk_update_postfx_params( uint32_t cmd_index )
{
	VkPostFXParams params;
	const float *projection;
	const float *view;
	float viewProj[16];
	qboolean motion_valid = qfalse;
	vec3_t shadowLift, midGamma, highlightGain, splitShadow, splitHighlight;
	cvar_t *r_post_contrast;
	cvar_t *r_post_saturation;
	cvar_t *r_outline;
	cvar_t *r_outlineThreshold;
	cvar_t *r_autoExposure_target;
	cvar_t *r_autoExposure_min;
	cvar_t *r_autoExposure_max;
	cvar_t *r_localExposure;
	cvar_t *r_localExposure_strength;
	cvar_t *r_localExposure_shadowClamp;
	cvar_t *r_localExposure_highlightClamp;

	if ( cmd_index >= NUM_COMMAND_BUFFERS || !vk.postfx_params_ptr[cmd_index] ) {
		return;
	}

	Com_Memset( &params, 0, sizeof( params ) );

	params.invViewProj[0] = 1.0f;
	params.invViewProj[5] = 1.0f;
	params.invViewProj[10] = 1.0f;
	params.invViewProj[15] = 1.0f;
	params.prevViewProj[0] = 1.0f;
	params.prevViewProj[5] = 1.0f;
	params.prevViewProj[10] = 1.0f;
	params.prevViewProj[15] = 1.0f;
	params.viewMatrix[0] = 1.0f;
	params.viewMatrix[5] = 1.0f;
	params.viewMatrix[10] = 1.0f;
	params.viewMatrix[15] = 1.0f;

	params.motionBlur[0] = PostFX_MotionBlur_IsEnabled() ? 1.0f : 0.0f;
	params.motionBlur[1] = PostFX_MotionBlur_GetStrength();
	params.motionBlur[2] = (float)Com_Clamp( 4, 32, PostFX_MotionBlur_GetSamples() );
	params.motionBlur[3] = Com_Clamp( 0.0f, 64.0f, PostFX_MotionBlur_GetMaxRadius() );
	params.depthOfField[0] = PostFX_DepthOfField_IsEnabled() ? 1.0f : 0.0f;
	params.depthOfField[1] = Com_Clamp( 0.0f, 8.0f, PostFX_DepthOfField_GetAperture() );
	params.depthOfField[2] = fmaxf( PostFX_DepthOfField_GetFocusDistance(), 0.0f );
	params.depthOfField[3] = fmaxf( PostFX_DepthOfField_GetFocusRange(), 1.0f );
	params.frameInfo[0] = Com_Clamp( 0.0f, 64.0f, PostFX_DepthOfField_GetMaxBlur() );
	params.frameInfo[1] = ( vk.renderWidth > 0 ) ? ( 1.0f / (float)vk.renderWidth ) : 0.0f;
	params.frameInfo[2] = ( vk.renderHeight > 0 ) ? ( 1.0f / (float)vk.renderHeight ) : 0.0f;
	params.toneMapParams0[0] = Com_Clamp( 0.0f, 1.0f, PostFX_GetGradeToe() );
	params.toneMapParams0[1] = Com_Clamp( 0.0f, 1.0f, PostFX_GetGradeShoulder() );
	params.toneMapParams0[2] = Com_Clamp( 0.5f, 32.0f, PostFX_GetGradeWhitePoint() );
	params.toneMapParams0[3] = Com_Clamp( 0.0f, 0.25f, PostFX_GetGradeBlackClip() );
	params.toneMapParams1[0] = Com_Clamp( 0.0f, 1.0f, PostFX_GetGradeHighlightDesat() );
	params.toneMapParams1[1] = Com_Clamp( 0.25f, 4.0f, PostFX_GetGradeContrast() );
	params.toneMapParams1[2] = Com_Clamp( 0.1f, 0.9f, PostFX_GetGradeContrastPivot() );
	params.toneMapParams1[3] = (float)( r_tonemap ? r_tonemap->integer : 3 );
	params.colorBalance[0] = Com_Clamp( -1.0f, 1.0f, PostFX_GetGradeTemperature() );
	params.colorBalance[1] = Com_Clamp( -1.0f, 1.0f, PostFX_GetGradeTint() );
	params.colorBalance[2] = Com_Clamp( -4.0f, 4.0f, PostFX_GetGradeExposureBias() );
	params.colorBalance[3] = ( r_pre_exposure_scale && r_pre_exposure_scale->value > 0.0f ) ? r_pre_exposure_scale->value : 1.0f;
	r_post_contrast = ri.Cvar_Get( "r_post_contrast", "1.0", 0 );
	r_post_saturation = ri.Cvar_Get( "r_post_saturation", "1.0", 0 );
	r_outline = ri.Cvar_Get( "r_outline", "0", 0 );
	r_outlineThreshold = ri.Cvar_Get( "r_outlineThreshold", "0.15", 0 );
	params.colorGrade[0] = Com_Clamp( 0.0f, 3.0f, PostFX_GetGradeSaturation() );
	params.colorGrade[1] = Com_Clamp( -1.0f, 1.0f, PostFX_GetGradeVibrance() );
	params.colorGrade[2] = ( r_post_contrast && r_post_contrast->value > 0.0f ) ? r_post_contrast->value : 1.0f;
	params.colorGrade[3] = ( r_post_saturation && r_post_saturation->value >= 0.0f ) ? r_post_saturation->value : 1.0f;
	PostFX_GetShadowLift( shadowLift );
	PostFX_GetMidGamma( midGamma );
	PostFX_GetHighlightGain( highlightGain );
	PostFX_GetSplitShadow( splitShadow );
	PostFX_GetSplitHighlight( splitHighlight );
	params.shadowsLift[0] = shadowLift[0];
	params.shadowsLift[1] = shadowLift[1];
	params.shadowsLift[2] = shadowLift[2];
	params.midsGamma[0] = midGamma[0];
	params.midsGamma[1] = midGamma[1];
	params.midsGamma[2] = midGamma[2];
	params.highlightsGain[0] = highlightGain[0];
	params.highlightsGain[1] = highlightGain[1];
	params.highlightsGain[2] = highlightGain[2];
	params.splitShadow[0] = splitShadow[0];
	params.splitShadow[1] = splitShadow[1];
	params.splitShadow[2] = splitShadow[2];
	params.splitShadow[3] = Com_Clamp( 0.0f, 1.0f, PostFX_GetSplitBalance() );
	params.splitHighlight[0] = splitHighlight[0];
	params.splitHighlight[1] = splitHighlight[1];
	params.splitHighlight[2] = splitHighlight[2];
	params.splitHighlight[3] = Com_Clamp( 0.0f, 1.0f, PostFX_GetSplitStrength() );
	params.lensEffects0[0] = PostFX_GetVignetteIntensity();
	params.lensEffects0[1] = PostFX_GetVignetteRadius();
	params.lensEffects0[2] = PostFX_GetChromaticAberration();
	params.lensEffects0[3] = PostFX_GetFilmGrain();
	params.lensEffects1[0] = r_outline ? r_outline->value : 0.0f;
	params.lensEffects1[1] = r_outlineThreshold ? r_outlineThreshold->value : 0.15f;
	params.lensEffects1[2] = (float)PostFX_GetFilmLook();
	params.lensEffects1[3] = PostFX_GetSharpen();
	params.runtimeFlags[0] = r_greyscale ? r_greyscale->value : 0.0f;
	params.runtimeFlags[1] = (float)( r_dither ? r_dither->integer : 0 );
	params.runtimeFlags[2] = (float)( r_post_debug ? r_post_debug->integer : 0 );
	params.runtimeFlags[3] = (float)( ( r_post && r_post->integer ) ? 1 : 0 );
	params.lutParams[0] = Com_Clamp( 0.0f, 1.0f, PostFX_GetLUTIntensity() );
	params.lutParams[1] = ( PostFX_GetLUTImage() && PostFX_GetLUTImage() != tr.whiteImage ) ? 1.0f : 0.0f;
	params.lutParams[2] = 32.0f;
	params.lutParams[3] = ( r_gamma && r_gamma->value > 0.0f ) ? ( 1.0f / r_gamma->value ) : 1.0f;
	r_autoExposure_target = ri.Cvar_Get( "r_exposure_auto_target", "1.0", 0 );
	r_autoExposure_min = ri.Cvar_Get( "r_autoExposure_min", "0.5", 0 );
	r_autoExposure_max = ri.Cvar_Get( "r_autoExposure_max", "4.0", 0 );
	r_localExposure = ri.Cvar_Get( "r_localExposure", "1", 0 );
	r_localExposure_strength = ri.Cvar_Get( "r_localExposure_strength", "0.35", 0 );
	r_localExposure_shadowClamp = ri.Cvar_Get( "r_localExposure_shadowClamp", "1.5", 0 );
	r_localExposure_highlightClamp = ri.Cvar_Get( "r_localExposure_highlightClamp", "1.5", 0 );
	vk_postfx_sanitize_auto_exposure_params(
		vk.temporal.hasValidLuminance ? 1 : 0,
		vk.temporal.filteredAvgLogLuminance,
		r_autoExposure_target ? r_autoExposure_target->value : 1.0f,
		r_exposure ? r_exposure->value : 1.0f,
		r_autoExposure_min ? r_autoExposure_min->value : 0.5f,
		r_autoExposure_max ? r_autoExposure_max->value : 4.0f,
		params.autoExposureParams );
	params.localExposureParams[0] = ( r_localExposure && r_localExposure->integer ) ? 1.0f : 0.0f;
	params.localExposureParams[1] = Com_Clamp( 0.0f, 1.0f, r_localExposure_strength ? r_localExposure_strength->value : 0.35f );
	params.localExposureParams[2] = Com_Clamp( 0.0f, 3.0f, r_localExposure_shadowClamp ? r_localExposure_shadowClamp->value : 1.5f );
	params.localExposureParams[3] = Com_Clamp( 0.0f, 3.0f, r_localExposure_highlightClamp ? r_localExposure_highlightClamp->value : 1.5f );
	vk_postfx_sanitize_taa_params(
		vk.temporal.hasValidTAAHistory ? 1 : 0,
		r_taa_feedbackStationary ? r_taa_feedbackStationary->value : 0.92f,
		r_taa_feedbackMotion ? r_taa_feedbackMotion->value : 0.72f,
		r_taa_sharpen ? r_taa_sharpen->value : 0.12f,
		params.taaParams );

	if ( backEnd.projection2D || !tr.world || backEnd.viewParms.portalView != PV_NONE ) {
		Com_Memcpy( vk.postfx_params_ptr[cmd_index], &params, sizeof( params ) );
		return;
	}

	projection = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	view = backEnd.viewParms.world.modelViewMatrix;

	myGlMultMatrix( view, projection, viewProj );
	Com_Memcpy( params.viewMatrix, view, sizeof( params.viewMatrix ) );

	if ( !vk_mat4_inverse( viewProj, params.invViewProj ) ) {
		Com_Memcpy( params.invViewProj, viewProj, sizeof( params.invViewProj ) );
	}

	if ( vk_prev_matrices_valid && !vk_temporal_has_reason( VK_TEMPORAL_RESET_CAMERA_CUT | VK_TEMPORAL_RESET_MISSING_PREV_DATA |
		VK_TEMPORAL_RESET_RENDERER_INIT | VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE | VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE |
		VK_TEMPORAL_RESET_WORLD_CHANGE | VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE | VK_TEMPORAL_RESET_EXPLICIT_DEBUG ) ) {
		Com_Memcpy( params.prevViewProj, vk_prev_viewproj_matrix, sizeof( params.prevViewProj ) );
		motion_valid = qtrue;
	} else {
		Com_Memcpy( params.prevViewProj, viewProj, sizeof( params.prevViewProj ) );
	}

	params.frameInfo[3] = motion_valid ? 1.0f : 0.0f;

	{
		float zNear = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 8.0f;
		float zFar = backEnd.viewParms.zFar;
		if ( zFar <= zNear ) {
			zFar = zNear + 100.0f;
		}
		params.depthParams[0] = zNear;
		params.depthParams[1] = zFar;
	}

	Com_Memcpy( vk.postfx_params_ptr[cmd_index], &params, sizeof( params ) );
}
