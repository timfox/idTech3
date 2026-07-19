#include "tr_local.h"
#include "vk.h"
#include "vk_post_fog.h"
#include "vk_pass_registry.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_util.h"

static qboolean s_device_lost_context_reported = qfalse;

void vk_get_active_render_extent( uint32_t *width, uint32_t *height )
{
	if ( width ) {
		*width = ( vk.renderWidth > 0 ) ? (uint32_t)vk.renderWidth :
			( glConfig.vidWidth > 0 ? (uint32_t)glConfig.vidWidth : 1u );
	}
	if ( height ) {
		*height = ( vk.renderHeight > 0 ) ? (uint32_t)vk.renderHeight :
			( glConfig.vidHeight > 0 ? (uint32_t)glConfig.vidHeight : 1u );
	}
}

static void vk_configure_scene_pass_dimensions( void )
{
	uint32_t logicalWidth = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
	uint32_t logicalHeight = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
	uint32_t targetWidth = logicalWidth;
	uint32_t targetHeight = logicalHeight;

	if ( vk.fboActive ) {
		if ( vk.mainColorWidth > 0u && vk.mainColorHeight > 0u ) {
			targetWidth = vk.mainColorWidth;
			targetHeight = vk.mainColorHeight;
		}
	} else {
		if ( vk.swapchain_extent_valid && vk.swapchain_extent.width > 0 && vk.swapchain_extent.height > 0 ) {
			targetWidth = vk.swapchain_extent.width;
			targetHeight = vk.swapchain_extent.height;
		} else {
			targetWidth = ( gls.windowWidth > 0 ) ? (uint32_t)gls.windowWidth : logicalWidth;
			targetHeight = ( gls.windowHeight > 0 ) ? (uint32_t)gls.windowHeight : logicalHeight;
		}
	}

	vk.renderWidth = targetWidth;
	vk.renderHeight = targetHeight;
	vk.renderScaleX = ( logicalWidth > 0 ) ? ( (float)targetWidth / (float)logicalWidth ) : 1.0f;
	vk.renderScaleY = ( logicalHeight > 0 ) ? ( (float)targetHeight / (float)logicalHeight ) : 1.0f;
}

static const char *vk_scene_pass_name( renderPass_t pass )
{
	switch ( pass ) {
	case RENDER_PASS_MAIN:
		return "main";
	case RENDER_PASS_SCREENMAP:
		return "screenmap";
	case RENDER_PASS_SUN_SHADOW:
		return "sun_shadow";
	case RENDER_PASS_POST_BLOOM:
		return "post_bloom";
	case RENDER_PASS_UI_OVERLAY:
		return "ui_overlay";
	case RENDER_PASS_CUBEMAP:
		return "cubemap";
	default:
		return "unknown_pass";
	}
}

void vk_pass_diag_reset( void )
{
	vk.passDiag.lastBegunPass[0] = '\0';
	vk.passDiag.lastEndedPass[0] = '\0';
	vk.passDiag.lastPostStage[0] = '\0';
	vk.passDiag.lastResumeTarget[0] = '\0';
	vk.passDiag.lastResumeSelfHeal = qfalse;
	vk.passDiag.inContinuationPass = qfalse;
	vk.passDiag.lastPassWidth = 0;
	vk.passDiag.lastPassHeight = 0;
}

void vk_pass_diag_begin( const char *passName, uint32_t width, uint32_t height )
{
	Q_strncpyz( vk.passDiag.lastBegunPass, passName ? passName : "unknown", sizeof( vk.passDiag.lastBegunPass ) );
	vk.passDiag.lastPassWidth = width;
	vk.passDiag.lastPassHeight = height;
	if ( passName && ( !Q_stricmp( passName, "post_bloom" ) || !Q_stricmp( passName, "ui_overlay" ) ) ) {
		vk.passDiag.inContinuationPass = qtrue;
	} else if ( passName && !Q_stricmp( passName, "main" ) ) {
		vk.passDiag.inContinuationPass = qfalse;
	}
	vk_spine_pass_begin_named( passName, width, height );
}

void vk_pass_diag_end( const char *passName )
{
	Q_strncpyz( vk.passDiag.lastEndedPass, passName ? passName : "unknown", sizeof( vk.passDiag.lastEndedPass ) );
	vk_spine_pass_end_named( passName );
}

