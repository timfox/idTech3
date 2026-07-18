#include "tr_local.h"
#include "vk.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include <math.h>

float vk_prev_view_matrix[16];
float vk_prev_projection_matrix[16];
float vk_prev_viewproj_matrix[16];
qboolean vk_prev_matrices_valid = qfalse;
int vk_prev_volumetric_time_ms = 0;
int vk_near_static_view_frames = 0;
qboolean vk_prev_volumetric_time_valid = qfalse;
float vk_volumetric_noise_time = 0.0f;
vk_volumetric_validation_state_t vk_volumetric_validation_state;

static const char *vk_temporal_reason_string( uint32_t reason )
{
	switch ( reason ) {
		case VK_TEMPORAL_RESET_RENDERER_INIT: return "renderer_init";
		case VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE: return "swapchain_change";
		case VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE: return "render_size_change";
		case VK_TEMPORAL_RESET_WORLD_CHANGE: return "world_change";
		case VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE: return "client_state_change";
		case VK_TEMPORAL_RESET_CAMERA_CUT: return "camera_cut";
		case VK_TEMPORAL_RESET_EXPLICIT_DEBUG: return "explicit_debug";
		case VK_TEMPORAL_RESET_MISSING_PREV_DATA: return "missing_prev_data";
		default: return "unknown";
	}
}

static float vk_matrix_rotation_max_abs_diff( const float *a, const float *b )
{
	static const int idx[] = {
		0, 1, 2,
		4, 5, 6,
		8, 9, 10
	};
	float max_diff = 0.0f;
	int i;

	for ( i = 0; i < (int)ARRAY_LEN( idx ); i++ ) {
		const float d = fabsf( a[idx[i]] - b[idx[i]] );
		if ( d > max_diff ) {
			max_diff = d;
		}
	}

	return max_diff;
}

static void vk_temporal_clear_frame_state( void )
{
	vk.temporal.pendingResetReasons = 0u;
	vk.temporal.appliedResetReasons = 0u;
	vk.temporal.sharedCameraCut = qfalse;
	vk.temporal.unreliableMotionThisFrame = qfalse;
	vk.temporal.firstPersonProjectionThisFrame = qfalse;
}

static void vk_temporal_request_reset( uint32_t reasons )
{
	vk.temporal.pendingResetReasons |= reasons;
}

void vk_temporal_request_sticky_reset( uint32_t reasons )
{
	vk.temporal.stickyResetReasons |= reasons;
}

qboolean vk_temporal_has_reason( uint32_t reasonMask )
{
	return ( vk.temporal.appliedResetReasons & reasonMask ) != 0u ? qtrue : qfalse;
}

qboolean vk_temporal_near_static_streak_guard( void )
{
	/* Matches volumetric SkipStatic threshold (~0.5s at 60Hz). */
	if ( !r_volumetricFogSkipStatic || !r_volumetricFogSkipStatic->integer ) {
		return qfalse;
	}
	return ( vk_near_static_view_frames >= 30 ) ? qtrue : qfalse;
}

void vk_temporal_note_first_person_projection( void )
{
	vk.temporal.firstPersonProjectionThisFrame = qtrue;
	vk.temporal.unreliableMotionThisFrame = qtrue;
}

