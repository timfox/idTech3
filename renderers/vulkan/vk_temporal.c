#include "tr_local.h"
#include "vk.h"
#include "vk_temporal.h"
#include "vk_renderer_iq_p1.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_ambient_visibility.h"
#include "vk_deferred_gbuffer.h"
#include "vk_upscale.h"
#include "vk_pass_registry.h"
#include "vk_exposure_histogram.h"
#include "vk_postfx.h"
#include "vk_reactive_mask.h"
#include "vk_object_id.h"
#include "vk_temporal_class.h"
#include "vk_velocity_space.h"
#include "vk_image_layout.h"
#include "vk_scene_pass.h"
#include "vk_volumetric_internal.h"
#include <math.h>
#include <stdlib.h>

float vk_prev_view_matrix[16];
float vk_prev_projection_matrix[16];
float vk_prev_viewproj_matrix[16];
qboolean vk_prev_matrices_valid = qfalse;
uint32_t vk_prev_matrices_frame = 0u;
float vk_prev_jitter_x = 0.0f;
float vk_prev_jitter_y = 0.0f;
qboolean vk_prev_jitter_valid = qfalse;
int vk_prev_volumetric_time_ms = 0;
int vk_near_static_view_frames = 0;
qboolean vk_prev_volumetric_time_valid = qfalse;
float vk_volumetric_noise_time = 0.0f;
vk_volumetric_validation_state_t vk_volumetric_validation_state;

/* Single table — regression check counts knownReasons[] entries once. */
static const uint32_t knownReasons[] = {
	VK_TEMPORAL_RESET_RENDERER_INIT,
	VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE,
	VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE,
	VK_TEMPORAL_RESET_WORLD_CHANGE,
	VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE,
	VK_TEMPORAL_RESET_CAMERA_CUT,
	VK_TEMPORAL_RESET_EXPLICIT_DEBUG,
	VK_TEMPORAL_RESET_MISSING_PREV_DATA
};

static qboolean vk_surf_temporal_is_game( void )
{
	const char *fsGame = ri.Cvar_VariableString( "fs_game" );
	const char *baseGame = ri.Cvar_VariableString( "fs_basegame" );

	if ( fsGame && !Q_stricmp( fsGame, "surf" ) ) {
		return qtrue;
	}
	if ( baseGame && !Q_stricmp( baseGame, "surf" ) ) {
		return qtrue;
	}
	return qfalse;
}

static const char *vk_surf_weapon_temporal_mode_name( int mode )
{
	switch ( mode ) {
	case 0:
		return "no weapon history";
	case 1:
		return "classified shared history";
	case 2:
		return "independent weapon history";
	default:
		return "invalid";
	}
}

static void vk_surf_validation_line( const char *result, const char *item, const char *detail )
{
	ri.Printf( PRINT_ALL, "  %-4s %-30s %s\n", result, item, detail );
}

void vk_surf_log_temporal_config( void )
{
	const int mode = r_weaponTemporalMode ? r_weaponTemporalMode->integer : -1;
	const qboolean classAvailable = vk_temporal_class_active() &&
		vk.temporal_class_stamp_pipeline != VK_NULL_HANDLE;
	const qboolean reactiveAvailable = vk_reactive_mask_active() &&
		vk.reactive_stamp_weapon_pipeline != VK_NULL_HANDLE;
	const qboolean velocityAvailable = r_taaMotionVectors && r_taaMotionVectors->integer &&
		vk.motion_vector_image != VK_NULL_HANDLE && vk.motion_vector_view != VK_NULL_HANDLE;
	const qboolean previousDepthAvailable =
		vk.temporal_prev_depth_image[0] != VK_NULL_HANDLE &&
		vk.temporal_prev_depth_image[1] != VK_NULL_HANDLE &&
		vk.temporal_depth_history_copy_pipeline != VK_NULL_HANDLE;
	const qboolean combinedBeforeBloom = vk_temporal_defer_bloom_for_weapon();

	if ( !vk_surf_temporal_is_game() ) {
		return;
	}

	ri.Printf( PRINT_ALL, "Surf temporal configuration:\n" );
	ri.Printf( PRINT_ALL, "  TAA: %s\n", ( r_taa && r_taa->integer ) ? "enabled" : "disabled" );
	ri.Printf( PRINT_ALL, "  weapon temporal mode: %s\n",
		vk_surf_weapon_temporal_mode_name( mode ) );
	ri.Printf( PRINT_ALL, "  weapon class mask: %s\n", classAvailable ? "available" : "unavailable" );
	ri.Printf( PRINT_ALL, "  weapon reactive mask: %s\n", reactiveAvailable ? "available" : "unavailable" );
	ri.Printf( PRINT_ALL, "  weapon MVP velocity: %s\n", velocityAvailable ? "available" : "unavailable" );
	ri.Printf( PRINT_ALL, "  previous depth: %s\n",
		previousDepthAvailable ? "available (dual R32F history)" : "unavailable" );
	ri.Printf( PRINT_ALL, "  weapon composition stage: %s\n",
		combinedBeforeBloom ? "pre-bloom combined HDR" : "post-bloom (weapon bloom disabled)" );
	vk_surf_validate_temporal_config_f();
}

