#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_present_color.h"
#include "vk_present_recon.h"
#include "vk_temporal.h"
#include "vk_temporal_class.h"
#include "vk_upscale.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include <math.h>

static qboolean s_pack_weapon_temporal_for_taa = qfalse;

void vk_postfx_params_set_taa_weapon_pack( int enable )
{
	s_pack_weapon_temporal_for_taa = enable ? qtrue : qfalse;
}

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
	{
		const uint32_t rtw = vk_get_render_target_width();
		const uint32_t rth = vk_get_render_target_height();
		params.frameInfo[0] = Com_Clamp( 0.0f, 64.0f, PostFX_DepthOfField_GetMaxBlur() );
		/* Texel size for post/TAA/motion in **render target** space (FBO / r_renderScale), not transient vk.renderWidth during shadow passes. */
		params.frameInfo[1] = ( rtw > 0u ) ? ( 1.0f / (float)rtw ) : 0.0f;
		params.frameInfo[2] = ( rth > 0u ) ? ( 1.0f / (float)rth ) : 0.0f;
	}
	params.toneMapParams0[0] = Com_Clamp( 0.0f, 1.0f, PostFX_GetGradeToe() );
	params.toneMapParams0[1] = Com_Clamp( 0.0f, 1.0f, PostFX_GetGradeShoulder() );
	params.toneMapParams0[2] = Com_Clamp( 0.5f, 32.0f, PostFX_GetGradeWhitePoint() );
	params.toneMapParams0[3] = Com_Clamp( 0.0f, 0.25f, PostFX_GetGradeBlackClip() );
	params.toneMapParams1[0] = Com_Clamp( 0.0f, 1.0f, PostFX_GetGradeHighlightDesat() );
	params.toneMapParams1[1] = Com_Clamp( 0.25f, 4.0f, PostFX_GetGradeContrast() );
	params.toneMapParams1[2] = Com_Clamp( 0.1f, 0.9f, PostFX_GetGradeContrastPivot() );
	params.toneMapParams1[3] = (float)vk_present_color_preferred_tonemap();
	params.colorBalance[0] = Com_Clamp( -1.0f, 1.0f, PostFX_GetGradeTemperature() );
	params.colorBalance[1] = Com_Clamp( -1.0f, 1.0f, PostFX_GetGradeTint() );
	params.colorBalance[2] = Com_Clamp( -4.0f, 4.0f, PostFX_GetGradeExposureBias() );
	params.colorBalance[3] = ( r_pre_exposure_scale && r_pre_exposure_scale->value > 0.0f ) ? r_pre_exposure_scale->value : 1.0f;
	r_post_contrast = ri.Cvar_Get( "r_post_contrast", "1.0", 0 );
	r_post_saturation = ri.Cvar_Get( "r_post_saturation", "1.0", 0 );
	params.colorGrade[0] = Com_Clamp( 0.0f, 3.0f, PostFX_GetGradeSaturation() );
	params.colorGrade[1] = Com_Clamp( -1.0f, 1.0f, PostFX_GetGradeVibrance() );
	params.colorGrade[2] = ( r_post_contrast && r_post_contrast->value > 0.0f ) ? r_post_contrast->value : 1.0f;
	params.colorGrade[3] = ( r_post_saturation && r_post_saturation->value >= 0.0f ) ? r_post_saturation->value : 1.0f;
	params.colorGrade2[0] = Com_Clamp( -180.0f, 180.0f, PostFX_GetGradeHue() );
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
	params.autoExposureParams[0] = vk.temporal.hasValidLuminance ? vk.temporal.filteredAvgLogLuminance :
		log2f( fmaxf( 1e-4f, ( r_autoExposure_target ? r_autoExposure_target->value : 1.0f ) /
			fmaxf( r_exposure ? r_exposure->value : 1.0f, 1e-4f ) ) );
	params.autoExposureParams[1] = fmaxf( r_autoExposure_target ? r_autoExposure_target->value : 1.0f, 1e-4f );
	params.autoExposureParams[2] = fmaxf( r_autoExposure_min ? r_autoExposure_min->value : 0.5f, 0.01f );
	params.autoExposureParams[3] = fmaxf( r_autoExposure_max ? r_autoExposure_max->value : 4.0f, params.autoExposureParams[2] );
	params.localExposureParams[0] = ( r_localExposure && r_localExposure->integer ) ? 1.0f : 0.0f;
	params.localExposureParams[1] = Com_Clamp( 0.0f, 1.0f, r_localExposure_strength ? r_localExposure_strength->value : 0.35f );
	params.localExposureParams[2] = Com_Clamp( 0.0f, 3.0f, r_localExposure_shadowClamp ? r_localExposure_shadowClamp->value : 1.5f );
	params.localExposureParams[3] = Com_Clamp( 0.0f, 3.0f, r_localExposure_highlightClamp ? r_localExposure_highlightClamp->value : 1.5f );
	params.taaParams[0] = vk.temporal.hasValidTAAHistory ? 1.0f : 0.0f;
	/* Soft-skip: partial entities with unreliable motion reduce history weight instead of killing TAA. */
	if ( vk.temporal.unreliableMotionThisFrame ) {
		params.taaParams[0] = vk.temporal.hasValidTAAHistory ? 0.85f : 0.0f;
	}
	{
		float histCap = Com_Clamp( 0.0f, 0.95f,
			r_temporalHistoryWeight ? r_temporalHistoryWeight->value : 0.72f );
		float stationary = Com_Clamp( 0.0f, 0.99f, r_taa_feedbackStationary ? r_taa_feedbackStationary->value : 0.92f );
		float motionFb = Com_Clamp( 0.0f, 0.99f, r_taa_feedbackMotion ? r_taa_feedbackMotion->value : 0.72f );
		qboolean deferredLit = qfalse;
		qboolean adaptive = vk_present_recon_wants_adaptive();

		if ( vk.temporal.unreliableMotionThisFrame ) {
			histCap *= 0.55f;
		}
		/*
		 * Deferred lighting (mode 1/3) writes view-dependent specular into color_image
		 * before TAA. Prefer current more aggressively so highlight trails do not ghost.
		 * Stable (mode 2, r_deferredLighting 0) is unaffected.
		 */
		deferredLit = ( r_deferredLighting && r_deferredLighting->integer &&
			r_renderMode && ( r_renderMode->integer == 1 || r_renderMode->integer == 3 ) ) ? qtrue : qfalse;
		if ( deferredLit ) {
			histCap *= 0.72f;
			if ( r_deferredSpecular && r_deferredSpecular->integer ) {
				histCap *= 0.88f;
				motionFb = Com_Clamp( 0.0f, histCap, motionFb * 0.90f );
			}
		}
		/* Present-Time Adaptive: hard-cap history so current frame stays authoritative. */
		if ( adaptive ) {
			float adaptCap = Com_Clamp( 0.0f, 0.85f,
				r_presentAdaptiveHistoryCap ? r_presentAdaptiveHistoryCap->value : 0.42f );
			histCap = fminf( histCap, adaptCap );
			stationary = fminf( stationary, histCap );
			motionFb = fminf( motionFb, histCap * 0.70f );
		}
		params.taaParams[1] = Com_Clamp( 0.0f, histCap, stationary );
		params.taaParams[2] = Com_Clamp( 0.0f, histCap, motionFb );
	}
	params.taaParams[3] = Com_Clamp( 0.0f, 1.0f, r_taa_sharpen ? r_taa_sharpen->value : 0.12f );
	params.temporalValidity[0] = vk.temporal.prevColorValid ? 1.0f : 0.0f;
	params.temporalValidity[1] = vk.temporal.prevDepthValid ? 1.0f : 0.0f;
	params.temporalValidity[2] = vk.temporal.prevClassValid ? 1.0f : 0.0f;
	params.temporalValidity[3] = vk.temporal.weaponHistoryValid ? 1.0f : 0.0f;
	params.weaponTemporalParams[0] = r_weaponTemporalHistoryWeight ?
		r_weaponTemporalHistoryWeight->value : 0.58f;
	params.weaponTemporalParams[1] = r_weaponTemporalVarianceGamma ?
		r_weaponTemporalVarianceGamma->value : 0.75f;
	params.weaponTemporalParams[2] = r_weaponTemporalDepthThreshold ?
		r_weaponTemporalDepthThreshold->value : 0.025f;
	params.weaponTemporalParams[3] = r_weaponTemporalReactiveScale ?
		r_weaponTemporalReactiveScale->value : 1.0f;
	params.temporalDebugParams[0] = r_temporalDebugVectorScale ?
		r_temporalDebugVectorScale->value : 80.0f;
	params.temporalDebugParams[1] = r_weaponThinSightReject ?
		r_weaponThinSightReject->value : 0.65f;
	params.temporalDebugParams[2] = 0.0f;
	if ( r_weaponAnalyticFog && r_weaponAnalyticFog->integer ) {
		params.temporalDebugParams[2] += 1.0f;
	}
	if ( r_weaponTemporalCompare && r_weaponTemporalCompare->integer == 1 ) {
		params.temporalDebugParams[2] += 2.0f; /* split-screen compare marker */
	}
	params.temporalDebugParams[3] = r_weaponLocalAO && r_weaponLocalAO->integer ? 1.0f : 0.0f;
	/* Dynamic-object temporal debug overrides vector-scale channel (debug views only). */
	{
		cvar_t *objDbg = ri.Cvar_Get( "r_temporalObjectDebug", "0", CVAR_TEMP );
		if ( objDbg && objDbg->integer > 0 ) {
			params.temporalDebugParams[0] = 100.0f + (float)objDbg->integer;
		}
	}
	/* Pack dynamic-object resolve knobs into DoF channels when DoF is off (TAA-only consumers). */
	if ( !( params.depthOfField[0] > 0.5f ) ) {
		cvar_t *dynHist = ri.Cvar_Get( "r_dynamicObjectHistoryMax", "0.48", CVAR_ARCHIVE_ND );
		cvar_t *dynDepth = ri.Cvar_Get( "r_dynamicObjectDepthThreshold", "0.012", CVAR_ARCHIVE_ND );
		cvar_t *dynDilate = ri.Cvar_Get( "r_dynamicObjectRejectDilation", "1.5", CVAR_ARCHIVE_ND );
		params.depthOfField[1] = dynHist ? dynHist->value : 0.48f;
		params.depthOfField[2] = dynDepth ? dynDepth->value : 0.012f;
		params.depthOfField[3] = dynDilate ? dynDilate->value : 1.5f;
	}
	if ( R_Upscale_WantTemporal() ) {
		float sharp = params.taaParams[3] + R_Upscale_GetSharpness();
		params.taaParams[3] = Com_Clamp( 0.0f, 1.0f, sharp );
	}
	{
		float jx = 0.0f, jy = 0.0f;
		R_Upscale_GetJitter( &jx, &jy );
		params.lutParams[2] = jx;
		params.lutParams[3] = jy;
	}

	/* Temporal Reconstruction flags in spare PostFX channels (do not clobber motionBlur). */
	params.colorGrade2[1] = ( r_temporalVarianceClip && r_temporalVarianceClip->integer ) ? 1.0f : 0.0f;
	params.colorGrade2[2] = ( r_temporalDisocclusion && r_temporalDisocclusion->integer ) ? 1.0f : 0.0f;
	params.colorGrade2[3] = ( r_temporalReactiveMask && r_temporalReactiveMask->integer ) ? 1.0f : 0.0f;
	params.midsGamma[3] = ( r_temporalReactiveMask && r_temporalReactiveMask->integer &&
		vk.reactive_mask_view != VK_NULL_HANDLE ) ? 1.0f : 0.0f;
	/*
	 * TAA-only packing (restored on next gamma/post refresh):
	 * splitShadow.a = r_weaponTemporalMode (0/1/2)
	 * splitHighlight.a = temporal class attachment active
	 */
	if ( s_pack_weapon_temporal_for_taa ) {
		cvar_t *mode = r_weaponTemporalMode;
		int effectiveMode;
		if ( !mode ) {
			mode = ri.Cvar_Get( "r_weaponTemporalMode", "1", CVAR_ARCHIVE_ND );
		}
		effectiveMode = Com_Clamp( 0, 2, mode->integer );
		if ( r_weaponTemporalCompare && r_weaponTemporalCompare->integer == 2 ) {
			/* Alternate frames: 0 → 1 → 2 → 0 … */
			effectiveMode = (int)( vk.temporal.frameIndex % 3u );
		} else if ( r_weaponTemporalCompare && r_weaponTemporalCompare->integer == 1 ) {
			/* Split handled in weapon_taa_composite; pack mode 2 so history path stays live. */
			effectiveMode = 2;
		}
		params.splitShadow[3] = (float)effectiveMode;
		params.splitHighlight[3] = ( vk.temporal_class_image[0] != VK_NULL_HANDLE &&
			vk.temporal.classHasPrev && vk.temporal.prevClassValid ) ? 1.0f : 0.0f;
	}
	/*
	 * highlightsGain.a → Present-Time Adaptive Reconstruction:
	 *   0 = off
	 *   1.0+budget = adaptive, spatial fallback off
	 *   2.0+budget = adaptive, spatial fallback on
	 * budget in [0,1] (r_presentAdaptiveBudget).
	 */
	params.highlightsGain[3] = 0.0f;
	if ( vk_present_recon_wants_adaptive() ) {
		float budget = Com_Clamp( 0.0f, 1.0f,
			r_presentAdaptiveBudget ? r_presentAdaptiveBudget->value : 0.15f );
		float base = ( r_presentAdaptiveSpatial && r_presentAdaptiveSpatial->integer ) ? 2.0f : 1.0f;
		params.highlightsGain[3] = base + budget;
	}
	/* shadowsLift.a → TAA debugMode: 1=MV, 2=reject reasons, 3–12 ownership / adaptive viz.
	 * r_temporalDebug (user-facing) maps onto the same codes; r_debugHistoryRejection wins if set. */
	params.shadowsLift[3] = 0.0f;
	if ( r_debugMotionVectors && r_debugMotionVectors->integer ) {
		params.shadowsLift[3] = 1.0f;
	} else if ( r_debugAdaptiveSampleMask && r_debugAdaptiveSampleMask->integer ) {
		params.shadowsLift[3] = 9.0f;
	} else if ( r_debugHistoryRejection && r_debugHistoryRejection->integer > 0 ) {
		params.shadowsLift[3] = (float)r_debugHistoryRejection->integer;
	} else if ( r_temporalDebug && r_temporalDebug->integer > 0 ) {
		/* User modes 1–6 → internal TAA debug codes. */
		static const int temporalDebugMap[7] = {
			0,
			1,  /* velocity / final MV */
			2,  /* depth rejection reasons */
			4,  /* history weight / confidence */
			5,  /* disocclusion */
			7,  /* weapon mask */
			10  /* current vs history */
		};
		int mode = r_temporalDebug->integer;
		if ( mode >= 1 && mode <= 6 ) {
			params.shadowsLift[3] = (float)temporalDebugMap[mode];
		} else if ( mode >= 7 && mode <= 33 ) {
			params.shadowsLift[3] = (float)mode;
		}
	}

	if ( backEnd.projection2D || !tr.world || backEnd.viewParms.portalView != PV_NONE ) {
		Com_Memcpy( vk.postfx_params_ptr[cmd_index], &params, sizeof( params ) );
		return;
	}

	projection = backEnd.viewParms.projectionMatrix;
	view = backEnd.viewParms.world.modelViewMatrix;

	/* Match rasterization: Vulkan Y-flip so depth reprojection agrees with MVs. */
	{
		float proj_vk[16];
		vk_get_projection_matrix_vk( projection, proj_vk );
		myGlMultMatrix( view, proj_vk, viewProj );
	}
	Com_Memcpy( params.viewMatrix, view, sizeof( params.viewMatrix ) );

	if ( !vk_mat4_inverse( viewProj, params.invViewProj ) ) {
		Com_Memcpy( params.invViewProj, viewProj, sizeof( params.invViewProj ) );
	}

	if ( vk_prev_matrices_valid &&
		!vk_temporal_has_reason( VK_TEMPORAL_RESET_CAMERA_CUT | VK_TEMPORAL_RESET_MISSING_PREV_DATA |
		VK_TEMPORAL_RESET_RENDERER_INIT | VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE | VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE |
		VK_TEMPORAL_RESET_WORLD_CHANGE | VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE | VK_TEMPORAL_RESET_EXPLICIT_DEBUG ) ) {
		float prev_proj_vk[16];
		float prev_vp[16];
		vk_get_projection_matrix_vk( vk_prev_projection_matrix, prev_proj_vk );
		myGlMultMatrix( vk_prev_view_matrix, prev_proj_vk, prev_vp );
		Com_Memcpy( params.prevViewProj, prev_vp, sizeof( params.prevViewProj ) );
		motion_valid = qtrue;
	} else {
		Com_Memcpy( params.prevViewProj, viewProj, sizeof( params.prevViewProj ) );
	}

	params.frameInfo[3] = motion_valid ? 1.0f : 0.0f;
	vk.temporal.prevVelocityValid = motion_valid;

	{
		float zNear = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 8.0f;
		float zFar = backEnd.viewParms.zFar;
		if ( zFar <= zNear ) {
			zFar = zNear + 100.0f;
		}
		params.depthParams[0] = zNear;
		params.depthParams[1] = zFar;
		params.depthParams[2] = ( r_taaMotionVectors && r_taaMotionVectors->integer &&
			vk.motion_vector_view != VK_NULL_HANDLE ) ? 1.0f : 0.0f;
		params.depthParams[3] = Com_Clamp( 0.0f, 0.95f,
			r_temporalHistoryWeight ? r_temporalHistoryWeight->value : 0.80f );
	}

	Com_Memcpy( vk.postfx_params_ptr[cmd_index], &params, sizeof( params ) );
}
