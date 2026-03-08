/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Post-fog color source selection and descriptor helpers for the Vulkan
FBO pipeline. Centralizes logic for luminance/gamma sampling source
when volumetrics are skipped or SMAA is applied.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_post_fog.h"

/*
===============
vk_post_fog_fbo_debug_throttle
===============
Throttle r_fboDebug 2/3 per-frame logs to at most once per second.
*/
qboolean vk_post_fog_fbo_debug_throttle( void )
{
	static unsigned int last_ms = 0;
	unsigned int now = ri.Milliseconds();
	if ( now - last_ms < 1000 )
		return qfalse;
	last_ms = now;
	return qtrue;
}

const char *vk_post_fog_source_name( VkImageView color_source )
{
	if ( color_source == vk.color_image_view ) {
		return "color_image";
	}
	if ( color_source == vk.smaa_output_image_view ) {
		return "smaa_output";
	}
	if ( color_source == vk.fog_scene_image_view ) {
		return "fog_scene";
	}
	if ( color_source == VK_NULL_HANDLE ) {
		return "null";
	}
	return "unknown_view";
}

static void vk_write_color_descriptor_image( VkDescriptorSet descriptor, VkImageView color_view )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	if ( descriptor == VK_NULL_HANDLE || color_view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	info.sampler = vk_find_sampler( &sd );
	info.imageView = color_view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	desc.pBufferInfo = NULL;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
}

void vk_update_color_descriptor_image( VkImageView color_view )
{
	if ( !vk.cmd ) {
		return;
	}

	vk_write_color_descriptor_image( vk.post_color_descriptor[vk.cmd_index], color_view );
}

VkImage vk_post_fog_source_image( VkImageView color_source )
{
	if ( color_source == vk.color_image_view ) {
		return vk.color_image;
	}
	if ( color_source == vk.smaa_output_image_view ) {
		return vk.smaa_output_image;
	}
	if ( color_source == vk.fog_scene_image_view ) {
		return vk.fog_scene_image;
	}
	return VK_NULL_HANDLE;
}

void vk_barrier_post_fog_source_for_sampling( VkImageView color_source, const char *reason )
{
	VkImage image;
	VkImageMemoryBarrier barrier;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	image = vk_post_fog_source_image( color_source );
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	if ( r_fboDebug && r_fboDebug->integer >= 2 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] post-fog source barrier (%s): %s image=0x%llx view=0x%llx\n",
			reason ? reason : "unspecified",
			vk_post_fog_source_name( color_source ),
			(unsigned long long)(uintptr_t)image,
			(unsigned long long)(uintptr_t)color_source );
	}
}

void vk_log_post_fog_rebind( const char *reason, VkImageView color_source )
{
	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][fbo] %s -> %s view=0x%llx\n",
			reason ? reason : "post-fog source rebind",
			vk_post_fog_source_name( color_source ),
			(unsigned long long)(uintptr_t)color_source );
	}
}

void vk_update_post_fog_descriptors( VkImageView color_source )
{
	VkImageView old_source;
	qboolean updated_luminance = qfalse;

	if ( color_source == VK_NULL_HANDLE ) {
		color_source = vk.color_image_view;
	}

	old_source = vk.post_fog_color_source;
	vk.post_fog_color_source = color_source;
	vk_update_color_descriptor_image( color_source );
	if ( vk.luminance_layout != VK_NULL_HANDLE && vk.luminance_image_view != VK_NULL_HANDLE &&
		color_source != VK_NULL_HANDLE && color_source != vk.luminance_image_view ) {
		VkDescriptorImageInfo lum_info[2];
		VkWriteDescriptorSet lum_writes[2];
		Vk_Sampler_Def sd_linear;
		uint32_t start_idx = vk.cmd ? vk.cmd_index : 0;
		uint32_t end_idx = vk.cmd ? vk.cmd_index + 1 : NUM_COMMAND_BUFFERS;
		uint32_t idx;

		Com_Memset( &sd_linear, 0, sizeof( sd_linear ) );
		sd_linear.gl_mag_filter = sd_linear.gl_min_filter = GL_LINEAR;
		sd_linear.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd_linear.noAnisotropy = qtrue;
		Com_Memset( lum_info, 0, sizeof( lum_info ) );
		lum_info[0].sampler = vk_find_sampler( &sd_linear );
		lum_info[0].imageView = color_source;
		lum_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		lum_info[1].imageView = vk.luminance_image_view;
		lum_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		Com_Memset( lum_writes, 0, sizeof( lum_writes ) );
		lum_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lum_writes[0].dstBinding = 0;
		lum_writes[0].descriptorCount = 1;
		lum_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		lum_writes[0].pImageInfo = &lum_info[0];
		lum_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lum_writes[1].dstBinding = 1;
		lum_writes[1].descriptorCount = 1;
		lum_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		lum_writes[1].pImageInfo = &lum_info[1];
		for ( idx = start_idx; idx < end_idx; idx++ ) {
			lum_writes[0].dstSet = vk.luminance_descriptor[idx];
			lum_writes[1].dstSet = vk.luminance_descriptor[idx];
			qvkUpdateDescriptorSets( vk.device, 2, lum_writes, 0, NULL );
		}
		updated_luminance = qtrue;
	}

	if ( r_fboDebug && r_fboDebug->integer >= 1 && vk_post_fog_fbo_debug_throttle() ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][fbo] post_fog_color_source: %s -> %s view=0x%llx luminance=%s\n",
			vk_post_fog_source_name( old_source ),
			vk_post_fog_source_name( color_source ),
			(unsigned long long)(uintptr_t)color_source,
			updated_luminance ? "updated" : "unchanged" );
	}
}

/*
 * Centralized post-fog source selection for luminance/gamma passes.
 */
VkImageView vk_get_post_fog_source( void )
{
	if ( vk.post_fog_color_source != VK_NULL_HANDLE ) {
		return vk.post_fog_color_source;
	}
	return vk.color_image_view;
}
