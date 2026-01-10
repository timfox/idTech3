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

#include "tr_local.h"
#include "vk_resource_state.h"
#include "vk.h"
#include <string.h>

// Renderer interface
extern refimport_t ri;

// Vulkan function pointers
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;

// Simple hash map for image state tracking
// Using a fixed-size hash table for efficiency
#define RESOURCE_STATE_HASH_SIZE 1024
#define RESOURCE_STATE_HASH_MASK (RESOURCE_STATE_HASH_SIZE - 1)

typedef struct image_state_entry_s {
    VkImage image;
    VkImageLayout layout;
    struct image_state_entry_s *next; // For hash collision chaining
} image_state_entry_t;

typedef struct {
    qboolean initialized;
    image_state_entry_t *hash_table[RESOURCE_STATE_HASH_SIZE];
    image_state_entry_t *free_entries;
    image_state_entry_t entry_pool[RESOURCE_STATE_HASH_SIZE * 2]; // Pool for entries
    uint32_t entry_count;
} resource_state_tracker_t;

static resource_state_tracker_t state_tracker = {qfalse};

// Simple hash function for VkImage handles
static uint32_t hash_image_handle(VkImage image) {
    // Use pointer value as hash (VkImage is a pointer type)
    uintptr_t ptr = (uintptr_t)image;
    return (uint32_t)(ptr ^ (ptr >> 16)) & RESOURCE_STATE_HASH_MASK;
}

// Find entry for image
static image_state_entry_t* find_entry(VkImage image) {
    if (!state_tracker.initialized) {
        return NULL;
    }
    
    uint32_t hash = hash_image_handle(image);
    image_state_entry_t *entry = state_tracker.hash_table[hash];
    
    while (entry) {
        if (entry->image == image) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

// Allocate new entry
static image_state_entry_t* allocate_entry(void) {
    if (state_tracker.entry_count >= (RESOURCE_STATE_HASH_SIZE * 2)) {
        ri.Printf(PRINT_WARNING, "vk_resource_state: Entry pool exhausted\n");
        return NULL;
    }
    
    return &state_tracker.entry_pool[state_tracker.entry_count++];
}

// Insert or update entry
static image_state_entry_t* insert_entry(VkImage image, VkImageLayout layout) {
    if (!state_tracker.initialized) {
        return NULL;
    }
    
    uint32_t hash = hash_image_handle(image);
    image_state_entry_t *entry = find_entry(image);
    
    if (entry) {
        // Update existing entry
        entry->layout = layout;
        return entry;
    }
    
    // Allocate new entry
    entry = allocate_entry();
    if (!entry) {
        return NULL;
    }
    
    entry->image = image;
    entry->layout = layout;
    entry->next = state_tracker.hash_table[hash];
    state_tracker.hash_table[hash] = entry;
    
    return entry;
}

// Initialize resource state tracker
void vk_resource_state_init(void) {
    if (state_tracker.initialized) {
        return;
    }
    
    memset(&state_tracker, 0, sizeof(state_tracker));
    state_tracker.initialized = qtrue;
    state_tracker.entry_count = 0;
    
    ri.Printf(PRINT_ALL, "Vulkan: Resource state tracker initialized\n");
}

// Shutdown resource state tracker
void vk_resource_state_shutdown(void) {
    if (!state_tracker.initialized) {
        return;
    }
    
    memset(&state_tracker, 0, sizeof(state_tracker));
    ri.Printf(PRINT_ALL, "Vulkan: Resource state tracker shut down\n");
}

// Reset state tracker for new frame (optional)
void vk_resource_state_reset_frame(void) {
    // For now, we keep state across frames
    // This can be changed if per-frame reset is needed
    // memset(state_tracker.hash_table, 0, sizeof(state_tracker.hash_table));
    // state_tracker.entry_count = 0;
}

// Get current image layout
VkImageLayout vk_resource_state_get_image_layout(VkImage image) {
    image_state_entry_t *entry = find_entry(image);
    if (entry) {
        return entry->layout;
    }
    return VK_IMAGE_LAYOUT_UNDEFINED; // Not tracked
}

// Set image layout
void vk_resource_state_set_image_layout(VkImage image, VkImageLayout layout) {
    if (image == VK_NULL_HANDLE) {
        return;
    }
    insert_entry(image, layout);
}

// Determine pipeline stages and access masks for layout transition
static void get_layout_transition_info(
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_override,
    VkPipelineStageFlags dst_stage_override,
    VkPipelineStageFlags *src_stage,
    VkPipelineStageFlags *dst_stage,
    VkAccessFlags *src_access,
    VkAccessFlags *dst_access
) {
    // Source stage and access
    switch (old_layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            *src_stage = (src_stage_override != 0) ? src_stage_override : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            *src_access = 0;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            *src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            *src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            *src_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            *src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *src_access = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            *src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *src_access = VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            *src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            *src_access = 0;
            break;
        case VK_IMAGE_LAYOUT_GENERAL:
            *src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *src_access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;
        default:
            *src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *src_access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;
    }
    
    // Destination stage and access
    switch (new_layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            *dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            *dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dst_access = 0;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dst_access = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_TRANSFER_BIT;
            *dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            *dst_access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_GENERAL:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *dst_access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;
        default:
            *dst_stage = (dst_stage_override != 0) ? dst_stage_override : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            *dst_access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;
    }
}

// Transition image layout with automatic barrier insertion
qboolean vk_resource_state_transition_image(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageAspectFlags image_aspect_flags,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_override,
    VkPipelineStageFlags dst_stage_override
) {
    if (image == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE) {
        return qfalse;
    }
    
    // Get current layout from tracker
    VkImageLayout old_layout = vk_resource_state_get_image_layout(image);
    
    // If not tracked, assume UNDEFINED (first use)
    // The get function already returns UNDEFINED if not tracked, so we're good
    
    // Skip barrier if layout hasn't changed
    if (old_layout == new_layout) {
        return qfalse; // No transition needed
    }
    
    // Get transition information
    VkPipelineStageFlags src_stage, dst_stage;
    VkAccessFlags src_access, dst_access;
    get_layout_transition_info(old_layout, new_layout, src_stage_override, dst_stage_override,
                              &src_stage, &dst_stage, &src_access, &dst_access);
    
    // Create barrier
    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = image_aspect_flags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        }
    };
    
    // Insert barrier
    qvkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    
    // Update tracked state
    vk_resource_state_set_image_layout(image, new_layout);
    
    return qtrue; // Barrier was inserted
}

