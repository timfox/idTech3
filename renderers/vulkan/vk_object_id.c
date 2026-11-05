/*
===========================================================================
Dynamic-object identity buffer: clear, storage descriptor, sampling barrier,
TAA descriptors and ping-pong commit. See vk_object_id.h for the rationale.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_object_id.h"
#include "vk_image_layout.h"
#include "vk_util.h"
#include "vk_upscale.h"
#include "vk_forward_plus.h"

static int s_object_id_cleared_frame = -1;

static cvar_t *vk_object_id_cvar( void )
{
	static cvar_t *c;
	if ( !c ) {
		c = ri.Cvar_Get( "r_temporalObjectId", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	}
	return c;
}

qboolean vk_object_id_wanted( void )
{
	cvar_t *c = vk_object_id_cvar();

	if ( !vk.fboActive || !c || !c->integer ) {
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

qboolean vk_object_id_active( void )
{
	return vk.object_id_image[0] != VK_NULL_HANDLE &&
		vk.object_id_image[1] != VK_NULL_HANDLE &&
		vk.object_id_view[0] != VK_NULL_HANDLE &&
		vk.object_id_view[1] != VK_NULL_HANDLE;
}

static uint32_t vk_object_id_curr_slot( void )
{
	return vk.temporal.objectIdIndex & 1u;
}

void vk_object_id_begin_frame( void )
{
	VkClearColorValue clear_value;
	VkImageSubresourceRange range;
	VkImageMemoryBarrier barrier;
	uint32_t curr;

	if ( !vk_object_id_active() || !vk_object_id_wanted() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( s_object_id_cleared_frame == tr.frameCount ) {
		return;
	}
	s_object_id_cleared_frame = tr.frameCount;

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	curr = vk_object_id_curr_slot();

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vk.object_id_layout[curr];
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.object_id_image[curr];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	Com_Memset( &clear_value, 0, sizeof( clear_value ) ); /* background id = 0 */
	Com_Memset( &range, 0, sizeof( range ) );
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = 1;
	range.layerCount = 1;
	qvkCmdClearColorImage( vk.cmd->command_buffer, vk.object_id_image[curr],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range );

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
	vk.object_id_layout[curr] = VK_IMAGE_LAYOUT_GENERAL;

	vk_object_id_update_storage_descriptor();
}

void vk_object_id_update_storage_descriptor( void )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	VkImageView view;
	VkDescriptorSet sets[2];
	uint32_t n = 0;
	uint32_t i;

	if ( vk_object_id_active() ) {
		view = vk.object_id_view[vk_object_id_curr_slot()];
	} else {
		view = vk.object_id_stub_view;
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
	write.dstBinding = 8;
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

void vk_barrier_object_id_for_sampling( const char *reason )
{
	VkImageMemoryBarrier barrier;
	uint32_t i;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk_object_id_active() ) {
		return;
	}

	for ( i = 0; i < 2; i++ ) {
		if ( vk.object_id_layout[i] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
			continue;
		}
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vk.object_id_layout[i];
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk.object_id_image[i];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		vk.object_id_layout[i] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	(void)reason;
}

void vk_object_id_update_taa_descriptors( void )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;
	int i;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;

	for ( i = 0; i < 2; i++ ) {
		if ( vk.taa_object_id_descriptor[i] == VK_NULL_HANDLE ) {
			continue;
		}
		info.imageView = vk_object_id_active() ? vk.object_id_view[i] : vk.object_id_stub_view;
		if ( info.imageView == VK_NULL_HANDLE ) {
			continue;
		}
		write.dstSet = vk.taa_object_id_descriptor[i];
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
	if ( vk.taa_object_id_stub_descriptor != VK_NULL_HANDLE && vk.object_id_stub_view != VK_NULL_HANDLE ) {
		info.imageView = vk.object_id_stub_view;
		info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		write.dstSet = vk.taa_object_id_stub_descriptor;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
}

VkDescriptorSet vk_object_id_curr_descriptor( void )
{
	uint32_t curr = vk_object_id_curr_slot();
	if ( !vk_object_id_active() || vk.taa_object_id_descriptor[curr] == VK_NULL_HANDLE ) {
		return vk.taa_object_id_stub_descriptor;
	}
	return vk.taa_object_id_descriptor[curr];
}

VkDescriptorSet vk_object_id_prev_descriptor( void )
{
	uint32_t prev = 1u - vk_object_id_curr_slot();
	if ( !vk_object_id_active() || !vk.temporal.objectIdHasPrev ||
		vk.taa_object_id_descriptor[prev] == VK_NULL_HANDLE ) {
		return vk.taa_object_id_stub_descriptor;
	}
	return vk.taa_object_id_descriptor[prev];
}

void vk_object_id_commit( void )
{
	uint32_t curr;

	if ( !vk_object_id_active() ) {
		return;
	}
	curr = vk_object_id_curr_slot();
	vk.temporal.objectIdFrameId[curr] = vk.temporal.frameIndex;
	vk.temporal.objectIdHasPrev = qtrue;
	/* Flip: next frame stamps the other slot, this frame becomes prev. */
	vk.temporal.objectIdIndex = 1u - curr;
}

void vk_object_id_reset( void )
{
	vk.temporal.objectIdIndex = 0u;
	vk.temporal.objectIdHasPrev = qfalse;
	vk.temporal.objectIdFrameId[0] = 0;
	vk.temporal.objectIdFrameId[1] = 0;
	s_object_id_cleared_frame = -1;
}