void vk_surf_validate_temporal_config_f( void )
{
	const int mode = r_weaponTemporalMode ? r_weaponTemporalMode->integer : -1;
	const qboolean taaEnabled = r_taa && r_taa->integer;
	const qboolean classEnabled = mode > 0;
	const qboolean classAvailable = vk_temporal_class_active() &&
		vk.temporal_class_stamp_pipeline != VK_NULL_HANDLE &&
		vk.temporal_class_stamp_descriptor != VK_NULL_HANDLE;
	const qboolean reactiveEnabled = r_temporalReactiveMask && r_temporalReactiveMask->integer;
	const qboolean reactiveAvailable = vk_reactive_mask_active() &&
		vk.reactive_stamp_weapon_pipeline != VK_NULL_HANDLE;
	const qboolean velocityEnabled = r_taaMotionVectors && r_taaMotionVectors->integer;
	const qboolean velocityAvailable = velocityEnabled &&
		vk.motion_vector_image != VK_NULL_HANDLE && vk.motion_vector_view != VK_NULL_HANDLE;
	const qboolean weaponAfter = r_temporalWeaponAfterTaa &&
		r_temporalWeaponAfterTaa->integer && vk_temporal_want_weapon_after_world_post();
	const qboolean previousDepthAvailable =
		vk.temporal_prev_depth_image[0] != VK_NULL_HANDLE &&
		vk.temporal_prev_depth_image[1] != VK_NULL_HANDLE &&
		vk.temporal_prev_depth_descriptor[0] != VK_NULL_HANDLE &&
		vk.temporal_prev_depth_descriptor[1] != VK_NULL_HANDLE &&
		vk.temporal_depth_history_copy_pipeline != VK_NULL_HANDLE;
	int failures = 0;
	int warnings = 0;

	ri.Printf( PRINT_ALL, "======== Surf Temporal Validation ========\n" );
	if ( vk_surf_temporal_is_game() ) {
		vk_surf_validation_line( "PASS", "Surf game context", "fs_game/fs_basegame is surf" );
	} else {
		vk_surf_validation_line( "WARN", "Surf game context", "not running fs_game surf" );
		warnings++;
	}

	if ( r_fbo && r_fbo->integer ) {
		vk_surf_validation_line( "PASS", "r_fbo", "1" );
	} else {
		vk_surf_validation_line( "FAIL", "r_fbo", "must be 1; execute surf.cfg then vid_restart" );
		failures++;
	}
	if ( taaEnabled ) {
		vk_surf_validation_line( "PASS", "r_taa", "1 (Temporal Reconstruction active)" );
	} else {
		vk_surf_validation_line( "FAIL", "r_taa", "0; Surf shipping path requires 1" );
		failures++;
	}
	if ( r_aaMode && r_aaMode->integer == 4 ) {
		vk_surf_validation_line( "PASS", "r_aaMode", "4 (native Temporal Reconstruction)" );
	} else {
		vk_surf_validation_line( "WARN", "r_aaMode", "expected 4; AA policy may override r_taa" );
		warnings++;
	}
	if ( mode == 1 || mode == 2 ) {
		vk_surf_validation_line( "PASS", "r_weaponTemporalMode",
			mode == 1 ? "1 (classified shared history)" : "2 (independent weapon history)" );
	} else {
		vk_surf_validation_line( "FAIL", "r_weaponTemporalMode",
			"expected 1 or 2; mode 0 is a current-frame correctness baseline" );
		failures++;
	}
	if ( classAvailable ) {
		vk_surf_validation_line( "PASS", "weapon class target", "R8 ping-pong + stamp pipeline available" );
	} else {
		vk_surf_validation_line( classEnabled ? "FAIL" : "WARN", "weapon class target",
			classEnabled ? "classification enabled without a valid class texture/pipeline" :
			"classification disabled" );
		if ( classEnabled ) {
			failures++;
		} else {
			warnings++;
		}
	}
	if ( reactiveEnabled ) {
		vk_surf_validation_line( "PASS", "r_temporalReactiveMask", "1" );
	} else {
		vk_surf_validation_line( "FAIL", "r_temporalReactiveMask",
			"0; weapon silhouettes cannot stamp reactive coverage" );
		failures++;
	}
	if ( reactiveAvailable ) {
		vk_surf_validation_line( "PASS", "weapon reactive target", "R8 target + weapon stamp pipeline available" );
	} else {
		vk_surf_validation_line( reactiveEnabled ? "FAIL" : "WARN", "weapon reactive target",
			reactiveEnabled ? "reactive masking enabled without a reactive target/pipeline" :
			"reactive masking disabled" );
		if ( reactiveEnabled ) {
			failures++;
		} else {
			warnings++;
		}
	}
	if ( velocityEnabled ) {
		vk_surf_validation_line( "PASS", "r_taaMotionVectors", "1" );
	} else {
		vk_surf_validation_line( "FAIL", "r_taaMotionVectors",
			"0; temporal weapon resolve requires weapon MVP velocity" );
		failures++;
	}
	if ( velocityAvailable ) {
		vk_surf_validation_line( "PASS", "weapon MVP velocity path",
			"motion target available; first-person prev MVP capture enabled" );
	} else {
		vk_surf_validation_line( "FAIL", "weapon MVP velocity path",
			"temporal weapon resolve enabled without a motion target" );
		failures++;
	}
	if ( weaponAfter ) {
		vk_surf_validation_line( "PASS", "weapon composition",
			vk_temporal_defer_bloom_for_weapon() ?
				"after world TAA, before one combined HDR bloom" :
				"after world TAA (weapon bloom intentionally disabled)" );
	} else {
		vk_surf_validation_line( "FAIL", "weapon composition",
			"pre-bloom; set r_temporalWeaponAfterTaa 1 and r_weaponSsrIsolation 1" );
		failures++;
	}
	if ( r_weaponSsrIsolation && r_weaponSsrIsolation->integer ) {
		vk_surf_validation_line( "PASS", "r_weaponSsrIsolation", "1" );
	} else {
		vk_surf_validation_line( "FAIL", "r_weaponSsrIsolation",
			"0; weapon depth may contaminate world SSR/SSAO" );
		failures++;
	}
	if ( r_weaponBloomMode && r_weaponBloomMode->integer == 1 ) {
		vk_surf_validation_line( "PASS", "r_weaponBloomMode",
			"1 (weapon composite precedes one combined HDR bloom)" );
	} else if ( r_weaponBloomMode && r_weaponBloomMode->integer == 2 ) {
		if ( vk.weapon_bloom_extract_pipeline != VK_NULL_HANDLE ) {
			vk_surf_validation_line( "PASS", "r_weaponBloomMode",
				"2 (dedicated class-gated weapon bloom after weapon flush)" );
		} else {
			vk_surf_validation_line( "FAIL", "r_weaponBloomMode",
				"2 requested but weapon_bloom_extract_pipeline is missing" );
			failures++;
		}
	} else {
		vk_surf_validation_line( "WARN", "r_weaponBloomMode",
			"Surf default is 1; mode 0 is comparison and mode 2 is dedicated weapon bloom" );
		warnings++;
	}

	if ( previousDepthAvailable ) {
		vk_surf_validation_line( "PASS", "previous depth",
			"dual R32F history + copy pipeline + descriptors available" );
	} else {
		vk_surf_validation_line( "FAIL", "previous depth",
			"required temporal previous-depth resource unavailable" );
		failures++;
	}

	if ( !taaEnabled && mode > 0 ) {
		vk_surf_validation_line( "WARN", "contradictory combination",
			"TAA disabled while weapon temporal mode is enabled" );
		warnings++;
	}
	if ( classEnabled && !classAvailable ) {
		vk_surf_validation_line( "FAIL", "contradictory combination",
			"weapon classification enabled without a valid class texture" );
	}
	if ( taaEnabled && mode > 0 && !velocityAvailable ) {
		vk_surf_validation_line( "FAIL", "contradictory combination",
			"temporal weapon resolve enabled without velocity" );
	}
	if ( reactiveEnabled && !reactiveAvailable ) {
		vk_surf_validation_line( "FAIL", "contradictory combination",
			"reactive masking enabled without a reactive target" );
	}

	ri.Printf( PRINT_ALL, "RESULT: %s (%d fail, %d warn)\n",
		failures ? "FAIL" : warnings ? "WARN" : "PASS", failures, warnings );
	ri.Printf( PRINT_ALL, "==========================================\n" );
}

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
	vk.temporal.worldMatricesCaptured = qfalse;
	vk.temporal.weaponRenderedThisFrame = qfalse;
	/* Phase 6: roll per-frame temporal resolve counters. */
	vk.temporal.worldResolvesLastFrame = vk.temporal.worldResolvesThisFrame;
	vk.temporal.weaponResolvesLastFrame = vk.temporal.weaponResolvesThisFrame;
	vk.temporal.upscaleBlitsLastFrame = vk.temporal.upscaleBlitsThisFrame;
	vk.temporal.worldResolvesThisFrame = 0u;
	vk.temporal.weaponResolvesThisFrame = 0u;
	vk.temporal.upscaleBlitsThisFrame = 0u;
}

/*
===============
Phase 6 — GPU markers + once-per-frame temporal resolve accounting
===============
*/
void vk_temporal_marker_begin( const char *name )
{
	if ( qvkCmdDebugMarkerBeginEXT && vk.cmd && vk.cmd->command_buffer != VK_NULL_HANDLE ) {
		VkDebugMarkerMarkerInfoEXT info;
		Com_Memset( &info, 0, sizeof( info ) );
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		info.pMarkerName = name;
		info.color[0] = 0.15f; info.color[1] = 0.85f; info.color[2] = 0.95f; info.color[3] = 1.0f;
		qvkCmdDebugMarkerBeginEXT( vk.cmd->command_buffer, &info );
	}
}

void vk_temporal_marker_end( void )
{
	if ( qvkCmdDebugMarkerEndEXT && vk.cmd && vk.cmd->command_buffer != VK_NULL_HANDLE ) {
		qvkCmdDebugMarkerEndEXT( vk.cmd->command_buffer );
	}
}

static void vk_temporal_warn_double_resolve( const char *pass, uint32_t count )
{
	static uint32_t lastWarnFrame = ~0u;

	if ( count <= 1u || lastWarnFrame == vk.temporal.frameIndex ) {
		return;
	}
	lastWarnFrame = vk.temporal.frameIndex;
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"[VK][temporal] %s executed %u times in frame %u — history would accumulate "
		"multiple reprojection steps per frame (multi-silhouette ghosting)\n" S_COLOR_WHITE,
		pass, count, vk.temporal.frameIndex );
}

void vk_temporal_note_world_resolve( void )
{
	vk.temporal.worldResolvesThisFrame++;
	vk_temporal_warn_double_resolve( "TemporalResolveWorld", vk.temporal.worldResolvesThisFrame );
}

void vk_temporal_note_weapon_resolve( void )
{
	vk.temporal.weaponResolvesThisFrame++;
	vk_temporal_warn_double_resolve( "TemporalResolveWeapon", vk.temporal.weaponResolvesThisFrame );
}

void vk_temporal_note_upscale_blit( void )
{
	vk.temporal.upscaleBlitsThisFrame++;
	vk_temporal_warn_double_resolve( "TemporalUpscale", vk.temporal.upscaleBlitsThisFrame );
}

/*
===============
vk_temporal_capture_world_viewparms

Snapshot world camera matrices while backEnd still holds the world view.
Must run before deferred weapon / HUD overwrites RDF_NOWORLDMODEL viewParms.
===============
*/
void vk_temporal_capture_world_viewparms( void )
{
	if ( backEnd.viewParms.portalView != PV_NONE ) {
		return;
	}
	Com_Memcpy( vk.temporal.worldViewMatrix, backEnd.viewParms.world.modelViewMatrix,
		sizeof( vk.temporal.worldViewMatrix ) );
	Com_Memcpy( vk.temporal.worldProjectionMatrix, backEnd.viewParms.projectionMatrix,
		sizeof( vk.temporal.worldProjectionMatrix ) );
	vk.temporal.worldMatricesCaptured = qtrue;
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
	/* Track FP projection for sticky camera compare, but do not disable world TAA globally. */
	vk.temporal.firstPersonProjectionThisFrame = qtrue;
}

static void vk_temporal_log_reset( uint32_t reasons, qboolean hardReset )
{
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
	vk.temporal.prevColorValid = qfalse;
	vk.temporal.prevDepthValid = qfalse;
	vk.temporal.prevClassValid = qfalse;
	vk.temporal.prevVelocityValid = qfalse;
	vk.temporal.taaHistoryIndex = 0u;
	vk.temporal.prevDepthIndex = 0u;
	vk.temporal.weaponMatricesValid = qfalse;
	vk.temporal.weaponMatricesHavePrev = qfalse;
	vk.temporal.classHasPrev = qfalse;
	vk_object_id_reset();
	vk_reset_weapon_history();
	vk_temporal_history_note( HISTORY_TAA, qfalse, "taa history reset" );
}

void vk_reset_weapon_history( void )
{
	vk.temporal.weaponHistoryValid = qfalse;
	vk.temporal.weaponHistoryIndex = 0u;
	vk.temporal.weaponHistoryResetSerial++;
	vk_temporal_history_note( HISTORY_WEAPON, qfalse, "weapon history reset" );
}

qboolean vk_temporal_prepare_current_depth( void )
{
	VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.depth_image == VK_NULL_HANDLE ) {
		return qfalse;
	}
	record_depth_image_layout_transition( vk.cmd->command_buffer, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	if ( !vk.msaaActive ) {
		return ( vk.depth_image_view_sample != VK_NULL_HANDLE || vk.depth_image_view != VK_NULL_HANDLE ) ?
			qtrue : qfalse;
	}
	if ( vk.volumetric_depth_image == VK_NULL_HANDLE ||
		vk.volumetric_depth_view == VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}
	vk_resolve_volumetric_depth_msaa();
	return qtrue;
}