void vk_pass_diag_stage( const char *stageName )
{
	Q_strncpyz( vk.passDiag.lastPostStage, stageName ? stageName : "unknown", sizeof( vk.passDiag.lastPostStage ) );
}

void vk_pass_diag_resume( const char *targetName, qboolean selfHeal )
{
	Q_strncpyz( vk.passDiag.lastResumeTarget, targetName ? targetName : "unknown", sizeof( vk.passDiag.lastResumeTarget ) );
	vk.passDiag.lastResumeSelfHeal = selfHeal;
	vk_pass_diag_stage( "resume" );
}

void vk_assert_ui_pass_consistency( const char *where )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}
	if ( vk.uiOverlayActive && vk.inRenderPass && vk.renderPassIndex != RENDER_PASS_UI_OVERLAY ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][scene] %s: uiOverlayActive=1 while in-pass %s (expected ui_overlay)\n",
			where ? where : "assert_ui",
			vk_scene_pass_name( vk.renderPassIndex ) );
	}
}

void vk_report_device_lost_context( const char *where )
{
	if ( s_device_lost_context_reported ) {
		return;
	}
	s_device_lost_context_reported = qtrue;

	ri.Printf( PRINT_ALL, S_COLOR_RED
		"[VK][device_lost] at %s\n",
		where ? where : "unknown" );
	ri.Printf( PRINT_ALL,
		"[VK][device_lost] profile mode=%d fbo=%d hdr=%d bloom=%d pbr=%d forwardPlus=%d deferredGBuffer=%d smaa=%d taa=%d\n",
		r_renderMode ? r_renderMode->integer : -1,
		r_fbo ? r_fbo->integer : -1,
		r_hdr ? r_hdr->integer : -1,
		r_bloom ? r_bloom->integer : -1,
		r_pbr ? r_pbr->integer : -1,
		r_forwardPlus ? r_forwardPlus->integer : -1,
		r_deferredGBuffer ? r_deferredGBuffer->integer : -1,
		r_ext_smaa ? r_ext_smaa->integer : -1,
		r_taa ? r_taa->integer : -1 );
	ri.Printf( PRINT_ALL,
		"[VK][device_lost] pass begun=%s ended=%s stage=%s resume=%s selfHeal=%s inPass=%s continuation=%s extent=%ux%u\n",
		vk.passDiag.lastBegunPass[0] ? vk.passDiag.lastBegunPass : "(none)",
		vk.passDiag.lastEndedPass[0] ? vk.passDiag.lastEndedPass : "(none)",
		vk.passDiag.lastPostStage[0] ? vk.passDiag.lastPostStage : "(none)",
		vk.passDiag.lastResumeTarget[0] ? vk.passDiag.lastResumeTarget : "(none)",
		vk.passDiag.lastResumeSelfHeal ? "yes" : "no",
		vk.inRenderPass ? "yes" : "no",
		vk.passDiag.inContinuationPass ? "yes" : "no",
		vk.passDiag.lastPassWidth, vk.passDiag.lastPassHeight );
	ri.Printf( PRINT_ALL,
		"[VK][device_lost] renderTarget=%ux%u activePass=%s postFog=%s\n",
		vk.renderWidth, vk.renderHeight,
		vk_scene_pass_name( vk.renderPassIndex ),
		vk_post_fog_source_name( vk_get_post_fog_source() ) );
	vk_spine_dump_device_lost();
}

void vk_fatal_device_lost( const char *where, VkResult res )
{
	vk.device_lost = qtrue;
	vk_report_device_lost_context( where );
	ri.Error( ERR_FATAL, "Vulkan GPU lost at %s (%s)",
		where ? where : "unknown",
		vk_result_string( res ) );
}

static void vk_scene_pass_validate_begin( const char *op, renderPass_t targetPass, VkRenderPass renderPass, VkFramebuffer frameBuffer )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}

	if ( vk.inRenderPass ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][scene] %s: expected no active render pass before entering %s, still in %s\n",
			op ? op : "begin",
			vk_scene_pass_name( targetPass ),
			vk_scene_pass_name( vk.renderPassIndex ) );
	}

	if ( renderPass == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][scene] %s: render pass handle is null for %s\n",
			op ? op : "begin",
			vk_scene_pass_name( targetPass ) );
	}

	if ( frameBuffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][scene] %s: framebuffer is null for %s\n",
			op ? op : "begin",
			vk_scene_pass_name( targetPass ) );
	}
}