static void vk_temporal_log_reset( uint32_t reasons, qboolean hardReset )
{
	uint32_t knownReasons[] = {
		VK_TEMPORAL_RESET_RENDERER_INIT,
		VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE,
		VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE,
		VK_TEMPORAL_RESET_WORLD_CHANGE,
		VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE,
		VK_TEMPORAL_RESET_CAMERA_CUT,
		VK_TEMPORAL_RESET_EXPLICIT_DEBUG,
		VK_TEMPORAL_RESET_MISSING_PREV_DATA
	};
	char reasonBuf[256];
	char *ptr = reasonBuf;
	char *end = reasonBuf + sizeof( reasonBuf );
	int i;

	if ( !r_temporalDebug || r_temporalDebug->integer <= 0 || reasons == 0u ) {
		return;
	}

	*ptr = '\0';
	for ( i = 0; i < (int)ARRAY_LEN( knownReasons ); i++ ) {
		const uint32_t reason = knownReasons[i];
		const char *name;
		size_t len;

		if ( !( reasons & reason ) ) {
			continue;
		}
		name = vk_temporal_reason_string( reason );
		if ( ptr != reasonBuf && ptr + 1 < end ) {
			*ptr++ = ',';
		}
		len = strlen( name );
		if ( ptr + len >= end ) {
			break;
		}
		Com_Memcpy( ptr, name, len );
		ptr += len;
		*ptr = '\0';
	}

	ri.Printf( PRINT_DEVELOPER, "[VK][temporal] reset=%s reasons=%s\n",
		hardReset ? "hard" : "soft",
		reasonBuf[0] ? reasonBuf : "none" );
	if ( r_temporalDebug->integer >= 2 ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][temporal] frame=%u cameraCut=%d invalidate={motion=1 taa=1 volumetric=1 exposure=1} world=%s noWorld=%d gameplay=%d render=%ux%u swapchain=%ux%u\n",
			vk.temporal.frameIndex,
			vk.temporal.sharedCameraCut,
			vk.temporal.worldName[0] ? vk.temporal.worldName : "<none>",
			vk.temporal.noWorldModel,
			vk.temporal.stableGameplayState,
			vk.temporal.lastRenderWidth, vk.temporal.lastRenderHeight,
			vk.temporal.lastSwapchainWidth, vk.temporal.lastSwapchainHeight );
		ri.Printf( PRINT_DEVELOPER, "[VK][temporal] unreliableMotion=%d\n",
			vk.temporal.unreliableMotionThisFrame );
	}
}

void vk_reset_taa_history( void )
{
	vk.temporal.hasValidTAAHistory = qfalse;
	vk.temporal.taaHistoryIndex = 0u;
}

static void vk_temporal_apply_resets( qboolean hardReset )
{
	const uint32_t reasons = vk.temporal.pendingResetReasons;
	const uint32_t nonCameraReasons = reasons & ~VK_TEMPORAL_RESET_CAMERA_CUT;
	const float manualExposure = ( r_exposure && r_exposure->value > 0.0f ) ? r_exposure->value : 1.0f;

	if ( reasons == 0u ) {
		vk.temporal.appliedResetReasons = 0u;
		return;
	}

	vk.temporal.appliedResetReasons = reasons;
	vk.temporal.sharedCameraCut = ( reasons & VK_TEMPORAL_RESET_CAMERA_CUT ) != 0u ? qtrue : qfalse;
	/*
	 * Keep temporal consumers invalidated from one place so new history-based
	 * passes hook into the shared reset policy instead of open-coding cuts.
	 */
	vk_reset_motion_history();
	vk_reset_taa_history();
	vk_reset_volumetric_history();
	vk_reset_occlusion_visibility();
	if ( nonCameraReasons != 0u ) {
		vk.adaptedExposure = manualExposure;
		vk.temporal.hasValidLuminance = qfalse;
		vk.temporal.filteredAvgLogLuminance = 0.0f;
	}

	vk_temporal_log_reset( reasons, hardReset );
}