qboolean vk_temporal_store_previous_depth( uint32_t writeIndex )
{
	uint32_t width = 0u;
	uint32_t height = 0u;

	writeIndex &= 1u;
	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.temporal_depth_history_copy_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_pipeline_layout == VK_NULL_HANDLE ||
		vk.temporal_depth_copy_descriptor[writeIndex] == VK_NULL_HANDLE ||
		vk.temporal_prev_depth_image[writeIndex] == VK_NULL_HANDLE ) {
		vk.temporal.prevDepthValid = qfalse;
		return qfalse;
	}
	vk_get_active_render_extent( &width, &height );
	if ( width == 0u || height == 0u ) {
		width = vk_get_render_target_width();
		height = vk_get_render_target_height();
	}
	if ( width == 0u || height == 0u ) {
		vk.temporal.prevDepthValid = qfalse;
		return qfalse;
	}

	record_image_layout_transition( vk.cmd->command_buffer,
		vk.temporal_prev_depth_image[writeIndex], VK_IMAGE_ASPECT_COLOR_BIT,
		vk.temporal_prev_depth_layout[writeIndex], VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk.temporal_prev_depth_layout[writeIndex] = VK_IMAGE_LAYOUT_GENERAL;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.temporal_depth_history_copy_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.volumetric_depth_resolve_pipeline_layout, 0, 1,
		&vk.temporal_depth_copy_descriptor[writeIndex], 0, NULL );
	qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7u ) / 8u, ( height + 7u ) / 8u, 1u );

	record_image_layout_transition( vk.cmd->command_buffer,
		vk.temporal_prev_depth_image[writeIndex], VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	vk.temporal_prev_depth_layout[writeIndex] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vk.temporal.prevDepthIndex = writeIndex;
	vk.temporal.prevDepthValid = qtrue;
	return qtrue;
}

qboolean vk_temporal_store_weapon_depth( uint32_t writeIndex )
{
	uint32_t width = 0u;
	uint32_t height = 0u;

	writeIndex &= 1u;
	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.temporal_depth_history_copy_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_pipeline_layout == VK_NULL_HANDLE ||
		vk.weapon_depth_copy_descriptor[writeIndex] == VK_NULL_HANDLE ||
		vk.weapon_prev_depth_image[writeIndex] == VK_NULL_HANDLE ) {
		return qfalse;
	}
	vk_get_active_render_extent( &width, &height );
	if ( width == 0u || height == 0u ) {
		width = vk_get_render_target_width();
		height = vk_get_render_target_height();
	}
	if ( width == 0u || height == 0u ) {
		return qfalse;
	}

	record_image_layout_transition( vk.cmd->command_buffer,
		vk.weapon_prev_depth_image[writeIndex], VK_IMAGE_ASPECT_COLOR_BIT,
		vk.weapon_prev_depth_layout[writeIndex], VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk.weapon_prev_depth_layout[writeIndex] = VK_IMAGE_LAYOUT_GENERAL;
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.temporal_depth_history_copy_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.volumetric_depth_resolve_pipeline_layout, 0, 1,
		&vk.weapon_depth_copy_descriptor[writeIndex], 0, NULL );
	qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7u ) / 8u, ( height + 7u ) / 8u, 1u );
	record_image_layout_transition( vk.cmd->command_buffer,
		vk.weapon_prev_depth_image[writeIndex], VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	vk.weapon_prev_depth_layout[writeIndex] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return qtrue;
}

static qboolean vk_temporal_ensure_depth_reject_stats_buffer( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements memReq;
	VkMemoryAllocateInfo mai;

	if ( vk.temporal_depth_reject_stats_buffer != VK_NULL_HANDLE &&
		vk.temporal_depth_reject_stats_mapped != NULL ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = sizeof( uint32_t ) * 4u;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.temporal_depth_reject_stats_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.temporal_depth_reject_stats_buffer, &memReq );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = memReq.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.temporal_depth_reject_stats_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.temporal_depth_reject_stats_buffer,
		vk.temporal_depth_reject_stats_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.temporal_depth_reject_stats_memory, 0, bci.size, 0,
		&vk.temporal_depth_reject_stats_mapped ) );
	Com_Memset( vk.temporal_depth_reject_stats_mapped, 0, (size_t)bci.size );
	SET_OBJECT_NAME( vk.temporal_depth_reject_stats_buffer,
		"TemporalDepthRejectStatsSSBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static void vk_temporal_update_depth_reject_stats_descriptor( uint32_t prevDepthIndex )
{
	VkDescriptorImageInfo depthInfo;
	VkDescriptorImageInfo prevInfo;
	VkDescriptorImageInfo motionInfo;
	VkDescriptorBufferInfo bufInfo;
	VkWriteDescriptorSet writes[4];
	Vk_Sampler_Def sd;
	VkSampler sampler;

	if ( vk.temporal_depth_reject_stats_descriptor == VK_NULL_HANDLE ||
		vk.temporal_depth_reject_stats_buffer == VK_NULL_HANDLE ) {
		return;
	}

	prevDepthIndex &= 1u;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	sampler = vk_find_sampler( &sd );

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = sampler;
	depthInfo.imageView = ( vk.depth_image_view_sample != VK_NULL_HANDLE ) ?
		vk.depth_image_view_sample : vk.depth_image_view;
	if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
		depthInfo.imageView = vk.volumetric_depth_view;
	}
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &prevInfo, 0, sizeof( prevInfo ) );
	prevInfo.sampler = sampler;
	prevInfo.imageView = vk.temporal_prev_depth_view[prevDepthIndex];
	prevInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &motionInfo, 0, sizeof( motionInfo ) );
	motionInfo.sampler = sampler;
	motionInfo.imageView = vk.motion_vector_view;
	motionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.buffer = vk.temporal_depth_reject_stats_buffer;
	bufInfo.offset = 0;
	bufInfo.range = sizeof( uint32_t ) * 4u;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.temporal_depth_reject_stats_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depthInfo;

	writes[1] = writes[0];
	writes[1].dstBinding = 1;
	writes[1].pImageInfo = &prevInfo;

	writes[2] = writes[0];
	writes[2].dstBinding = 2;
	writes[2].pImageInfo = &motionInfo;

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = vk.temporal_depth_reject_stats_descriptor;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].pBufferInfo = &bufInfo;

	if ( depthInfo.imageView == VK_NULL_HANDLE || prevInfo.imageView == VK_NULL_HANDLE ||
		motionInfo.imageView == VK_NULL_HANDLE ) {
		return;
	}
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
}

void vk_temporal_dispatch_depth_reject_stats( uint32_t prevDepthIndex )
{
	uint32_t width = 0u;
	uint32_t height = 0u;
	float push[4];
	uint32_t *counters;

	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ||
		vk.temporal_depth_reject_stats_pipeline == VK_NULL_HANDLE ||
		vk.temporal_depth_reject_stats_pipeline_layout == VK_NULL_HANDLE ||
		vk.temporal_depth_reject_stats_descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( !vk_temporal_ensure_depth_reject_stats_buffer() ) {
		return;
	}

	counters = (uint32_t *)vk.temporal_depth_reject_stats_mapped;
	counters[0] = counters[1] = counters[2] = counters[3] = 0u;

	vk_temporal_update_depth_reject_stats_descriptor( prevDepthIndex );
	vk_get_active_render_extent( &width, &height );
	if ( width == 0u || height == 0u ) {
		width = vk_get_render_target_width();
		height = vk_get_render_target_height();
	}
	if ( width == 0u || height == 0u ) {
		return;
	}

	push[0] = r_znear ? r_znear->value : 8.0f;
	push[1] = backEnd.viewParms.zFar;
	if ( push[1] <= push[0] ) {
		push[1] = push[0] + 100.0f;
	}
	push[2] = vk.temporal.prevDepthValid ? 1.0f : 0.0f;
	push[3] = 0.04f;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.temporal_depth_reject_stats_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.temporal_depth_reject_stats_pipeline_layout, 0, 1,
		&vk.temporal_depth_reject_stats_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.temporal_depth_reject_stats_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), push );
	qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7u ) / 8u, ( height + 7u ) / 8u, 1u );
}

void vk_temporal_readback_depth_reject_stats( void )
{
	const uint32_t *counters;

	if ( vk.temporal_depth_reject_stats_mapped == NULL ) {
		return;
	}
	counters = (const uint32_t *)vk.temporal_depth_reject_stats_mapped;
	vk.temporal.depthRealRejectCount = counters[0];
	vk.temporal.depthOldApproxWouldPassCount = counters[1];
	vk.temporal.depthBothAgreeRejectCount = counters[2];
	vk.temporal.depthSampleCount = counters[3];
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
	vk_ambient_visibility_reset_history();
	if ( nonCameraReasons != 0u ) {
		vk.adaptedExposure = manualExposure;
		vk.temporal.hasValidLuminance = qfalse;
		vk.temporal.filteredAvgLogLuminance = 0.0f;
	}

	vk_temporal_log_reset( reasons, hardReset );
	vk_spine_cert_check_history_invalidated( reasons );
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
	/* Weapon/HUD after world: RDF_NOWORLDMODEL flips every frame — do not reset TAA history. */
	qboolean noWorldTransition = ( noWorldModel != vk.temporal.noWorldModel ) ? qtrue : qfalse;
	if ( noWorldTransition && backEnd.doneWorldScene && worldValid ) {
		noWorldTransition = qfalse;
	}
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

	/* Phase 3/4: extent + velocity-space report when r_temporalResolutionDebug is set. */
	vk_temporal_resolution_report( qfalse );
}

