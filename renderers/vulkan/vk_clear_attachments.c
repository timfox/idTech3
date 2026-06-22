/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

In-render-pass color/depth clears and dynamic color write mask.
Split from vk.c.
===========================================================================
*/

#include "tr_local.h"

void vk_clear_color( const vec4_t color )
{
	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	vk_get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}

void vk_set_color_write_mask( qboolean r, qboolean g, qboolean b, qboolean a )
{
	VkColorComponentFlags mask;

	if ( !vk.active || !vk.colorWriteMaskDynamic || !qvkCmdSetColorWriteMaskEXT )
		return;

	mask = 0;
	if ( r ) mask |= VK_COLOR_COMPONENT_R_BIT;
	if ( g ) mask |= VK_COLOR_COMPONENT_G_BIT;
	if ( b ) mask |= VK_COLOR_COMPONENT_B_BIT;
	if ( a ) mask |= VK_COLOR_COMPONENT_A_BIT;

	qvkCmdSetColorWriteMaskEXT( vk.cmd->command_buffer, 0, 1, &mask );
}

void vk_clear_depth( qboolean clear_stencil )
{
	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.depthStencil.depth = 0.0f;
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	vk_get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}