static qboolean vk_temporal_compute_shared_camera_cut( uint32_t *outReasons )
{
	uint32_t reasons = 0u;
	int clientState = ri.CL_GetState ? ri.CL_GetState() : CA_ACTIVE;
	qboolean stableGameplayState = ( clientState == CA_ACTIVE ) ? qtrue : qfalse;
	qboolean stateTransition = ( clientState != vk.prevClientState ) ? qtrue : qfalse;
	qboolean worldValid = ( tr.world != NULL ) ? qtrue : qfalse;
	qboolean noWorldModel = ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;
	qboolean worldTransition = ( worldValid != vk.temporal.worldWasValid ) ? qtrue : qfalse;
	qboolean noWorldTransition = ( noWorldModel != vk.temporal.noWorldModel ) ? qtrue : qfalse;
	qboolean cameraCut = qfalse;

	{
		static cvar_t *r_temporalScopeReduce;
		r_temporalScopeReduce = ri.Cvar_Get( "r_temporalScopeReduce", "1", CVAR_ARCHIVE_ND );
		if ( !r_temporalScopeReduce->integer ) {
			if ( !vk_prev_matrices_valid || !vk.temporal.worldWasValid ) {
				reasons |= VK_TEMPORAL_RESET_MISSING_PREV_DATA;
			}
		} else if ( !vk_prev_matrices_valid ) {
			/* Camera matrices only — per-entity missing prev skin handled in glTF path */
			reasons |= VK_TEMPORAL_RESET_MISSING_PREV_DATA;
		}
	}
	if ( worldTransition || noWorldTransition ) {
		reasons |= VK_TEMPORAL_RESET_WORLD_CHANGE;
	}
	{
		static int prevWorldConfigEpoch = -1;
		cvar_t *epoch = ri.Cvar_Get( "r_worldConfigEpoch", "0", 0 );
		if ( epoch && epoch->integer != prevWorldConfigEpoch ) {
			if ( prevWorldConfigEpoch >= 0 ) {
				reasons |= VK_TEMPORAL_RESET_WORLD_CHANGE;
				vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_WORLD_CHANGE );
			}
			prevWorldConfigEpoch = epoch->integer;
		}
	}
	if ( stateTransition || stableGameplayState != vk.temporal.stableGameplayState ) {
		reasons |= VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE;
	}

	if ( !stableGameplayState || !worldValid || noWorldModel ) {
		cameraCut = stateTransition || worldTransition || noWorldTransition;
		vk_near_static_view_frames = 0;
	} else {
		float dx = tr.refdef.vieworg[0] - vk.prevViewOrigin[0];
		float dy = tr.refdef.vieworg[1] - vk.prevViewOrigin[1];
		float dz = tr.refdef.vieworg[2] - vk.prevViewOrigin[2];
		float distSq = dx * dx + dy * dy + dz * dz;
		float dotForward = DotProduct( tr.refdef.viewaxis[0], vk.prevViewForward );
		qboolean posCut = ( distSq > 192.0f * 192.0f );
		qboolean angleCut = ( dotForward < 0.55f );
		qboolean portalView = ( backEnd.viewParms.portalView != PV_NONE ) ? qtrue : qfalse;

		if ( vk_prev_matrices_valid ) {
			const float *projection = backEnd.viewParms.projectionMatrix;
			const float *view = backEnd.viewParms.world.modelViewMatrix;
			float viewRotDelta = vk_matrix_rotation_max_abs_diff( view, vk_prev_view_matrix );
			float projDelta = vk_matrix_max_abs_diff( projection, vk_prev_projection_matrix );

			/*
			 * Full 4x4 diffs include translation, which changes every movement frame.
			 * That caused false camera-cut resets while simply walking.
			 */
			if ( projDelta > 0.05f || portalView ) {
				cameraCut = qtrue;
				vk_near_static_view_frames = 0;
			} else {
				const float nearStaticRotThresh = 0.003f;
				if ( viewRotDelta < nearStaticRotThresh && distSq < ( 0.5f * 0.5f ) && dotForward > 0.9990f ) {
					if ( vk_near_static_view_frames < 600 ) {
						vk_near_static_view_frames++;
					}
				} else {
					vk_near_static_view_frames = 0;
				}
			}
		} else {
			cameraCut = qtrue;
			vk_near_static_view_frames = 0;
		}

		if ( posCut || angleCut || portalView ) {
			cameraCut = qtrue;
		}
	}

	if ( r_volumetricFogForceCameraCut && r_volumetricFogForceCameraCut->integer > 0 ) {
		reasons |= VK_TEMPORAL_RESET_EXPLICIT_DEBUG;
		cameraCut = qtrue;
		vk_volumetric_validation_state.forced_camera_cut_events++;
		ri.Cvar_SetValue( "r_volumetricFogForceCameraCut", 0.0f );
	}

	if ( cameraCut ) {
		reasons |= VK_TEMPORAL_RESET_CAMERA_CUT;
	}

	vk.temporal.sharedCameraCut = cameraCut;
	if ( outReasons ) {
		*outReasons |= reasons;
	}
	return cameraCut;
}

void vk_temporal_begin_frame( void )
{
	uint32_t reasons = 0u;
	uint32_t renderWidth = vk_get_render_target_width();
	uint32_t renderHeight = vk_get_render_target_height();
	uint32_t swapchainWidth = vk.swapchain_extent_valid ? vk.swapchain_extent.width : 0u;
	uint32_t swapchainHeight = vk.swapchain_extent_valid ? vk.swapchain_extent.height : 0u;
	qboolean hardReset;

	if ( vk.temporal.frameIndex == 0u ) {
		reasons |= VK_TEMPORAL_RESET_RENDERER_INIT;
	}
	vk.temporal.frameIndex++;
	vk_temporal_clear_frame_state();
	reasons |= vk.temporal.stickyResetReasons;
	vk.temporal.stickyResetReasons = 0u;

	if ( vk.temporal.lastRenderWidth && vk.temporal.lastRenderHeight &&
		( renderWidth != vk.temporal.lastRenderWidth || renderHeight != vk.temporal.lastRenderHeight ) ) {
		reasons |= VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE;
	}
	if ( vk.temporal.lastSwapchainWidth && vk.temporal.lastSwapchainHeight &&
		( swapchainWidth != vk.temporal.lastSwapchainWidth || swapchainHeight != vk.temporal.lastSwapchainHeight ) ) {
		reasons |= VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE;
	}

	vk_temporal_compute_shared_camera_cut( &reasons );
	vk_temporal_request_reset( reasons );

	hardReset = ( vk.temporal.pendingResetReasons & ~VK_TEMPORAL_RESET_CAMERA_CUT ) != 0u ? qtrue : qfalse;
	vk_temporal_apply_resets( hardReset );
}

