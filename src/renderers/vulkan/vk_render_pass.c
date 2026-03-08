#include "tr_local.h"
#include "vk.h"
#include "vk_render_pass.h"

void vk_set_fullscreen_viewport_scissor( uint32_t width, uint32_t height )
{
	VkViewport viewport;
	VkRect2D scissor;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
}

void vk_begin_render_pass_tracked( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[5];

	if ( width == 0 ) {
		width = 1u;
	}
	if ( height == 0 ) {
		height = 1u;
	}

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		uint32_t clear_count = 2;

		Com_Memset( clear_values, 0, sizeof( clear_values ) );
		clear_values[0].color.float32[0] = 0.0f;
		clear_values[0].color.float32[1] = 0.0f;
		clear_values[0].color.float32[2] = 0.0f;
		clear_values[0].color.float32[3] = 1.0f;
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		if ( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {
			if ( vk.fboActive ) {
				clear_values[2].color.float32[0] = 0.0f;
				clear_values[2].color.float32[1] = 0.0f;
				clear_values[2].color.float32[2] = 0.0f;
				clear_values[2].color.float32[3] = 0.0f;
				clear_count = vk.msaaActive ? 5 : 3;
			} else {
				clear_count = vk.msaaActive ? 3 : 2;
			}
		} else if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
			clear_values[2].color.float32[0] = 0.0f;
			clear_values[2].color.float32[1] = 0.0f;
			clear_values[2].color.float32[2] = 0.0f;
			clear_values[2].color.float32[3] = 0.0f;
			clear_count = ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) ? 5 : 3;
		} else if ( vk.renderPassIndex == RENDER_PASS_SUN_SHADOW ) {
			clear_count = 2;
		} else {
			clear_count = vk.msaaActive ? 3 : 2;
		}
		render_pass_begin_info.clearValueCount = clear_count;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk.inRenderPass = qtrue;

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}

void vk_end_render_pass_tracked( void )
{
	if ( !vk.inRenderPass ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		vk.inRenderPass = qfalse;
		return;
	}

	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk.inRenderPass = qfalse;
}
