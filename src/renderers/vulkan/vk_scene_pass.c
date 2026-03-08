#include "tr_local.h"
#include "vk.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"

void vk_begin_main_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.main[ vk.cmd->swapchain_image_index ];

	vk.renderPassIndex = RENDER_PASS_MAIN;

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
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
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;

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
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;
	vk.uiOverlayActive = qtrue;

	vk_begin_render_pass_tracked( vk.render_pass.ui_overlay, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}
