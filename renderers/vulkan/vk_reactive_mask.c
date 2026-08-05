/*
===========================================================================
Temporal reactive mask buffer: clear, stamp (OIT reveal), barriers, pipelines.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_reactive_mask.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_pipeline_helpers.h"
#include "vk_util.h"
#include "vk_upscale.h"
#include "vk_post_fog.h"
#include "vk_scene_pass.h"
#include "vk_pass_registry.h"
#include "vk_forward_plus.h"

#include "vk_raster_ultra.h"

static int s_reactive_mask_cleared_frame = -1;

qboolean vk_reactive_mask_wanted( void )
{
	if ( !vk.fboActive ) {
		return qfalse;
	}
	if ( r_taa && r_taa->integer ) {
		return qtrue;
	}
	if ( R_Upscale_WantTemporal() ) {
		return qtrue;
	}
	if ( r_aaMode && r_aaMode->integer >= 3 && r_aaMode->integer <= 5 ) {
		return qtrue;
	}
	if ( ri.Cvar_VariableIntegerValue( "r_reactiveMaskForce" ) ) {
		return qtrue;
	}
	if ( VK_RasterUltra_Active() ) {
		if ( ri.Cvar_VariableIntegerValue( "r_oit" ) ||
			ri.Cvar_VariableIntegerValue( "r_gpuParticles" ) ||
			ri.Cvar_VariableIntegerValue( "r_distortion" ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

qboolean vk_reactive_mask_active( void )
{
	return vk.reactive_mask_image != VK_NULL_HANDLE && vk.reactive_mask_view != VK_NULL_HANDLE;
}

qboolean vk_reactive_mask_stamp_enabled( void )
{
	cvar_t *r_temporalTransparency;

	if ( !vk_reactive_mask_active() ) {
		return qfalse;
	}
	if ( !r_temporalReactiveMask || !r_temporalReactiveMask->integer ) {
		return qfalse;
	}
	r_temporalTransparency = ri.Cvar_Get( "r_temporalTransparency", "1", CVAR_ARCHIVE_ND );
	if ( r_temporalTransparency && !r_temporalTransparency->integer ) {
		return qfalse;
	}
	return qtrue;
}

void vk_barrier_reactive_mask_for_sampling( const char *reason )
{
	VkImageMemoryBarrier barrier;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk_reactive_mask_active() ) {
		return;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vk.reactive_mask_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.reactive_mask_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	vk.reactive_mask_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vk_spine_note_layout( VK_SPINE_RES_REACTIVE_MASK, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	if ( r_fboDebug && r_fboDebug->integer >= 2 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] reactive-mask barrier (%s): image=0x%llx\n",
			reason ? reason : "unspecified",
			(unsigned long long)(uintptr_t)vk.reactive_mask_image );
	}
}

void vk_barrier_reactive_mask_for_storage( const char *reason )
{
	VkImageMemoryBarrier barrier;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk_reactive_mask_active() ) {
		return;
	}
	if ( vk.reactive_mask_layout == VK_IMAGE_LAYOUT_GENERAL ) {
		return;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vk.reactive_mask_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.reactive_mask_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	vk.reactive_mask_layout = VK_IMAGE_LAYOUT_GENERAL;
	vk_spine_note_layout( VK_SPINE_RES_REACTIVE_MASK, VK_IMAGE_LAYOUT_GENERAL );
	(void)reason;
}

void vk_reactive_mask_clear( void )
{
	VkClearColorValue clear_value;
	VkImageSubresourceRange range;
	VkImageMemoryBarrier barrier;

	if ( !vk_reactive_mask_stamp_enabled() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( s_reactive_mask_cleared_frame == tr.frameCount ) {
		return;
	}
	s_reactive_mask_cleared_frame = tr.frameCount;

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vk.reactive_mask_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.reactive_mask_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	Com_Memset( &clear_value, 0, sizeof( clear_value ) );
	Com_Memset( &range, 0, sizeof( range ) );
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = 1;
	range.layerCount = 1;
	qvkCmdClearColorImage( vk.cmd->command_buffer, vk.reactive_mask_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range );

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	vk.reactive_mask_layout = VK_IMAGE_LAYOUT_GENERAL;
	vk_spine_pass_begin( VK_SPINE_PASS_REACTIVE_MASK );
	vk_spine_note_clear( VK_SPINE_RES_REACTIVE_MASK, VK_SPINE_PASS_REACTIVE_MASK );
	vk_spine_note_barrier( VK_SPINE_RES_REACTIVE_MASK, VK_SPINE_PASS_REACTIVE_MASK, "reactive_clear" );
	vk_spine_pass_end( VK_SPINE_PASS_REACTIVE_MASK );
}

void vk_reactive_mask_stamp_from_reveal( void )
{
	uint32_t width = 0;
	uint32_t height = 0;
	VkImageMemoryBarrier barrier;

	if ( !vk_reactive_mask_stamp_enabled() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.reactive_stamp == VK_NULL_HANDLE ||
		vk.framebuffers.reactive_stamp == VK_NULL_HANDLE ||
		vk.reactive_stamp_pipeline == VK_NULL_HANDLE ||
		vk.pipeline_layout_reactive_stamp == VK_NULL_HANDLE ||
		vk.reactive_stamp_reveal_descriptor == VK_NULL_HANDLE ||
		vk.oit_reveal_image_view == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	/* GENERAL (gen_frag stores) -> COLOR_ATTACHMENT for MAX-blend stamp */
	if ( vk.reactive_mask_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) {
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vk.reactive_mask_layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk.reactive_mask_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		vk.reactive_mask_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	vk_get_active_render_extent( &width, &height );
	vk.renderWidth = width;
	vk.renderHeight = height;
	vk_begin_render_pass_tracked( vk.render_pass.reactive_stamp, vk.framebuffers.reactive_stamp,
		qfalse, width, height );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.reactive_stamp_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_reactive_stamp, 0, 1, &vk.reactive_stamp_reveal_descriptor, 0, NULL );
	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	/* Render pass finalLayout is SHADER_READ_ONLY */
	vk.reactive_mask_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	/* Allow further gen_frag stamps (unlikely after OIT) or leave for TAA sampling */
	vk_barrier_reactive_mask_for_storage( "post-oit-stamp" );
}

