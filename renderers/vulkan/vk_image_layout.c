#include "tr_local.h"
#include "vk.h"
#include "vk_image_layout.h"
#include "vk_pass_registry.h"

void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override )
{
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;
	VkAccessFlags supported_src_access = 0;
	VkAccessFlags supported_dst_access = 0;

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported new layout %i", new_layout );
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}

	if ( src_stage_override != 0 ) {
		src_stage = src_stage_override;
	}
	if ( dst_stage_override != 0 ) {
		dst_stage = dst_stage_override;
		if ( dst_stage & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT ) {
			barrier.dstAccessMask &= ~VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		}
	}

	if ( old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
		if ( src_stage & ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ) ) {
			barrier.srcAccessMask &= ~VK_ACCESS_SHADER_READ_BIT;
			barrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
	}
	if ( new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
		if ( dst_stage & ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ) ) {
			barrier.dstAccessMask &= ~VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
	}

	if ( ( dst_stage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT ) == 0 &&
	     ( barrier.dstAccessMask & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ) ) {
		barrier.dstAccessMask &= ~VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}

	if ( src_stage & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT ) {
		supported_src_access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	}
	if ( src_stage & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT ) {
		supported_src_access |= VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if ( src_stage & ( VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
		VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
		VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
		VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT ) ) {
		supported_src_access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	if ( src_stage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT ) {
		supported_src_access |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}
	if ( src_stage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ) {
		supported_src_access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}
	if ( src_stage & ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ) ) {
		supported_src_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	if ( src_stage & VK_PIPELINE_STAGE_TRANSFER_BIT ) {
		supported_src_access |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if ( src_stage & VK_PIPELINE_STAGE_HOST_BIT ) {
		supported_src_access |= VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
	}

	if ( dst_stage & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT ) {
		supported_dst_access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	}
	if ( dst_stage & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT ) {
		supported_dst_access |= VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if ( dst_stage & ( VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
		VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
		VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
		VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT ) ) {
		supported_dst_access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	if ( dst_stage & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT ) {
		supported_dst_access |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}
	if ( dst_stage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ) {
		supported_dst_access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}
	if ( dst_stage & ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ) ) {
		supported_dst_access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	if ( dst_stage & VK_PIPELINE_STAGE_TRANSFER_BIT ) {
		supported_dst_access |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if ( dst_stage & VK_PIPELINE_STAGE_HOST_BIT ) {
		supported_dst_access |= VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
	}

	if ( src_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT ) {
		barrier.srcAccessMask = VK_ACCESS_NONE;
	} else {
		barrier.srcAccessMask &= supported_src_access;
		if ( old_layout == VK_IMAGE_LAYOUT_GENERAL &&
			( src_stage & VK_PIPELINE_STAGE_TRANSFER_BIT ) != 0 &&
			barrier.srcAccessMask == 0 ) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
	}

	barrier.dstAccessMask &= supported_dst_access;
	if ( new_layout == VK_IMAGE_LAYOUT_GENERAL &&
		( dst_stage & VK_PIPELINE_STAGE_TRANSFER_BIT ) != 0 &&
		barrier.dstAccessMask == 0 ) {
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
	/* All renderer-owned images pass through this helper.  Keep the Spine resource
	 * owner synchronized here instead of relying on scattered note calls. */
	vk_spine_transition_image( image, old_layout, new_layout,
		vk_spine_current_pass(), "image_transition" );
}

void record_depth_image_layout_transition( VkCommandBuffer command_buffer, VkImageAspectFlags image_aspect_flags,
	VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override )
{
	if ( vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.depth_image_layout == new_layout ) {
		return;
	}

	record_image_layout_transition( command_buffer, vk.depth_image, image_aspect_flags,
		vk.depth_image_layout, new_layout, src_stage_override, dst_stage_override );
	vk.depth_image_layout = new_layout;
	vk_spine_note_layout( VK_SPINE_RES_DEPTH, new_layout );
	vk_spine_note_barrier( VK_SPINE_RES_DEPTH, VK_SPINE_PASS_NONE, "depth_layout" );
}
