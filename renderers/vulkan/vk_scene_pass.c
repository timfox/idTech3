#include "tr_local.h"
#include "vk.h"
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

void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

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
		return;
	}

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
		return;
	}

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

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || vk.inRenderPass ) {
		return;
	}
	if ( vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}

	switch ( vk.renderPassIndex ) {
	case RENDER_PASS_SCREENMAP:
		frameBuffer = vk.framebuffers.screenmap;
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.screenmap, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	case RENDER_PASS_POST_BLOOM:
		frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.post_bloom, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	case RENDER_PASS_UI_OVERLAY:
		frameBuffer = vk.framebuffers.ui_overlay[ vk.cmd->swapchain_image_index ];
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.ui_overlay, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	case RENDER_PASS_MAIN:
	default:
		frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];
		if ( frameBuffer != VK_NULL_HANDLE ) {
			vk_begin_render_pass_tracked( vk.render_pass.main, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
		}
		break;
	}

	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}