void vk_reactive_mask_stamp_weapon_from_depth( void )
{
	uint32_t width = 0;
	uint32_t height = 0;
	VkImageMemoryBarrier barrier;
	VkImageView depthView;
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;
	VkImageAspectFlags depth_aspect;

	if ( !vk_reactive_mask_active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.reactive_stamp == VK_NULL_HANDLE ||
		vk.framebuffers.reactive_stamp == VK_NULL_HANDLE ||
		vk.reactive_stamp_weapon_pipeline == VK_NULL_HANDLE ||
		vk.pipeline_layout_reactive_stamp == VK_NULL_HANDLE ||
		vk.reactive_stamp_reveal_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	depthView = vk.depth_image_view_sample;
	if ( depthView == VK_NULL_HANDLE ) {
		depthView = vk.depth_image_view;
	}
	if ( depthView == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	/* Ensure depth is sampleable. */
	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageView = depthView;
	info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.reactive_stamp_reveal_descriptor;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	if ( vk.reactive_mask_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) {
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vk.reactive_mask_layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk.reactive_mask_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
			VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		vk.reactive_mask_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	vk_get_active_render_extent( &width, &height );
	vk_begin_render_pass_tracked( vk.render_pass.reactive_stamp, vk.framebuffers.reactive_stamp,
		qfalse, width, height );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.reactive_stamp_weapon_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_reactive_stamp, 0, 1, &vk.reactive_stamp_reveal_descriptor, 0, NULL );
	{
		VkViewport viewport;
		VkRect2D scissor;
		Com_Memset( &viewport, 0, sizeof( viewport ) );
		viewport.width = (float)width;
		viewport.height = (float)height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		Com_Memset( &scissor, 0, sizeof( scissor ) );
		scissor.extent.width = width;
		scissor.extent.height = height;
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
		qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
	}
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	vk.reactive_mask_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vk_reactive_mask_update_taa_descriptors();

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );

	/* Restore reveal descriptor if OIT reveal is available for later stamps. */
	if ( vk.oit_reveal_image_view != VK_NULL_HANDLE ) {
		info.imageView = vk.oit_reveal_image_view;
		write.pImageInfo = &info;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
}

void vk_reactive_mask_update_taa_descriptors( void )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;
	VkImageView view;
	uint32_t i;

	view = vk.reactive_mask_view;
	if ( view == VK_NULL_HANDLE ) {
		view = vk.reactive_mask_stub_view;
	}
	if ( view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstBinding = 0;
	desc.descriptorCount = 1;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.taa_reactive_descriptor[i] == VK_NULL_HANDLE ) {
			continue;
		}
		desc.dstSet = vk.taa_reactive_descriptor[i];
		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		vk.taaReactiveBoundView[i] = view;
	}
}