static void vk_scene_pass_validate_resume( void )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}

	if ( vk.inRenderPass ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][scene] resume_current_render_pass: expected out-of-pass state, still in %s\n",
			vk_scene_pass_name( vk.renderPassIndex ) );
	}

	if ( vk.renderPassIndex == RENDER_PASS_UI_OVERLAY && !vk.uiOverlayActive ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][scene] resume_current_render_pass: ui overlay resume requested but uiOverlayActive=0\n" );
	}
}

static VkFramebuffer vk_scene_pass_resume_framebuffer( renderPass_t *pass )
{
	renderPass_t target = pass ? *pass : vk.renderPassIndex;
	VkFramebuffer frameBuffer = VK_NULL_HANDLE;

	if ( !vk.cmd || vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return VK_NULL_HANDLE;
	}

	switch ( target ) {
	case RENDER_PASS_SCREENMAP:
		frameBuffer = vk.framebuffers.screenmap;
		break;
	case RENDER_PASS_POST_BLOOM:
		frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];
		break;
	case RENDER_PASS_UI_OVERLAY:
		if ( !vk.uiOverlayActive ) {
			if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
				ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
					"[VK][scene] resume_current_render_pass: auto-restoring uiOverlayActive for UI overlay resume\n" );
			}
			vk.uiOverlayActive = qtrue;
		}
		frameBuffer = vk.framebuffers.ui_overlay[ vk.cmd->swapchain_image_index ];
		break;
	case RENDER_PASS_MAIN:
	default:
		target = RENDER_PASS_MAIN;
		frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];
		break;
	}

	if ( frameBuffer == VK_NULL_HANDLE && target != RENDER_PASS_MAIN ) {
		if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][scene] resume_current_render_pass: framebuffer missing for %s, falling back to main\n",
				vk_scene_pass_name( target ) );
		}
		target = RENDER_PASS_MAIN;
		frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];
	}

	if ( pass ) {
		*pass = target;
	}
	return frameBuffer;
}

