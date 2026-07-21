/*
===========================================================================
Temporal class R8 ping-pong: clear WORLD, stamp WEAPON from depth, feed TAA.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_temporal_class.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_pipeline_helpers.h"
#include "vk_util.h"
#include "vk_upscale.h"
#include "vk_post_fog.h"
#include "vk_aa_policy.h"
#include "vk_scene_pass.h"
#include "vk_view_state.h"

static int s_class_cleared_frame = -1;
static int s_class_committed_frame = -1;

qboolean vk_temporal_class_wanted( void )
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
	return qfalse;
}

qboolean vk_temporal_class_active( void )
{
	return vk.temporal_class_image[0] != VK_NULL_HANDLE &&
		vk.temporal_class_image[1] != VK_NULL_HANDLE &&
		vk.temporal_class_view[0] != VK_NULL_HANDLE &&
		vk.temporal_class_view[1] != VK_NULL_HANDLE;
}

VkImageView vk_temporal_class_prev_view( void )
{
	uint32_t prev;

	if ( !vk_temporal_class_active() ) {
		return VK_NULL_HANDLE;
	}
	prev = vk.temporal.classHistoryIndex & 1u;
	return vk.temporal_class_view[prev];
}

VkImageView vk_temporal_class_current_view( void )
{
	if ( !vk_temporal_class_active() ) {
		return VK_NULL_HANDLE;
	}
	return vk.temporal_class_view[vk.temporal.classHistoryIndex & 1u];
}

static uint32_t vk_temporal_class_curr_index( void )
{
	return 1u - ( vk.temporal.classHistoryIndex & 1u );
}

void vk_barrier_temporal_class_for_sampling( const char *reason )
{
	VkImageMemoryBarrier barrier;
	uint32_t idx;
	uint32_t i;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk_temporal_class_active() ) {
		return;
	}

	for ( i = 0; i < 2; i++ ) {
		idx = (uint32_t)i;
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vk.temporal_class_layout[idx];
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk.temporal_class_image[idx];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
			VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT |
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		vk.temporal_class_layout[idx] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	(void)reason;
}

void vk_temporal_class_clear( void )
{
	VkClearColorValue clear_value;
	VkImageSubresourceRange range;
	VkImageMemoryBarrier barrier;
	uint32_t curr;

	if ( !vk_temporal_class_active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( s_class_cleared_frame == tr.frameCount ) {
		return;
	}
	s_class_cleared_frame = tr.frameCount;

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	curr = vk_temporal_class_curr_index();

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vk.temporal_class_layout[curr];
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.temporal_class_image[curr];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	Com_Memset( &clear_value, 0, sizeof( clear_value ) ); /* WORLD = 0 */
	Com_Memset( &range, 0, sizeof( range ) );
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = 1;
	range.layerCount = 1;
	qvkCmdClearColorImage( vk.cmd->command_buffer, vk.temporal_class_image[curr],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range );

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
	vk.temporal_class_layout[curr] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void vk_temporal_class_stamp_weapon_from_depth( void )
{
	VkImageMemoryBarrier barrier;
	VkImageView depthView;
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	uint32_t curr;
	uint32_t width = 0;
	uint32_t height = 0;

	if ( !vk_temporal_class_active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.temporal_class_stamp == VK_NULL_HANDLE ||
		vk.temporal_class_stamp_pipeline == VK_NULL_HANDLE ||
		vk.pipeline_layout_temporal_class_stamp == VK_NULL_HANDLE ||
		vk.temporal_class_stamp_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	if ( !r_weaponTemporalMode ) {
		/* Registered in vk_aa_policy_register_cvars; lazy fallback for early frames. */
		r_weaponTemporalMode = ri.Cvar_Get( "r_weaponTemporalMode", "1", CVAR_ARCHIVE_ND );
	}
	/* Mode 2 reserved; modes 0 and 1 both stamp so next-frame reject works. */

	vk_temporal_class_clear();

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	curr = vk_temporal_class_curr_index();
	vk_get_active_render_extent( &width, &height );
	if ( width == 0 || height == 0 ) {
		width = vk_get_render_target_width();
		height = vk_get_render_target_height();
	}

	depthView = vk.depth_image_view_sample;
	if ( depthView == VK_NULL_HANDLE ) {
		depthView = vk.depth_image_view;
	}
	if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
		depthView = vk.volumetric_depth_view;
	}
	if ( depthView == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.imageView = depthView;
	info.imageLayout = ( vk.msaaActive && depthView == vk.volumetric_depth_view ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	{
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;
		info.sampler = vk_find_sampler( &sd );
	}

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.temporal_class_stamp_descriptor;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	/* Depth must be readable. */
	{
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	}

	if ( vk.temporal_class_layout[curr] != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) {
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vk.temporal_class_layout[curr];
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk.temporal_class_image[curr];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		vk.temporal_class_layout[curr] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	/* Rebuild FB attachment to current ping-pong view if needed. */
	if ( vk.framebuffers.temporal_class_stamp[curr] == VK_NULL_HANDLE ) {
		return;
	}

	vk_begin_render_pass_tracked( vk.render_pass.temporal_class_stamp,
		vk.framebuffers.temporal_class_stamp[curr],
		width, height, qfalse );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.temporal_class_stamp_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_temporal_class_stamp, 0, 1, &vk.temporal_class_stamp_descriptor, 0, NULL );
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

	vk.temporal_class_layout[curr] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	/* Promote stamped buffer to previous for next TAA. */
	vk.temporal.classHistoryIndex = curr;
	vk.temporal.classHasPrev = qtrue;
	vk.temporal.prevClassValid = qtrue;
	vk.temporal.classFrameId[curr] = vk.temporal.frameIndex;
	s_class_committed_frame = tr.frameCount;
	vk_temporal_class_update_current_weapon_descriptors();

	/* Restore depth for subsequent attachment use. */
	{
		VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}
}

void vk_temporal_class_commit_world_only( void )
{
	uint32_t curr;
	VkImageMemoryBarrier barrier;

	if ( !vk_temporal_class_active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( s_class_committed_frame == tr.frameCount ) {
		return;
	}

	/* Force a WORLD clear even if stamp already cleared earlier this frame. */
	s_class_cleared_frame = -1;
	vk_temporal_class_clear();

	curr = vk_temporal_class_curr_index();
	if ( vk.temporal_class_layout[curr] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vk.temporal_class_layout[curr];
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk.temporal_class_image[curr];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		vk.temporal_class_layout[curr] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	vk.temporal.classHistoryIndex = curr;
	vk.temporal.classHasPrev = qtrue;
	vk.temporal.prevClassValid = qtrue;
	vk.temporal.classFrameId[curr] = vk.temporal.frameIndex;
	s_class_committed_frame = tr.frameCount;
}

void vk_temporal_class_update_taa_descriptors( void )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	VkImageView view;
	int i;

	if ( !vk_temporal_class_active() ) {
		return;
	}

	view = vk_temporal_class_prev_view();
	if ( view == VK_NULL_HANDLE || !vk.temporal.classHasPrev ) {
		/* First frame: bind current cleared WORLD. */
		view = vk.temporal_class_view[vk_temporal_class_curr_index()];
	}
	if ( view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	{
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;
		info.sampler = vk_find_sampler( &sd );
	}

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.taa_class_descriptor[i] == VK_NULL_HANDLE ) {
			continue;
		}
		write.dstSet = vk.taa_class_descriptor[i];
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
		vk.taaClassBoundView[i] = view;
	}
}

void vk_temporal_class_update_current_weapon_descriptors( void )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	VkImageView view = vk_temporal_class_current_view();
	int i;

	if ( view == VK_NULL_HANDLE ) {
		return;
	}
	Com_Memset( &info, 0, sizeof( info ) );
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	{
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;
		info.sampler = vk_find_sampler( &sd );
	}
	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	i = vk.cmd_index;
	if ( i < NUM_COMMAND_BUFFERS && vk.weapon_current_class_descriptor[i] != VK_NULL_HANDLE ) {
		write.dstSet = vk.weapon_current_class_descriptor[i];
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
}

void vk_destroy_temporal_class_pipeline( void )
{
	if ( vk.temporal_class_stamp_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.temporal_class_stamp_pipeline, NULL );
		vk.temporal_class_stamp_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_temporal_class_stamp != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_temporal_class_stamp, NULL );
		vk.pipeline_layout_temporal_class_stamp = VK_NULL_HANDLE;
	}
}