void vk_temporal_commit_frame_state( void )
{
	const int clientState = ri.CL_GetState ? ri.CL_GetState() : CA_ACTIVE;
	const qboolean worldValid = ( tr.world != NULL ) ? qtrue : qfalse;
	/* Live refdef may be weapon/HUD (RDF_NOWORLDMODEL) after deferred flush — track that
	 * for telemetry only; matrix commit uses the world snapshot when available. */
	const qboolean noWorldModel = ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;
	const float *projection;
	const float *view;
	float viewProj[16];

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

	/*
	 * Commit previous-frame matrices once at frame end — never mid-post (volumetric
	 * used to overwrite these before TAA refreshed postfx, collapsing matrix
	 * reprojection to identity and corrupting history UV for pixels without MVs).
	 *
	 * Prefer matrices captured at world draw time: RB_FlushDeferredWeaponAfterTaa
	 * replaces backEnd.viewParms with a first-person RDF_NOWORLDMODEL camera.
	 */
	/* Phase 2/10: CPU reprojection probe runs against the still-previous matrices
	 * before they are overwritten below. */
	vk_temporal_velocity_probe();

	if ( worldValid && backEnd.doneWorldScene && vk.temporal.worldMatricesCaptured ) {
		projection = vk.temporal.worldProjectionMatrix;
		view = vk.temporal.worldViewMatrix;
		myGlMultMatrix( view, projection, viewProj );
		Com_Memcpy( vk_prev_view_matrix, view, sizeof( vk_prev_view_matrix ) );
		Com_Memcpy( vk_prev_projection_matrix, projection, sizeof( vk_prev_projection_matrix ) );
		Com_Memcpy( vk_prev_viewproj_matrix, viewProj, sizeof( vk_prev_viewproj_matrix ) );
		vk_prev_matrices_valid = qtrue;
		vk_prev_matrices_frame = vk.temporal.frameIndex;
		R_Upscale_GetJitter( &vk_prev_jitter_x, &vk_prev_jitter_y );
		vk_prev_jitter_valid = qtrue;
	} else if ( worldValid && backEnd.doneWorldScene && !noWorldModel &&
		backEnd.viewParms.portalView == PV_NONE ) {
		projection = backEnd.viewParms.projectionMatrix;
		view = backEnd.viewParms.world.modelViewMatrix;
		myGlMultMatrix( view, projection, viewProj );
		Com_Memcpy( vk_prev_view_matrix, view, sizeof( vk_prev_view_matrix ) );
		Com_Memcpy( vk_prev_projection_matrix, projection, sizeof( vk_prev_projection_matrix ) );
		Com_Memcpy( vk_prev_viewproj_matrix, viewProj, sizeof( vk_prev_viewproj_matrix ) );
		vk_prev_matrices_valid = qtrue;
		vk_prev_matrices_frame = vk.temporal.frameIndex;
		R_Upscale_GetJitter( &vk_prev_jitter_x, &vk_prev_jitter_y );
		vk_prev_jitter_valid = qtrue;
	}
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
		float meterScale = vk_exposure_histogram_meter_scale();
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

		/* Use doneWorldScene — HUD/weapon may set RDF_NOWORLDMODEL after the world view. */
		if ( !stableGameplayState || !tr.world || !backEnd.doneWorldScene ) {
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
				vk_exposure_histogram_notify_luminance( avgLogLum, qtrue );
				target *= meterScale;
				if ( vk_exposure_histogram_active() ) {
					const vkExposureHistogramState_t *hst = vk_exposure_histogram_state();
					if ( hst && hst->fixedExposure ) {
						targetExp = manualExposure;
						luminanceValid = qfalse;
					}
				}
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
				vk_temporal_history_note( HISTORY_EXPOSURE, qtrue, NULL );

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
				vk_temporal_history_note( HISTORY_EXPOSURE, qfalse, "exposure reset" );
			}
			targetExp = manualExposure;
		}

		if ( cameraCut && significantCut ) {
			cvar_t *cap_var = ri.Cvar_Get( "r_exposure_auto_cap_on_cut", "1.35", 0 );
			float cap = cap_var ? cap_var->value : 1.35f;
			speed = 0.5f;
			vk_exposure_histogram_on_camera_cut();
			if ( cap > 0.0f && targetExp > cap ) {
				targetExp = cap;
			}
		}

		if ( !hardReset && vk.adaptedExposure > 0.0f ) {
			/*
			 * Asymmetric eye adaptation:
			 *   target < current → scene brighter → darken faster (speedDown)
			 *   target > current → scene darker  → brighten slower (speedUp)
			 * Exponential: current += (target - current) * (1 - exp(-rate * dt))
			 * speedUp/Down already include a ~16ms frame factor from cvar * 0.016.
			 */
			float darkenForBrightSceneRate = speedDown;
			float brightenForDarkSceneRate = speedUp;
			float rate = ( targetExp < vk.adaptedExposure ) ? darkenForBrightSceneRate : brightenForDarkSceneRate;
			float ratio = targetExp / vk.adaptedExposure;
			if ( ratio < 0.75f ) {
				rate = ( rate < darkenForBrightSceneRate ) ? darkenForBrightSceneRate : rate;
			} else if ( ratio > 1.5f ) {
				rate = ( rate < brightenForDarkSceneRate ) ? brightenForDarkSceneRate : rate;
			}
			speed = 1.0f - expf( -rate );
		}

		if ( stateTransition ) {
			speed = ( speed < 0.30f ) ? 0.30f : speed;
		}

		{
			cvar_t *freeze = ri.Cvar_Get( "r_autoExposureFreeze", "0", 0 );
			if ( !( freeze && freeze->integer ) ) {
				vk.adaptedExposure += ( targetExp - vk.adaptedExposure ) * speed;
				vk.adaptedExposure = ( vk.adaptedExposure < minExposure ) ? minExposure :
					( vk.adaptedExposure > maxExposure ? maxExposure : vk.adaptedExposure );
			}
		}
	}
}

qboolean vk_temporal_reconstruction_wanted( void )
{
	cvar_t *r_tsr;

	if ( !vk.fboActive ) {
		return qfalse;
	}
	/*
	 * r_taa is the hard enable. Do not keep reconstruction alive from a stale
	 * latched r_aaMode 3–5 after the user sets r_taa 0 (Surf residual shading /
	 * AZ-decal echoes). AA policy sets r_taa 1 when applying modes 3–5 at init.
	 */
	if ( r_taa && r_taa->integer ) {
		/* IQ P1-G: ghost isolation can disable TAA without clearing the cvar permanently. */
		const int iso = vk_ghost_isolation_mode();
		if ( iso == 1 || iso == 7 ) {
			return qfalse;
		}
		return qtrue;
	}
	r_tsr = ri.Cvar_Get( "r_tsr", "1", CVAR_ARCHIVE_ND );
	if ( r_tsr && !r_tsr->integer ) {
		return qfalse;
	}
	/* Upscale temporal (r_upscale 2) may still request history with r_taa 0. */
	if ( R_Upscale_WantTemporal() ) {
		return qtrue;
	}
	return qfalse;
}

qboolean vk_temporal_want_weapon_after_taa( void )
{
	if ( !vk_temporal_reconstruction_wanted() ) {
		return qfalse;
	}
	if ( !r_temporalWeaponAfterTaa || !r_temporalWeaponAfterTaa->integer ) {
		return qfalse;
	}
	return qtrue;
}

