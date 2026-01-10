/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef VK_RESOURCE_STATE_H
#define VK_RESOURCE_STATE_H

#include <vulkan/vulkan.h>
#include "../common/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Resource state tracker for images
// Tracks current layout state to avoid redundant barriers

// Initialize resource state tracker
void vk_resource_state_init(void);

// Shutdown resource state tracker
void vk_resource_state_shutdown(void);

// Reset state tracker for new frame (optional - can be called per frame)
void vk_resource_state_reset_frame(void);

// Get current image layout (returns VK_IMAGE_LAYOUT_UNDEFINED if not tracked)
VkImageLayout vk_resource_state_get_image_layout(VkImage image);

// Set image layout (for initialization or explicit state setting)
void vk_resource_state_set_image_layout(VkImage image, VkImageLayout layout);

// Transition image layout with automatic barrier insertion
// Only inserts barrier if layout actually changes
// Returns qtrue if barrier was inserted, qfalse if no transition needed
qboolean vk_resource_state_transition_image(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageAspectFlags image_aspect_flags,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_override,
    VkPipelineStageFlags dst_stage_override
);

// Transition image layout with explicit old layout (for cases where state is unknown)
// Always inserts barrier
void vk_resource_state_transition_image_explicit(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageAspectFlags image_aspect_flags,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_override,
    VkPipelineStageFlags dst_stage_override
);

// Remove image from tracking (when image is destroyed)
void vk_resource_state_remove_image(VkImage image);

#ifdef __cplusplus
}
#endif

#endif // VK_RESOURCE_STATE_H