void vk_create_temporal_class_pipeline( void )
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

	vk_destroy_temporal_class_pipeline();

	if ( vk.render_pass.temporal_class_stamp == VK_NULL_HANDLE ||
		vk.modules.temporal_class_stamp_fs == VK_NULL_HANDLE ||
		vk.modules.gamma_vs == VK_NULL_HANDLE ||
		vk.set_layout_sampler == VK_NULL_HANDLE ) {
		return;
	}

	set_layouts[0] = vk.set_layout_sampler;
	Com_Memset( &layout_desc, 0, sizeof( layout_desc ) );
	layout_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_desc.setLayoutCount = 1;
	layout_desc.pSetLayouts = set_layouts;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &layout_desc, NULL,
		&vk.pipeline_layout_temporal_class_stamp ) );
	SET_OBJECT_NAME( vk.pipeline_layout_temporal_class_stamp, "pipeline layout - temporal class stamp",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

	Com_Memset( &shader_stages, 0, sizeof( shader_stages ) );
	vk_set_shader_stage_desc( &shader_stages[0], VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	vk_set_shader_stage_desc( &shader_stages[1], VK_SHADER_STAGE_FRAGMENT_BIT,
		vk.modules.temporal_class_stamp_fs, "main" );

	Com_Memset( &vertex_input_state, 0, sizeof( vertex_input_state ) );
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	Com_Memset( &input_assembly_state, 0, sizeof( input_assembly_state ) );
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &rasterization_state, 0, sizeof( rasterization_state ) );
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
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
	create_info.layout = vk.pipeline_layout_temporal_class_stamp;
	create_info.renderPass = vk.render_pass.temporal_class_stamp;
	create_info.subpass = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL,
		&vk.temporal_class_stamp_pipeline ) );
	SET_OBJECT_NAME( vk.temporal_class_stamp_pipeline, "temporal class stamp from depth",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}
