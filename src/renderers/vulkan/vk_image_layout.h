#ifndef VK_IMAGE_LAYOUT_H
#define VK_IMAGE_LAYOUT_H

#include "../common/vulkan/vulkan.h"

void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override );
void record_depth_image_layout_transition( VkCommandBuffer command_buffer, VkImageAspectFlags image_aspect_flags,
	VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override );

#endif