void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk_scene_pass_validate_begin( "begin_main_render_pass", RENDER_PASS_MAIN, vk.render_pass.main, frameBuffer );
	vk.renderPassIndex = RENDER_PASS_MAIN;

	vk_configure_scene_pass_dimensions();
	vk_reset_scene_src_rect_tracking();
	vk_pass_diag_begin( "main", vk.renderWidth, vk.renderHeight );

	/* Color clear is gated by r_vk_clearhdr via loadOp; depth always clears. */
	vk_begin_render_pass_tracked( vk.render_pass.main, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void vk_begin_post_bloom_render_pass( void )
{
	VkFramebuffer frameBuffer;

	if ( !vk.cmd || vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}

	frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];
	if ( frameBuffer == VK_NULL_HANDLE ) {
		vk_scene_pass_validate_begin( "begin_post_bloom_render_pass", RENDER_PASS_POST_BLOOM, vk.render_pass.post_bloom, frameBuffer );
		vk_pass_diag_stage( "post_bloom_skip_null_framebuffer" );
		return;
	}

	/* Leaving UI overlay continuation: scene draws go to color_image again. */
	if ( vk.renderPassIndex == RENDER_PASS_UI_OVERLAY || vk.uiOverlayActive ) {
		vk.uiOverlayActive = qfalse;
		vk_pass_diag_stage( "leave_ui_overlay" );
	}
	vk_assert_ui_pass_consistency( "begin_post_bloom_render_pass" );

	vk_scene_pass_validate_begin( "begin_post_bloom_render_pass", RENDER_PASS_POST_BLOOM, vk.render_pass.post_bloom, frameBuffer );
	vk.renderPassIndex = RENDER_PASS_POST_BLOOM;
	vk_configure_scene_pass_dimensions();
	vk_pass_diag_begin( "post_bloom", vk.renderWidth, vk.renderHeight );
	vk_pass_diag_stage( "post_bloom_begin" );

	vk_begin_render_pass_tracked( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void vk_begin_ui_overlay_render_pass( void )
{
	VkFramebuffer frameBuffer;

	if ( !vk.cmd || vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}

	frameBuffer = vk.framebuffers.ui_overlay[ vk.cmd->swapchain_image_index ];
	if ( frameBuffer == VK_NULL_HANDLE ) {
		vk_scene_pass_validate_begin( "begin_ui_overlay_render_pass", RENDER_PASS_UI_OVERLAY, vk.render_pass.ui_overlay, frameBuffer );
		return;
	}

	vk_scene_pass_validate_begin( "begin_ui_overlay_render_pass", RENDER_PASS_UI_OVERLAY, vk.render_pass.ui_overlay, frameBuffer );
	vk.renderPassIndex = RENDER_PASS_UI_OVERLAY;
	vk_configure_scene_pass_dimensions();
	vk.uiOverlayActive = qtrue;
	vk.uiOverlayContentValid = qtrue;
	vk_pass_diag_begin( "ui_overlay", vk.renderWidth, vk.renderHeight );

	vk_begin_render_pass_tracked( vk.render_pass.ui_overlay, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

/*
===============
vk_begin_ui_overlay_render_pass_load
===============
Resume UI overlay without clearing (preserve HUD after mid-frame bloom).
*/
void vk_begin_ui_overlay_render_pass_load( void )
{
	VkFramebuffer frameBuffer;

	if ( !vk.cmd || vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}

	frameBuffer = vk.framebuffers.ui_overlay[ vk.cmd->swapchain_image_index ];
	if ( frameBuffer == VK_NULL_HANDLE ) {
		vk_scene_pass_validate_begin( "begin_ui_overlay_render_pass_load", RENDER_PASS_UI_OVERLAY, vk.render_pass.ui_overlay, frameBuffer );
		return;
	}

	vk_scene_pass_validate_begin( "begin_ui_overlay_render_pass_load", RENDER_PASS_UI_OVERLAY, vk.render_pass.ui_overlay, frameBuffer );
	vk.renderPassIndex = RENDER_PASS_UI_OVERLAY;
	vk_configure_scene_pass_dimensions();
	vk.uiOverlayActive = qtrue;
	vk.uiOverlayContentValid = qtrue;
	vk_pass_diag_begin( "ui_overlay", vk.renderWidth, vk.renderHeight );
	vk_pass_diag_stage( "ui_overlay_load" );

	vk_begin_render_pass_tracked( vk.render_pass.ui_overlay, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

/*
===============
vk_resume_current_render_pass
===============
Resume the active scene render pass after out-of-pass compute (e.g. vegetation wind)
without clearing color/depth attachments.
*/
void vk_resume_current_render_pass( void )
{
	VkFramebuffer frameBuffer;
	renderPass_t requestedPass;
	renderPass_t targetPass;
	qboolean selfHeal;

	vk_scene_pass_validate_resume();

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || vk.inRenderPass ) {
		return;
	}
	if ( vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}

	requestedPass = vk.renderPassIndex;
	targetPass = requestedPass;
	frameBuffer = vk_scene_pass_resume_framebuffer( &targetPass );
	selfHeal = ( targetPass != requestedPass ) ? qtrue : qfalse;
	vk.renderPassIndex = targetPass;
	vk_pass_diag_resume( vk_scene_pass_name( targetPass ), selfHeal );
	vk_pass_diag_begin( vk_scene_pass_name( targetPass ), vk.renderWidth, vk.renderHeight );

	switch ( targetPass ) {
	case RENDER_PASS_SCREENMAP:
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.screenmap, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	case RENDER_PASS_POST_BLOOM:
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	case RENDER_PASS_UI_OVERLAY:
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.ui_overlay, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	case RENDER_PASS_MAIN:
	default:
		if ( frameBuffer != VK_NULL_HANDLE ) {
			/* Main render pass uses CLEAR loadOps (r_vk_clearhdr). Mid-frame
			 * resumes after out-of-pass compute (G-buffer fill, visbuf, etc.)
			 * must LOAD prior color/depth via main_resume. */
			VkRenderPass resumePass = ( vk.fboActive && vk.render_pass.main_resume != VK_NULL_HANDLE )
				? vk.render_pass.main_resume
				: vk.render_pass.main;
			vk_begin_render_pass_tracked( resumePass, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	}

	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void vk_resume_main_render_pass( void )
{
	vk.renderPassIndex = RENDER_PASS_MAIN;
	vk_resume_current_render_pass();
}