void vk_temporal_commit_frame_state( void )
{
	const int clientState = ri.CL_GetState ? ri.CL_GetState() : CA_ACTIVE;
	const qboolean worldValid = ( tr.world != NULL ) ? qtrue : qfalse;
	const qboolean noWorldModel = ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;

	if ( vk.temporal.firstPersonProjectionThisFrame != vk.temporal.firstPersonProjectionLastFrame ) {
		vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_CAMERA_CUT );
	}

	vk.prevClientState = clientState;
	vk.temporal.worldWasValid = worldValid;
	vk.temporal.noWorldModel = noWorldModel;
	vk.temporal.stableGameplayState = ( clientState == CA_ACTIVE ) ? qtrue : qfalse;
	vk.temporal.firstPersonProjectionLastFrame = vk.temporal.firstPersonProjectionThisFrame;
	vk.temporal.lastRenderWidth = vk_get_render_target_width();
	vk.temporal.lastRenderHeight = vk_get_render_target_height();
	vk.temporal.lastSwapchainWidth = vk.swapchain_extent_valid ? vk.swapchain_extent.width : 0u;
	vk.temporal.lastSwapchainHeight = vk.swapchain_extent_valid ? vk.swapchain_extent.height : 0u;
	Q_strncpyz( vk.temporal.worldName, ( worldValid && tr.world->name[0] ) ? tr.world->name : "", sizeof( vk.temporal.worldName ) );
	VectorCopy( tr.refdef.vieworg, vk.prevViewOrigin );
	VectorCopy( tr.refdef.viewaxis[0], vk.prevViewForward );
}