// Transition image layout with explicit old layout
void vk_resource_state_transition_image_explicit(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageAspectFlags image_aspect_flags,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage_override,
    VkPipelineStageFlags dst_stage_override
) {
    if (image == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE) {
        return;
    }
    
    // Skip barrier if layout hasn't changed
    if (old_layout == new_layout) {
        // Still update tracker
        vk_resource_state_set_image_layout(image, new_layout);
        return;
    }
    
    // Get transition information
    VkPipelineStageFlags src_stage, dst_stage;
    VkAccessFlags src_access, dst_access;
    get_layout_transition_info(old_layout, new_layout, src_stage_override, dst_stage_override,
                              &src_stage, &dst_stage, &src_access, &dst_access);
    
    // Create barrier
    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = image_aspect_flags,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        }
    };
    
    // Insert barrier
    qvkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    
    // Update tracked state
    vk_resource_state_set_image_layout(image, new_layout);
}

// Remove image from tracking
void vk_resource_state_remove_image(VkImage image) {
    if (!state_tracker.initialized || image == VK_NULL_HANDLE) {
        return;
    }
    
    uint32_t hash = hash_image_handle(image);
    image_state_entry_t **entry_ptr = &state_tracker.hash_table[hash];
    
    while (*entry_ptr) {
        if ((*entry_ptr)->image == image) {
            // Remove from chain
            image_state_entry_t *to_remove = *entry_ptr;
            *entry_ptr = to_remove->next;
            // Entry remains in pool but is no longer in hash table
            return;
        }
        entry_ptr = &(*entry_ptr)->next;
    }
}