qboolean vk_temporal_want_weapon_after_world_post( void )
{
	if ( vk_temporal_want_weapon_after_taa() ) {
		return qtrue;
	}
	/*
	 * IQ P0-C: COLOR_PIPELINE requires weapon HDR before bloom. When bloom mode 1
	 * (combined extract) is active, defer weapon until after world post even if
	 * TAA reconstruction is off, so bloom can run after the weapon flush.
	 */
	if ( vk.fboActive &&
		r_bloom && r_bloom->integer &&
		r_weaponBloomMode && r_weaponBloomMode->integer == 1 &&
		r_temporalWeaponAfterTaa && r_temporalWeaponAfterTaa->integer ) {
		return qtrue;
	}
	/*
	 * Architecture B for unsupported view-model consumers: world SSR/SSAO
	 * finish first, then the weapon is composited by the existing deferred
	 * weapon pass. This keeps DEPTH_RANGE_WEAPON color/depth out of those
	 * passes without guessing a device-depth threshold that could also reject
	 * nearby world geometry.
	 */
	if ( vk.fboActive &&
		r_weaponSsrIsolation && r_weaponSsrIsolation->integer ) {
		if ( PostFX_SSR_IsEnabled() ) {
			return qtrue;
		}
		/* SSAO also samples depth; keep weapon out when AO is live. */
		if ( ri.Cvar_VariableIntegerValue( "r_ssao" ) &&
			ri.Cvar_VariableIntegerValue( "r_temporalAO" ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

qboolean vk_temporal_defer_bloom_for_weapon( void )
{
	/* Mode 1: combined HDR before one global bloom (weapon must be present).
	 * Mode 2 keeps legacy world bloom order and adds dedicated weapon bloom after the flush. */
	return vk.fboActive && r_bloom && r_bloom->integer &&
		r_weaponBloomMode && r_weaponBloomMode->integer == 1 &&
		vk_temporal_want_weapon_after_world_post();
}

qboolean vk_temporal_want_dedicated_weapon_bloom( void )
{
	return vk.fboActive && r_bloom && r_bloom->integer &&
		r_weaponBloomMode && r_weaponBloomMode->integer == 2 &&
		vk_temporal_want_weapon_after_world_post();
}

qboolean vk_temporal_defer_weapon_drawsurfs( const void *drawSurfsCmd )
{
	return RB_TryDeferWeaponDrawSurfs( (const drawSurfsCommand_t *)drawSurfsCmd );
}

void vk_temporal_flush_deferred_weapon_after_taa( VkImageView *post_fog_src, VkImageView *luminance_src )
{
	RB_FlushDeferredWeaponAfterTaa( post_fog_src, luminance_src );
}

static void vk_temporal_format_reasons( uint32_t reasons, char *buf, size_t bufSize )
{
	char *ptr;
	char *end;
	int i;

	if ( !buf || bufSize == 0u ) {
		return;
	}
	ptr = buf;
	end = buf + bufSize;
	*ptr = '\0';
	if ( reasons == 0u ) {
		Q_strncpyz( buf, "none", bufSize );
		return;
	}
	for ( i = 0; i < (int)ARRAY_LEN( knownReasons ); i++ ) {
		const uint32_t reason = knownReasons[i];
		const char *name;
		size_t len;

		if ( !( reasons & reason ) ) {
			continue;
		}
		name = vk_temporal_reason_string( reason );
		if ( ptr != buf && ptr + 1 < end ) {
			*ptr++ = ',';
			*ptr = '\0';
		}
		len = strlen( name );
		if ( ptr + len >= end ) {
			break;
		}
		Com_Memcpy( ptr, name, len );
		ptr += len;
		*ptr = '\0';
	}
	if ( buf[0] == '\0' ) {
		Q_strncpyz( buf, "unknown", bufSize );
	}
}

/*
===============
vk_temporal_status_f

Dump shared temporal ownership: reset reasons, history validity, weapon deferral,
and world-matrix capture (Spine 1.0 diagnostics).
===============
*/
void vk_temporal_status_f( void )
{
	char appliedBuf[256];
	char stickyBuf[256];
	char pendingBuf[256];
	const qboolean recon = vk_temporal_reconstruction_wanted();
	const qboolean weaponAfter = vk_temporal_want_weapon_after_world_post();

	vk_temporal_format_reasons( vk.temporal.appliedResetReasons, appliedBuf, sizeof( appliedBuf ) );
	vk_temporal_format_reasons( vk.temporal.stickyResetReasons, stickyBuf, sizeof( stickyBuf ) );
	vk_temporal_format_reasons( vk.temporal.pendingResetReasons, pendingBuf, sizeof( pendingBuf ) );

	ri.Printf( PRINT_ALL, "======== Temporal Ownership Status ========\n" );
	ri.Printf( PRINT_ALL, "frame     : %u prepared=%s\n",
		vk.temporal.frameIndex, vk.temporal.preparedThisFrame ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "wanted    : reconstruction=%s weaponAfterWorldPost=%s (taaCvar=%d ssrIsolation=%d)\n",
		recon ? "yes" : "no",
		weaponAfter ? "yes" : "no",
		r_temporalWeaponAfterTaa ? r_temporalWeaponAfterTaa->integer : 0,
		r_weaponSsrIsolation ? r_weaponSsrIsolation->integer : 0 );
	ri.Printf( PRINT_ALL,
		"taa gate  : world=%d doneWorld=%d portal=%d fpStable=%d resetBlock=%d nearStatic=%d "
		"pipelines=%d framebuffers=%d descriptors=%d "
		"last={wanted:%d allow:%d depth:%d gate:0x%x missing:0x%x}\n",
		tr.world != NULL, backEnd.doneWorldScene,
		backEnd.viewParms.portalView != PV_NONE,
		vk.temporal.firstPersonProjectionThisFrame == vk.temporal.firstPersonProjectionLastFrame,
		vk_temporal_has_reason( VK_TEMPORAL_RESET_CAMERA_CUT | VK_TEMPORAL_RESET_MISSING_PREV_DATA |
			VK_TEMPORAL_RESET_RENDERER_INIT | VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE |
			VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE | VK_TEMPORAL_RESET_WORLD_CHANGE |
			VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE ),
		vk_temporal_near_static_streak_guard(),
		vk.taa_pipeline != VK_NULL_HANDLE && vk.temporal_depth_history_copy_pipeline != VK_NULL_HANDLE,
		vk.framebuffers.taa[0] != VK_NULL_HANDLE && vk.framebuffers.taa[1] != VK_NULL_HANDLE,
		vk.taa_history_descriptor[0] != VK_NULL_HANDLE &&
			vk.temporal_prev_depth_descriptor[0] != VK_NULL_HANDLE,
		vk.temporal.lastTaaWanted, vk.temporal.lastTaaAllowed,
		vk.temporal.lastTaaDepthReady, vk.temporal.lastTaaGateMask,
		vk.temporal.lastTaaMissingMask );
	ri.Printf( PRINT_ALL, "history   : taa=%s idx=%u luminance=%s motionPrev=%s\n",
		vk.temporal.hasValidTAAHistory ? "valid" : "reset",
		vk.temporal.taaHistoryIndex,
		vk.temporal.hasValidLuminance ? "valid" : "reset",
		vk_prev_matrices_valid ? "valid" : "reset" );
	ri.Printf( PRINT_ALL,
		"bindings  : missingClass=%u missingReactive=%u fallback=%u forcedReject=%u\n",
		vk.temporal.missingClassDescriptorFrames,
		vk.temporal.missingReactiveDescriptorFrames,
		vk.temporal.fallbackTextureUsageFrames,
		vk.temporal.forcedHistoryRejectFrames );
	ri.Printf( PRINT_ALL,
		"validity  : color=%d depth=%d class=%d velocity=%d weapon=%d\n",
		vk.temporal.prevColorValid, vk.temporal.prevDepthValid,
		vk.temporal.prevClassValid, vk.temporal.prevVelocityValid,
		vk.temporal.weaponHistoryValid );
	ri.Printf( PRINT_ALL,
		"frame IDs : color={%llu,%llu} depth={%llu,%llu} class={%llu,%llu} "
		"weapon={%llu,%llu} weaponDepth={%llu,%llu}\n",
		(unsigned long long)vk.temporal.taaHistoryFrameId[0],
		(unsigned long long)vk.temporal.taaHistoryFrameId[1],
		(unsigned long long)vk.temporal.prevDepthFrameId[0],
		(unsigned long long)vk.temporal.prevDepthFrameId[1],
		(unsigned long long)vk.temporal.classFrameId[0],
		(unsigned long long)vk.temporal.classFrameId[1],
		(unsigned long long)vk.temporal.weaponHistoryFrameId[0],
		(unsigned long long)vk.temporal.weaponHistoryFrameId[1],
		(unsigned long long)vk.temporal.weaponDepthFrameId[0],
		(unsigned long long)vk.temporal.weaponDepthFrameId[1] );
	ri.Printf( PRINT_ALL,
		"resources : extent=%ux%u colorFmt=%d depthFmt=R32F classFmt=R8 "
		"sets={depth:%s class:%s reactive:%s weapon:%s}\n",
		vk_get_render_target_width(), vk_get_render_target_height(), (int)vk.color_format,
		vk.temporal_prev_depth_descriptor[vk.temporal.prevDepthIndex & 1u] ? "bound" : "missing",
		vk.taa_class_descriptor[vk.cmd_index] ? "bound" : "missing",
		vk.taa_reactive_descriptor[vk.cmd_index] ? "bound" : "missing",
		vk.weapon_history_descriptor[vk.temporal.weaponHistoryIndex & 1u] ? "bound" : "missing" );
	ri.Printf( PRINT_ALL, "timing    : independent weapon resolve %.3f ms GPU\n",
		vk.temporal.weaponResolveGpuMs );
	ri.Printf( PRINT_ALL, "resets    : applied=%s sticky=%s pending=%s cameraCut=%s\n",
		appliedBuf, stickyBuf, pendingBuf,
		vk.temporal.sharedCameraCut ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "consumers : motion+taa+volumetric+occlusion+AV (via apply_resets)\n" );
	ri.Printf( PRINT_ALL, "world     : name=%s valid=%s noWorld=%s gameplay=%s\n",
		vk.temporal.worldName[0] ? vk.temporal.worldName : "<none>",
		vk.temporal.worldWasValid ? "yes" : "no",
		vk.temporal.noWorldModel ? "yes" : "no",
		vk.temporal.stableGameplayState ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "matrices  : worldCaptured=%s fpProj=%s (this=%s last=%s)\n",
		vk.temporal.worldMatricesCaptured ? "yes" : "no",
		vk.temporal.firstPersonProjectionThisFrame ? "yes" : "no",
		vk.temporal.firstPersonProjectionThisFrame ? "1" : "0",
		vk.temporal.firstPersonProjectionLastFrame ? "1" : "0" );
	ri.Printf( PRINT_ALL, "extent    : render=%ux%u swapchain=%ux%u nearStaticFrames=%d\n",
		vk.temporal.lastRenderWidth, vk.temporal.lastRenderHeight,
		vk.temporal.lastSwapchainWidth, vk.temporal.lastSwapchainHeight,
		vk_near_static_view_frames );
	ri.Printf( PRINT_ALL, "motion    : unreliable=%s viewClass=%s\n",
		vk.temporal.unreliableMotionThisFrame ? "yes" : "no",
		vk_view_class_name( vk_classify_current_view() ) );
	ri.Printf( PRINT_ALL, "debug     : temporalDebug=%d historyRejection=%d motionVectors=%d\n",
		r_temporalDebug ? r_temporalDebug->integer : 0,
		r_debugHistoryRejection ? r_debugHistoryRejection->integer : 0,
		r_debugMotionVectors ? r_debugMotionVectors->integer : 0 );
	ri.Printf( PRINT_ALL, "            r_temporalDebug: 0=off 1=velocity 2=depthReject 3=histWeight\n" );
	ri.Printf( PRINT_ALL, "            4=disocclusion 5=weaponMask 6=currVsHist\n" );
	ri.Printf( PRINT_ALL, "            7=histUV 8=worldVsReactive 9=adaptSample 10=currVsHist\n" );
	ri.Printf( PRINT_ALL, "            11=neighVar 12=histDelta 13=NaN 14=preWeaponMV 15=priorClassMV\n" );
	ri.Printf( PRINT_ALL, "            16–27 class/velocity/reactive/confidence (weapon resolve)\n" );
	ri.Printf( PRINT_ALL, "            28–33 depth/prev-depth/rejection (weapon resolve)\n" );
	ri.Printf( PRINT_ALL, "            rejection viz (r_debugHistoryRejection): same codes 1–12\n" );
	vk_temporal_readback_depth_reject_stats();
	ri.Printf( PRINT_ALL, "depthRej  : real=%u oldApproxWouldPass=%u bothAgree=%u samples=%u\n",
		vk.temporal.depthRealRejectCount,
		vk.temporal.depthOldApproxWouldPassCount,
		vk.temporal.depthBothAgreeRejectCount,
		vk.temporal.depthSampleCount );
	ri.Printf( PRINT_ALL, "policy    : RDF_NOWORLDMODEL after doneWorldScene does not thrash history;\n" );
	ri.Printf( PRINT_ALL, "            weapon draws defer after world TAA and after SSR when isolation is on;\n" );
	ri.Printf( PRINT_ALL, "            portals force camera-cut; commit prefers worldMatricesCaptured.\n" );
	ri.Printf( PRINT_ALL, "present   : adaptive=%s frame_generation=off presentation_source=current_simulation_frame\n",
		( r_aaMode && r_aaMode->integer == 3 ) ? "yes (aaMode 3)" :
		( r_presentAdaptiveRecon && r_presentAdaptiveRecon->integer ) ? "flag" : "no" );
	ri.Printf( PRINT_ALL, "===========================================\n" );
}

void vk_capture_temporal_debug_f( void )
{
	int mode;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: r_captureTemporalDebug <1..33>\n" );
		return;
	}
	mode = atoi( ri.Cmd_Argv( 1 ) );
	if ( mode < 1 || mode > 33 ) {
		ri.Printf( PRINT_WARNING, "r_captureTemporalDebug: mode must be 1..33\n" );
		return;
	}
	ri.Cvar_Set( "r_temporalDebug", va( "%d", mode ) );
	ri.Cmd_ExecuteText( EXEC_APPEND,
		va( "wait 2; screenshot temporal_debug_%02d\n", mode ) );
	ri.Printf( PRINT_ALL, "[VK][temporal] queued temporal_debug_%02d capture\n", mode );
}

/*
===============
vk_temporal_ghost_status_f

Pass inventory for first-person weapon trail bisect. Prints which temporal /
temporal-adjacent consumers are live so a moving/turning camera test can
isolate the first offending pass without enabling blur clamps.
===============
*/
void vk_temporal_ghost_status_f( void )
{
	const qboolean recon = vk_temporal_reconstruction_wanted();
	const qboolean ssr = PostFX_SSR_IsEnabled();
	const int taa = r_taa ? r_taa->integer : 0;
	const int aa = r_aaMode ? r_aaMode->integer : 0;
	const int tsr = ri.Cvar_VariableIntegerValue( "r_tsr" );
	const int temporalSSR = ri.Cvar_VariableIntegerValue( "r_temporalSSR" );
	const int temporalAO = ri.Cvar_VariableIntegerValue( "r_temporalAO" );
	const int temporalFog = ri.Cvar_VariableIntegerValue( "r_temporalFog" );
	const int temporalTransparency = ri.Cvar_VariableIntegerValue( "r_temporalTransparency" );
	const int bloom = ri.Cvar_VariableIntegerValue( "r_bloom" );
	const int motionBlur = ri.Cvar_VariableIntegerValue( "r_motionBlur" );
	const int dof = ri.Cvar_VariableIntegerValue( "r_depthOfField" ) ||
		ri.Cvar_VariableIntegerValue( "r_dof" );
	const int sharpen = ( atof( ri.Cvar_VariableString( "r_sharpen" ) ) > 0.0 ) ? 1 : 0;
	const int ssao = ri.Cvar_VariableIntegerValue( "r_ssao" );
	const int fog = ri.Cvar_VariableIntegerValue( "r_volumetricFog" );
	const int oit = ri.Cvar_VariableIntegerValue( "r_oit" );
	const int dbg = r_temporalDebug ? r_temporalDebug->integer : 0;
	const float fogTemporalWeight = atof( ri.Cvar_VariableString( "r_volumetricFogTemporalWeight" ) );

	ri.Printf( PRINT_ALL, "======== Temporal Ghost Bisect ========\n" );
	ri.Printf( PRINT_ALL, "reconstruction : %s (r_taa=%d r_aaMode=%d r_tsr=%d weaponAfterWorldPost=%s)\n",
		recon ? "ON" : "OFF", taa, aa, tsr,
		vk_temporal_want_weapon_after_world_post() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "consumers      : SSR=%s (r_ssr/r_temporalSSR=%d/%d) SSAO=%s (%d/%d)\n",
		ssr ? "ON" : "off", ri.Cvar_VariableIntegerValue( "r_ssr" ), temporalSSR,
		( ssao && temporalAO ) ? "ON" : "off", ssao, temporalAO );
	ri.Printf( PRINT_ALL, "               : fogTemporal=%s (fog=%d gate=%d) transparencyReactive=%s (oit=%d)\n",
		( fog && temporalFog && fogTemporalWeight > 0.0f ) ? "ON" : "off",
		fog, temporalFog,
		( temporalTransparency && ri.Cvar_VariableIntegerValue( "r_temporalReactiveMask" ) ) ? "ON" : "off",
		oit );
	ri.Printf( PRINT_ALL, "post           : bloom=%d motionBlur=%d dof=%d sharpen=%d\n",
		bloom, motionBlur, dof, sharpen );
	ri.Printf( PRINT_ALL, "weapon depth   : DEPTH_RANGE_WEAPON viewport [0.6..1.0] reverse-Z (see vk_view_state)\n" );
	ri.Printf( PRINT_ALL, "debug          : r_temporalDebug=%d  (1 MV 2 reject 3 weight 4 disocc 5 weapon 6 curr/hist 13 NaN)\n",
		dbg );
	ri.Printf( PRINT_ALL, "bisect order   : 1) r_temporalSSR 0  2) r_taa 0 + r_tsr 0  3) r_temporalFog 0\n" );
	ri.Printf( PRINT_ALL, "               : 4) r_temporalAO 0  5) r_temporalTransparency 0  6) r_bloom/r_motionBlur/r_dof 0\n" );
	if ( !recon && ssr && vk_temporal_want_weapon_after_world_post() ) {
		ri.Printf( PRINT_ALL, "live finding   : reconstruction OFF + SSR ON + isolation ON\n" );
		ri.Printf( PRINT_ALL, "               : weapon is deferred; SSR/SSAO receive world-only color/depth.\n" );
	} else if ( !recon && ssr ) {
		ri.Printf( PRINT_ALL, "live finding   : reconstruction OFF + SSR ON → SSR samples post-weapon depth\n" );
		ri.Printf( PRINT_ALL, "               : (DEPTH_RANGE_WEAPON [0.6..1.0]); primary screen-space contaminant.\n" );
	} else if ( !recon && !ssr && vk_temporal_want_weapon_after_world_post() ) {
		ri.Printf( PRINT_ALL, "live finding   : reconstruction OFF + isolation ON (SSAO and/or other depth consumers)\n" );
		ri.Printf( PRINT_ALL, "               : weapon is deferred past incompatible world post.\n" );
	} else if ( !recon && !ssr ) {
		ri.Printf( PRINT_ALL, "live finding   : reconstruction OFF + SSR off → residual silhouette echoes are NOT\n" );
		ri.Printf( PRINT_ALL, "               : from TAA/SSR; check FP projection / multi-part weapon draws next.\n" );
	} else if ( recon ) {
		ri.Printf( PRINT_ALL, "live finding   : reconstruction ON → use r_temporalDebug 1–6; confirm weaponAfterTaa.\n" );
	} else {
		ri.Printf( PRINT_ALL, "live finding   : see docs/RENDERER_TEMPORAL_GHOSTING.md\n" );
	}
	ri.Printf( PRINT_ALL, "docs           : docs/RENDERER_TEMPORAL_GHOSTING.md\n" );
	ri.Printf( PRINT_ALL, "=========================================\n" );
}

void vk_print_weapon_presentation_f( void )
{
	const int bloomMode = r_weaponBloomMode ? r_weaponBloomMode->integer : 0;
	const int temporalMode = r_weaponTemporalMode ? r_weaponTemporalMode->integer : 0;
	const char *bloomName =
		bloomMode == 1 ? "combined HDR before one global bloom" :
		bloomMode == 2 ? "dedicated class-gated weapon bloom" :
		"legacy world bloom (no weapon bloom)";
	const char *fogSrc = ( r_weaponAnalyticFog && r_weaponAnalyticFog->integer ) ?
		"analytic camera-space (weapon only)" : "none (weapon inherits no volumetric history)";
	const char *reflSrc = ( r_weaponLocalReflection && r_weaponLocalReflection->integer ) ?
		"weapon-local probe/IBL" : "shared IBL only; world SSR disabled for weapon";
	const char *aoSrc = ( r_weaponLocalAO && r_weaponLocalAO->integer ) ?
		"weapon-local non-temporal contact" : "material AO only; no world temporal AO";

	ri.Printf( PRINT_ALL, "======== Weapon Presentation Policy ========\n" );
	ri.Printf( PRINT_ALL, "temporal mode : %d (r_weaponTemporalMode) compare=%d\n",
		temporalMode, r_weaponTemporalCompare ? r_weaponTemporalCompare->integer : 0 );
	ri.Printf( PRINT_ALL, "bloom         : mode %d — %s\n", bloomMode, bloomName );
	ri.Printf( PRINT_ALL, "exposure      : shared scene HDR epoch (single tonemap/grade after bloom)\n" );
	ri.Printf( PRINT_ALL, "fog           : %s (r_weaponAnalyticFog=%d)\n",
		fogSrc, r_weaponAnalyticFog ? r_weaponAnalyticFog->integer : 0 );
	ri.Printf( PRINT_ALL, "reflections   : %s (r_weaponLocalReflection=%d)\n",
		reflSrc, r_weaponLocalReflection ? r_weaponLocalReflection->integer : 0 );
	ri.Printf( PRINT_ALL, "AO            : %s (r_weaponLocalAO=%d)\n",
		aoSrc, r_weaponLocalAO ? r_weaponLocalAO->integer : 0 );
	ri.Printf( PRINT_ALL, "readability   : %s intensity=%.3f\n",
		( r_weaponReadabilityLight && r_weaponReadabilityLight->integer ) ? "ON" : "off",
		r_weaponReadabilityLightIntensity ? r_weaponReadabilityLightIntensity->value : 0.0f );
	ri.Printf( PRINT_ALL, "thin sights   : rejectScale=%.3f (r_weaponThinSightReject)\n",
		r_weaponThinSightReject ? r_weaponThinSightReject->value : 0.0f );
	ri.Printf( PRINT_ALL, "isolation     : weapon deferred past world SSR/SSAO/volumetric history\n" );
	ri.Printf( PRINT_ALL, "history valid : weapon=%s worldColor=%s prevDepth=%s class=%s\n",
		vk.temporal.weaponHistoryValid ? "yes" : "no",
		vk.temporal.prevColorValid ? "yes" : "no",
		vk.temporal.prevDepthValid ? "yes" : "no",
		vk.temporal.prevClassValid ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "=============================================\n" );
}

/*
===============================================================================
Phase 1/3/4 — canonical velocity space + extent report (vk_velocity_space.h)
===============================================================================
*/

const char *vk_velocity_space_name( vkVelocitySpace_t space )
{
	switch ( space ) {
		case VK_VELOCITY_SPACE_UV: return "UV [0,1] @ render target";
		case VK_VELOCITY_SPACE_PIXELS: return "pixels (derived, tagged extent)";
		case VK_VELOCITY_SPACE_NDC: return "NDC [-1,1] (2x UV — never stored)";
		default: return "?";
	}
}

static void vk_temporal_extent_ratio_warn( const char *what,
	uint32_t aW, uint32_t aH, uint32_t bW, uint32_t bH )
{
	float rw, rh;

	if ( aW == 0u || aH == 0u || bW == 0u || bH == 0u || ( aW == bW && aH == bH ) ) {
		return;
	}
	rw = (float)aW / (float)bW;
	rh = (float)aH / (float)bH;
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"[VK][temporal] %s extent %ux%u != render extent %ux%u (ratio %.3fx%.3f) — "
		"velocity sampled across mismatched extents scales reprojection by that ratio\n"
		S_COLOR_WHITE, what, aW, aH, bW, bH, rw, rh );
}

void vk_temporal_resolution_report( qboolean force )
{
	static cvar_t *resDebug = NULL;
	static uint32_t lastHash = 0u;
	uint32_t sceneW = vk_get_render_target_width();
	uint32_t sceneH = vk_get_render_target_height();
	uint32_t activeW = 0u, activeH = 0u;
	uint32_t swapW = vk.swapchain_extent_valid ? vk.swapchain_extent.width : 0u;
	uint32_t swapH = vk.swapchain_extent_valid ? vk.swapchain_extent.height : 0u;
	/* Motion vectors / TAA history / prev depth are created via
	 * vk_create_fullres_color_attachment at the main color extent. */
	uint32_t velocityW = ( vk.motion_vector_image != VK_NULL_HANDLE ) ? vk.mainColorWidth : 0u;
	uint32_t velocityH = ( vk.motion_vector_image != VK_NULL_HANDLE ) ? vk.mainColorHeight : 0u;
	uint32_t historyW = ( vk.taa_history_image[0] != VK_NULL_HANDLE ) ? vk.mainColorWidth : 0u;
	uint32_t historyH = ( vk.taa_history_image[0] != VK_NULL_HANDLE ) ? vk.mainColorHeight : 0u;
	uint32_t hash;

	if ( !resDebug ) {
		resDebug = ri.Cvar_Get( "r_temporalResolutionDebug", "0", CVAR_TEMP );
	}
	if ( !force && ( !resDebug || !resDebug->integer ) ) {
		return;
	}
	vk_get_active_render_extent( &activeW, &activeH );

	hash = sceneW * 73856093u ^ sceneH * 19349663u ^ activeW * 83492791u ^ activeH * 2654435761u ^
		swapW * 374761393u ^ swapH * 668265263u ^ velocityW * 2246822519u ^ velocityH * 3266489917u;
	if ( !force && hash == lastHash ) {
		return;
	}
	lastHash = hash;

	ri.Printf( PRINT_ALL, "======== Temporal Resolution / Velocity Space ========\n" );
	ri.Printf( PRINT_ALL, "  scene render extent    : %ux%u (vk_get_render_target_*)\n", sceneW, sceneH );
	ri.Printf( PRINT_ALL, "  active viewport extent : %ux%u (vk.renderWidth/Height)\n", activeW, activeH );
	ri.Printf( PRINT_ALL, "  velocity buffer extent : %ux%u (%s)\n", velocityW, velocityH,
		vk.motion_vector_image != VK_NULL_HANDLE ? "R16G16_SFLOAT" : "not allocated" );
	ri.Printf( PRINT_ALL, "  TAA input extent       : %ux%u (HDR color)\n", vk.mainColorWidth, vk.mainColorHeight );
	ri.Printf( PRINT_ALL, "  TAA output extent      : %ux%u (history %s)\n", historyW, historyH,
		vk.taa_history_image[0] != VK_NULL_HANDLE ? "allocated" : "missing" );
	ri.Printf( PRINT_ALL, "  display/swapchain      : %ux%u window=%dx%d\n", swapW, swapH,
		gls.windowWidth, gls.windowHeight );
	ri.Printf( PRINT_ALL, "  r_renderScale=%d r_renderWidth=%d r_renderHeight=%d r_upscale=%s\n",
		r_renderScale ? r_renderScale->integer : 0,
		r_renderWidth ? r_renderWidth->integer : 0,
		r_renderHeight ? r_renderHeight->integer : 0,
		R_Upscale_WantTemporal() ? "2 (temporal)" : "off/spatial" );
	{
		float jx = 0.0f, jy = 0.0f;
		R_Upscale_GetJitter( &jx, &jy );
		ri.Printf( PRINT_ALL, "  jitter now=(%.4f,%.4f)px prevCommitted=(%.4f,%.4f)px\n",
			jx, jy, vk_prev_jitter_x, vk_prev_jitter_y );
	}
	ri.Printf( PRINT_ALL, "  velocity space         : %s (canonical)\n",
		vk_velocity_space_name( VK_VELOCITY_SPACE_CANONICAL ) );
	ri.Printf( PRINT_ALL, "  convention             : out_motion = currentUV - previousUV; historyUV = sampleUV - motion\n" );
	ri.Printf( PRINT_ALL, "  applied velocity scale : 1.0 (no pass may rescale stored motion)\n" );
	ri.Printf( PRINT_ALL, "  prev matrices          : %s (frame %u, current frame %u, age %d)\n",
		vk_prev_matrices_valid ? "valid" : "INVALID",
		vk_prev_matrices_frame, vk.temporal.frameIndex,
		vk_prev_matrices_valid ? (int)( vk.temporal.frameIndex - vk_prev_matrices_frame ) : -1 );
	ri.Printf( PRINT_ALL, "  resolves last frame    : world=%u weapon=%u upscaleBlit=%u\n",
		vk.temporal.worldResolvesLastFrame, vk.temporal.weaponResolvesLastFrame,
		vk.temporal.upscaleBlitsLastFrame );
	ri.Printf( PRINT_ALL, "======================================================\n" );

	vk_temporal_extent_ratio_warn( "velocity buffer", velocityW, velocityH, sceneW, sceneH );
	vk_temporal_extent_ratio_warn( "TAA history", historyW, historyH, sceneW, sceneH );
	if ( vk_prev_matrices_valid && vk.temporal.frameIndex > vk_prev_matrices_frame + 1u ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][temporal] previous matrices are %u frames old (expected 1) — camera velocity "
			"is exaggerated by that factor\n" S_COLOR_WHITE,
			vk.temporal.frameIndex - vk_prev_matrices_frame );
	}
}