void vk_reactive_mask_update_storage_descriptor( void )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	VkImageView view;
	VkDescriptorSet sets[2];
	uint32_t n = 0;
	uint32_t i;

	view = vk.reactive_mask_view;
	if ( view == VK_NULL_HANDLE ) {
		view = vk.reactive_mask_stub_view;
	}
	if ( view == VK_NULL_HANDLE || vk.set_layout_forward_plus == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	info.sampler = VK_NULL_HANDLE;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = 5;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &info;

	if ( vk_forward_plus_get_graphics_descriptor_set() != VK_NULL_HANDLE ) {
		sets[n++] = vk_forward_plus_get_graphics_descriptor_set();
	}
	if ( vk.forward_plus.descriptor != VK_NULL_HANDLE ) {
		qboolean dup = qfalse;
		for ( i = 0; i < n; i++ ) {
			if ( sets[i] == vk.forward_plus.descriptor ) {
				dup = qtrue;
				break;
			}
		}
		if ( !dup ) {
			sets[n++] = vk.forward_plus.descriptor;
		}
	}
	for ( i = 0; i < n; i++ ) {
		write.dstSet = sets[i];
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
}

void vk_destroy_reactive_mask_pipeline( void )
{
	if ( vk.reactive_stamp_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.reactive_stamp_pipeline, NULL );
		vk.reactive_stamp_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.reactive_stamp_weapon_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.reactive_stamp_weapon_pipeline, NULL );
		vk.reactive_stamp_weapon_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_reactive_stamp != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_reactive_stamp, NULL );
		vk.pipeline_layout_reactive_stamp = VK_NULL_HANDLE;
	}
}

void vk_create_reactive_mask_pipeline( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend;
	VkDynamicState dynamic_states[2];
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkPipelineLayoutCreateInfo layout_desc;
	VkDescriptorSetLayout set_layouts[1];

	vk_destroy_reactive_mask_pipeline();

	if ( vk.render_pass.reactive_stamp == VK_NULL_HANDLE ||
		vk.modules.reactive_stamp_reveal_fs == VK_NULL_HANDLE ||
		vk.modules.gamma_vs == VK_NULL_HANDLE ||
		vk.set_layout_sampler == VK_NULL_HANDLE ) {
		return;
	}

	set_layouts[0] = vk.set_layout_sampler;
	Com_Memset( &layout_desc, 0, sizeof( layout_desc ) );
	layout_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_desc.setLayoutCount = 1;
	layout_desc.pSetLayouts = set_layouts;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &layout_desc, NULL, &vk.pipeline_layout_reactive_stamp ) );
	SET_OBJECT_NAME( vk.pipeline_layout_reactive_stamp, "pipeline layout - reactive stamp",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

	Com_Memset( &shader_stages, 0, sizeof( shader_stages ) );
	vk_set_shader_stage_desc( &shader_stages[0], VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	vk_set_shader_stage_desc( &shader_stages[1], VK_SHADER_STAGE_FRAGMENT_BIT,
		vk.modules.reactive_stamp_reveal_fs, "main" );

	Com_Memset( &vertex_input_state, 0, sizeof( vertex_input_state ) );
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	Com_Memset( &input_assembly_state, 0, sizeof( input_assembly_state ) );
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &rasterization_state, 0, sizeof( rasterization_state ) );
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	Com_Memset( &attachment_blend, 0, sizeof( attachment_blend ) );
	attachment_blend.blendEnable = VK_TRUE;
	attachment_blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.colorBlendOp = VK_BLEND_OP_MAX;
	attachment_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend.alphaBlendOp = VK_BLEND_OP_MAX;
	attachment_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend;

	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_states );
	dynamic_state.pDynamicStates = dynamic_states;

	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = vk.pipeline_layout_reactive_stamp;
	create_info.renderPass = vk.render_pass.reactive_stamp;
	create_info.subpass = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL,
		&vk.reactive_stamp_pipeline ) );
	SET_OBJECT_NAME( vk.reactive_stamp_pipeline, "reactive stamp from reveal",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	if ( vk.modules.reactive_stamp_weapon_fs != VK_NULL_HANDLE ) {
		vk_set_shader_stage_desc( &shader_stages[1], VK_SHADER_STAGE_FRAGMENT_BIT,
			vk.modules.reactive_stamp_weapon_fs, "main" );
		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL,
			&vk.reactive_stamp_weapon_pipeline ) );
		SET_OBJECT_NAME( vk.reactive_stamp_weapon_pipeline, "reactive stamp weapon from depth",
			VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	}

	ri.Printf( PRINT_ALL, "[VK] Temporal reactive mask stamp pipeline ready\n" );
}