void vk_temporal_update_auto_exposure( void )
{
	cvar_t *auto_var = ri.Cvar_Get( "r_exposure_auto", "0", 0 );
	cvar_t *target_var = ri.Cvar_Get( "r_exposure_auto_target", "1.0", CVAR_ARCHIVE_ND );
	cvar_t *speed_var = ri.Cvar_Get( "r_exposure_auto_speed", "2.0", CVAR_ARCHIVE_ND );
	cvar_t *speed_up_var = ri.Cvar_Get( "r_autoExposure_speedUp", "1.5", CVAR_ARCHIVE_ND );
	cvar_t *speed_down_var = ri.Cvar_Get( "r_autoExposure_speedDown", "3.0", CVAR_ARCHIVE_ND );
	cvar_t *min_var = ri.Cvar_Get( "r_autoExposure_min", "0.5", CVAR_ARCHIVE_ND );
	cvar_t *max_var = ri.Cvar_Get( "r_autoExposure_max", "4.0", CVAR_ARCHIVE_ND );

	if ( !( auto_var && auto_var->integer && target_var && speed_var ) ) {
		return;
	}

	{
		int clientState = ri.CL_GetState ? ri.CL_GetState() : CA_ACTIVE;
		qboolean stateTransition = ( clientState != vk.prevClientState );
		qboolean stableGameplayState = vk.temporal.stableGameplayState;
		qboolean hardReset = ( vk.temporal.appliedResetReasons & ~VK_TEMPORAL_RESET_CAMERA_CUT ) != 0u ? qtrue : qfalse;
		qboolean cameraCut = vk.temporal.sharedCameraCut;
		qboolean significantCut = ( vk.temporal.appliedResetReasons &
			( VK_TEMPORAL_RESET_RENDERER_INIT |
			  VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE |
			  VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE |
			  VK_TEMPORAL_RESET_WORLD_CHANGE |
			  VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE |
			  VK_TEMPORAL_RESET_EXPLICIT_DEBUG |
			  VK_TEMPORAL_RESET_MISSING_PREV_DATA ) ) ? qtrue : qfalse;
		float target = target_var->value > 0.0f ? target_var->value : 1.0f;
		float legacySpeed = speed_var->value > 0.0f ? speed_var->value * 0.016f : 0.02f;
		float speedUp = speed_up_var && speed_up_var->value > 0.0f ? speed_up_var->value * 0.016f : legacySpeed;
		float speedDown = speed_down_var && speed_down_var->value > 0.0f ? speed_down_var->value * 0.016f : legacySpeed;
		float speed = legacySpeed;
		const float manualExposure = ( r_exposure && r_exposure->value > 0.0f ) ? r_exposure->value : 1.0f;
		const float minExposure = ( min_var && min_var->value > 0.0f ) ? min_var->value : manualExposure;
		const float maxExposure = ( max_var && max_var->value >= minExposure ) ? max_var->value :
			( manualExposure * 6.0f < 6.0f ? 6.0f : manualExposure * 6.0f );
		float targetExp = manualExposure;
		qboolean luminanceValid = qfalse;
		float avgLogLum = 0.0f;

		if ( !stableGameplayState || !tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			vk.temporal.hasValidLuminance = qfalse;
			vk.temporal.filteredAvgLogLuminance = 0.0f;
			targetExp = manualExposure;
			speed = stateTransition ? 0.35f : speedUp;
		} else if ( !hardReset && ( !cameraCut || !significantCut ) && vk.luminance_staging_ptr ) {
			avgLogLum = *(const float *)vk.luminance_staging_ptr;
			if ( avgLogLum == avgLogLum && avgLogLum > -20.0f && avgLogLum < 20.0f ) {
				luminanceValid = qtrue;
			}

			if ( luminanceValid ) {
				if ( vk.temporal.hasValidLuminance ) {
					float delta = avgLogLum - vk.temporal.filteredAvgLogLuminance;
					const float maxStep = 0.35f;
					const float smoothing = 0.20f;

					if ( delta > maxStep ) {
						avgLogLum = vk.temporal.filteredAvgLogLuminance + maxStep;
					} else if ( delta < -maxStep ) {
						avgLogLum = vk.temporal.filteredAvgLogLuminance - maxStep;
					}

					avgLogLum = vk.temporal.filteredAvgLogLuminance +
						( avgLogLum - vk.temporal.filteredAvgLogLuminance ) * smoothing;
				}

				vk.temporal.filteredAvgLogLuminance = avgLogLum;
				vk.temporal.hasValidLuminance = qtrue;

				{
					float sceneLum = powf( 2.0f, vk.temporal.filteredAvgLogLuminance );
					targetExp = ( sceneLum > 1e-6f ) ? ( target / sceneLum ) : manualExposure;
					targetExp = ( targetExp < minExposure ) ? minExposure : ( targetExp > maxExposure ? maxExposure : targetExp );
				}
			} else {
				targetExp = manualExposure;
			}
		} else {
			if ( hardReset || ( cameraCut && significantCut ) ) {
				vk.temporal.hasValidLuminance = qfalse;
				vk.temporal.filteredAvgLogLuminance = 0.0f;
			}
			targetExp = manualExposure;
		}

		if ( cameraCut && significantCut ) {
			cvar_t *cap_var = ri.Cvar_Get( "r_exposure_auto_cap_on_cut", "1.35", 0 );
			float cap = cap_var ? cap_var->value : 1.35f;
			speed = 0.5f;
			if ( cap > 0.0f && targetExp > cap ) {
				targetExp = cap;
			}
		}

		if ( !hardReset && vk.adaptedExposure > 0.0f ) {
			speed = ( targetExp > vk.adaptedExposure ) ? speedUp : speedDown;
			float ratio = targetExp / vk.adaptedExposure;
			if ( ratio < 0.75f ) {
				speed = ( speed < speedDown ) ? speedDown : speed;
			} else if ( ratio > 1.5f ) {
				speed = ( speed < speedUp ) ? speedUp : speed;
			}
		}

		if ( stateTransition ) {
			speed = ( speed < 0.30f ) ? 0.30f : speed;
		}

		vk.adaptedExposure += ( targetExp - vk.adaptedExposure ) * speed;
		vk.adaptedExposure = ( vk.adaptedExposure < minExposure ) ? minExposure : ( vk.adaptedExposure > maxExposure ? maxExposure : vk.adaptedExposure );
	}
}
