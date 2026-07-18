#include "tr_local.h"
#include "vk.h"
#include "vk_post_fog.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"

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

	if ( !vk.fboActive ) {
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
		return;
	}

	vk_scene_pass_validate_begin( "begin_post_bloom_render_pass", RENDER_PASS_POST_BLOOM, vk.render_pass.post_bloom, frameBuffer );
	vk.renderPassIndex = RENDER_PASS_POST_BLOOM;
	vk_configure_scene_pass_dimensions();

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

	vk_begin_render_pass_tracked( vk.render_pass.ui_overlay, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
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
	renderPass_t targetPass;

	vk_scene_pass_validate_resume();

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || vk.inRenderPass ) {
		return;
	}
	if ( vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}

	targetPass = vk.renderPassIndex;
	frameBuffer = vk_scene_pass_resume_framebuffer( &targetPass );
	vk.renderPassIndex = targetPass;

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
			vk_begin_render_pass_tracked( vk.render_pass.main, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
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