void vk_temporal_resolution_status_f( void )
{
	vk_temporal_resolution_report( qtrue );
}

/*
===============================================================================
Phase 2/10 — CPU reprojection probe

Projects a fixed world-space point through the current (captured world) and
previous frame matrices using exactly the shader conversion chain
(clip → NDC → *0.5+0.5 → UV → pixels), then cross-checks:
  1. previous matrices are exactly one temporal frame old,
  2. reconstructed pixel displacement via canonical UV equals the reference
     NDC*0.5 conversion (flags 2x / 4x / 0.5x / 0.25x scale bugs),
  3. velocity-buffer extent matches the render extent used for pixel units.
===============================================================================
*/
void vk_temporal_velocity_probe( void )
{
	static cvar_t *probe = NULL;
	static uint32_t lastPrintFrame = 0u;
	float currProjVk[16], prevProjVk[16];
	float currVP[16], prevVP[16];
	float currClip[4], prevClip[4];
	float point[4];
	vec3_t p;
	float currNdc[2], prevNdc[2], currUV[2], prevUV[2];
	float velocityUV[2], velocityPx[2], ndcDeltaPx[2];
	uint32_t extW, extH;
	int i;

	if ( !probe ) {
		probe = ri.Cvar_Get( "r_temporalVelocityProbe", "0", CVAR_TEMP );
	}
	if ( !probe->integer ) {
		return;
	}
	if ( !vk_prev_matrices_valid || !vk.temporal.worldMatricesCaptured || !tr.world ) {
		return;
	}

	/* Fixed probe point 512 units along the current view forward. */
	VectorMA( tr.refdef.vieworg, 512.0f, tr.refdef.viewaxis[0], p );
	point[0] = p[0]; point[1] = p[1]; point[2] = p[2]; point[3] = 1.0f;

	vk_get_projection_matrix_vk( vk.temporal.worldProjectionMatrix, currProjVk );
	myGlMultMatrix( vk.temporal.worldViewMatrix, currProjVk, currVP );
	vk_get_projection_matrix_vk( vk_prev_projection_matrix, prevProjVk );
	myGlMultMatrix( vk_prev_view_matrix, prevProjVk, prevVP );

	for ( i = 0; i < 4; i++ ) {
		currClip[i] = point[0] * currVP[i] + point[1] * currVP[4 + i] +
			point[2] * currVP[8 + i] + point[3] * currVP[12 + i];
		prevClip[i] = point[0] * prevVP[i] + point[1] * prevVP[4 + i] +
			point[2] * prevVP[8 + i] + point[3] * prevVP[12 + i];
	}
	if ( currClip[3] < 1e-4f || prevClip[3] < 1e-4f ) {
		return;
	}

	currNdc[0] = currClip[0] / currClip[3];
	currNdc[1] = currClip[1] / currClip[3];
	prevNdc[0] = prevClip[0] / prevClip[3];
	prevNdc[1] = prevClip[1] / prevClip[3];
	/* Canonical NDC → UV conversion (the 0.5 factor under audit in Phase 2). */
	currUV[0] = currNdc[0] * 0.5f + 0.5f;
	currUV[1] = currNdc[1] * 0.5f + 0.5f;
	prevUV[0] = prevNdc[0] * 0.5f + 0.5f;
	prevUV[1] = prevNdc[1] * 0.5f + 0.5f;
	velocityUV[0] = currUV[0] - prevUV[0];
	velocityUV[1] = currUV[1] - prevUV[1];

	extW = vk_get_render_target_width();
	extH = vk_get_render_target_height();
	velocityPx[0] = velocityUV[0] * (float)extW;
	velocityPx[1] = velocityUV[1] * (float)extH;
	/* Measured displacement in pixels (direct projection reference). */
	ndcDeltaPx[0] = ( currNdc[0] - prevNdc[0] ) * 0.5f * (float)extW;
	ndcDeltaPx[1] = ( currNdc[1] - prevNdc[1] ) * 0.5f * (float)extH;

	{
		/*
		 * Independent reconstruction: replay taa.frag's reprojectHistoryUV()
		 * (UV+depth → invViewProj → world → prevViewProj → prevUV). A missing
		 * or doubled 0.5 NDC↔UV factor anywhere in that chain shows up as a
		 * 2x / 4x / 0.5x / 0.25x ratio against the direct projection above.
		 */
		float invVP[16];
		float reconPrevUV[2];
		float reconVelPx[2] = { 0.0f, 0.0f };
		qboolean reconValid = qfalse;

		if ( vk_mat4_inverse( currVP, invVP ) ) {
			float depthNdc = currClip[2] / currClip[3];
			float posClip[4], posWorld[4], prevClip2[4];
			posClip[0] = currUV[0] * 2.0f - 1.0f;
			posClip[1] = currUV[1] * 2.0f - 1.0f;
			posClip[2] = depthNdc;
			posClip[3] = 1.0f;
			for ( i = 0; i < 4; i++ ) {
				posWorld[i] = posClip[0] * invVP[i] + posClip[1] * invVP[4 + i] +
					posClip[2] * invVP[8 + i] + posClip[3] * invVP[12 + i];
			}
			if ( fabsf( posWorld[3] ) > 1e-6f ) {
				posWorld[0] /= posWorld[3]; posWorld[1] /= posWorld[3];
				posWorld[2] /= posWorld[3]; posWorld[3] = 1.0f;
				for ( i = 0; i < 4; i++ ) {
					prevClip2[i] = posWorld[0] * prevVP[i] + posWorld[1] * prevVP[4 + i] +
						posWorld[2] * prevVP[8 + i] + posWorld[3] * prevVP[12 + i];
				}
				if ( prevClip2[3] > 1e-4f ) {
					reconPrevUV[0] = prevClip2[0] / prevClip2[3] * 0.5f + 0.5f;
					reconPrevUV[1] = prevClip2[1] / prevClip2[3] * 0.5f + 0.5f;
					reconVelPx[0] = ( currUV[0] - reconPrevUV[0] ) * (float)extW;
					reconVelPx[1] = ( currUV[1] - reconPrevUV[1] ) * (float)extH;
					reconValid = qtrue;
				}
			}
		}

		float lenUVpx = sqrtf( velocityPx[0] * velocityPx[0] + velocityPx[1] * velocityPx[1] );
		float lenRefPx = reconValid ?
			sqrtf( reconVelPx[0] * reconVelPx[0] + reconVelPx[1] * reconVelPx[1] ) : lenUVpx;
		float ratio = ( lenRefPx > 1e-4f ) ? lenUVpx / lenRefPx : 1.0f;
		uint32_t age = vk.temporal.frameIndex - vk_prev_matrices_frame;
		qboolean anomalous = qfalse;
		const char *scaleWarn = NULL;

		if ( ratio > 1.8f && ratio < 2.2f ) { scaleWarn = "2x"; }
		else if ( ratio > 3.6f && ratio < 4.4f ) { scaleWarn = "4x"; }
		else if ( ratio > 0.45f && ratio < 0.55f ) { scaleWarn = "0.5x"; }
		else if ( ratio > 0.22f && ratio < 0.28f ) { scaleWarn = "0.25x"; }
		else if ( ratio < 0.95f || ratio > 1.05f ) { scaleWarn = "non-unit"; }
		if ( scaleWarn ) {
			anomalous = qtrue;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][temporal][probe] velocity scale ratio %.3f (%s) — UV chain and NDC*0.5 "
				"reference disagree; check NDC/UV conversion in producers\n" S_COLOR_WHITE,
				ratio, scaleWarn );
		}
		if ( age != 1u && lenRefPx > 0.25f ) {
			anomalous = qtrue;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][temporal][probe] previous matrices are %u frames old (expected 1) — "
				"camera velocity exaggerated %ux\n" S_COLOR_WHITE, age, age );
		}

		if ( probe->integer >= 2 || anomalous ||
			( vk.temporal.frameIndex - lastPrintFrame ) >= 60u ) {
			lastPrintFrame = vk.temporal.frameIndex;
			ri.Printf( PRINT_ALL,
				"[VK][temporal][probe] frame=%u prevAge=%u extent=%ux%u\n"
				"  measured displacement : %.3f, %.3f px\n"
				"  encoded velocity (UV) : %.6f, %.6f\n"
				"  encoded velocity (px) : %.3f, %.3f\n"
				"  reconstructed (px)    : %.3f, %.3f\n"
				"  error ratio           : %.4f\n",
				vk.temporal.frameIndex, age, extW, extH,
				ndcDeltaPx[0], ndcDeltaPx[1],
				velocityUV[0], velocityUV[1],
				velocityPx[0], velocityPx[1],
				reconValid ? reconVelPx[0] : velocityPx[0],
				reconValid ? reconVelPx[1] : velocityPx[1],
				ratio );
		}
	}
}
