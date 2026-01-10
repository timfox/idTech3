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
#include "vk_secondary_buffers.h"
#include "vk.h"
#include <string.h>
#include <stdlib.h>

// Renderer interface
extern refimport_t ri;

// Vulkan function pointers (declared in vk.c)
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;
extern PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer qvkEndCommandBuffer;
extern PFN_vkResetCommandBuffer qvkResetCommandBuffer;
extern PFN_vkCmdExecuteCommands qvkCmdExecuteCommands;

// Maximum number of secondary command buffers per frame
#define MAX_SECONDARY_BUFFERS 256

// Draw surface batch for grouping
typedef struct {
    drawSurf_t *surfaces;
    int count;
    unsigned int sort_key; // For grouping by shader/entity
} draw_surf_batch_t;

// Secondary command buffer state
typedef struct {
    qboolean initialized;
    qboolean enabled;
    VkCommandPool command_pool;
    VkCommandBuffer *buffers;
    uint32_t buffer_count;
    uint32_t buffer_capacity;
    
    // Current recording state
    VkCommandBuffer primary_buffer;
    VkRenderPass render_pass;
    uint32_t subpass;
    VkFramebuffer framebuffer;
    qboolean recording;
    
    // Draw surface batches
    draw_surf_batch_t *batches;
    uint32_t batch_count;
    uint32_t batch_capacity;
} secondary_buffer_state_t;

static secondary_buffer_state_t state = {qfalse};

// Initialize secondary command buffer system
void vk_secondary_buffers_init(void) {
    if (state.initialized) {
        return;
    }
    
    memset(&state, 0, sizeof(state));
    state.enabled = qtrue; // Enable by default
    state.buffer_capacity = MAX_SECONDARY_BUFFERS;
    state.batch_capacity = MAX_SECONDARY_BUFFERS;
    
    // Create command pool for secondary buffers
    if (vk.device && vk.device != (VkDevice)0x20000000 && vk.command_pool != VK_NULL_HANDLE) {
        // Use the main command pool for secondary buffers
        // Secondary buffers can share the same pool as primary buffers
        state.command_pool = vk.command_pool;
    }
    
    state.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan: Secondary command buffer system initialized\n");
}

// Shutdown secondary command buffer system
void vk_secondary_buffers_shutdown(void) {
    if (!state.initialized) {
        return;
    }
    
    // Free command buffers
    if (state.buffers && state.buffer_count > 0 && state.command_pool != VK_NULL_HANDLE) {
        if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
            qvkFreeCommandBuffers(vk.device, state.command_pool, state.buffer_count, state.buffers);
        }
        free(state.buffers);
        state.buffers = NULL;
    }
    
    // Free batches
    if (state.batches) {
        free(state.batches);
        state.batches = NULL;
    }
    
    memset(&state, 0, sizeof(state));
    ri.Printf(PRINT_ALL, "Vulkan: Secondary command buffer system shut down\n");
}

// Reset secondary buffers for next frame
// Uses optimized batch reset pattern for better performance
void vk_secondary_buffers_reset(void) {
    if (!state.initialized) {
        return;
    }
    
    // Batch reset all command buffers for better performance
    // Collect valid buffers and reset them in a tight loop for better cache behavior
    if (state.buffers && state.buffer_count > 0 && !vk.device_lost && vk.device != VK_NULL_HANDLE) {
        // Reset all buffers in a batch - individual calls but organized efficiently
        // This pattern is better than scattered resets throughout the code
        qboolean device_lost_during_reset = qfalse;
        for (uint32_t i = 0; i < state.buffer_count; i++) {
            if (state.buffers[i] != VK_NULL_HANDLE) {
                VkResult result = qvkResetCommandBuffer(state.buffers[i], 0);
                if (result == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                    extern void vk_reset_memory_tracking_on_device_lost(void);
                    vk_reset_memory_tracking_on_device_lost();
                    ri.Printf(PRINT_ERROR, "Vulkan: Device lost during secondary buffer reset\n");
                    device_lost_during_reset = qtrue;
                    break; // Stop resetting on device loss
                } else if (result != VK_SUCCESS) {
                    ri.Printf(PRINT_WARNING, "vk_secondary_buffers_reset: Failed to reset buffer %u: %d\n", i, result);
                }
            }
        }
        
        // If device was lost, skip further operations
        if (device_lost_during_reset) {
            return;
        }
    }
    
    state.buffer_count = 0;
    state.batch_count = 0;
    state.recording = qfalse;
}

// Check if secondary buffers are enabled
qboolean vk_secondary_buffers_enabled(void) {
    return state.initialized && state.enabled;
}

// Enable/disable secondary buffers
void vk_secondary_buffers_set_enabled(qboolean enabled) {
    state.enabled = enabled && state.initialized;
}

// Allocate secondary command buffers
static qboolean allocate_secondary_buffers(uint32_t count) {
    if (!state.command_pool || count == 0) {
        return qfalse;
    }
    
    // Expand buffer array if needed
    if (state.buffer_count + count > state.buffer_capacity) {
        uint32_t new_capacity = state.buffer_capacity * 2;
        if (new_capacity < state.buffer_count + count) {
            new_capacity = state.buffer_count + count;
        }
        
        VkCommandBuffer *new_buffers = (VkCommandBuffer*)realloc(state.buffers, 
                                                                  new_capacity * sizeof(VkCommandBuffer));
        if (!new_buffers) {
            ri.Printf(PRINT_ERROR, "vk_secondary_buffers: Failed to reallocate buffer array\n");
            return qfalse;
        }
        
        state.buffers = new_buffers;
        state.buffer_capacity = new_capacity;
    }
    
    // Allocate new buffers
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = state.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = count
    };
    
    VkResult result = qvkAllocateCommandBuffers(vk.device, &allocInfo, 
                                                &state.buffers[state.buffer_count]);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during secondary buffer allocation\n");
        } else {
            ri.Printf(PRINT_ERROR, "vk_secondary_buffers: Failed to allocate buffers: %d\n", result);
        }
        return qfalse;
    }
    
    state.buffer_count += count;
    return qtrue;
}

// Begin recording secondary command buffers
uint32_t vk_secondary_buffers_begin(VkCommandBuffer primary_buffer, 
                                    VkRenderPass render_pass,
                                    uint32_t subpass,
                                    VkFramebuffer framebuffer) {
    if (!state.initialized || !state.enabled || !primary_buffer || !render_pass) {
        return 0;
    }
    
    if (vk.device_lost) {
        return 0;
    }
    
    // Reset for new recording
    vk_secondary_buffers_reset();
    
    state.primary_buffer = primary_buffer;
    state.render_pass = render_pass;
    state.subpass = subpass;
    state.framebuffer = framebuffer;
    state.recording = qtrue;
    
    return state.buffer_capacity;
}

// Group draw surfaces by shader/entity for independent recording
static void group_draw_surfaces(drawSurf_t *drawSurfs, int numDrawSurfs) {
    if (!drawSurfs || numDrawSurfs <= 0) {
        return;
    }
    
    // Allocate batch array if needed
    if (!state.batches) {
        state.batches = (draw_surf_batch_t*)malloc(state.batch_capacity * sizeof(draw_surf_batch_t));
        if (!state.batches) {
            ri.Printf(PRINT_ERROR, "vk_secondary_buffers: Failed to allocate batch array\n");
            return;
        }
    }
    
    state.batch_count = 0;
    
    // Group surfaces by sort key (shader + entity combination)
    // Surfaces with the same sort key can be recorded independently
    unsigned int current_sort = MAX_UINT;
    drawSurf_t *batch_start = NULL;
    int batch_size = 0;
    
    for (int i = 0; i < numDrawSurfs; i++) {
        drawSurf_t *surf = &drawSurfs[i];
        unsigned int sort = surf->sort;
        
        // Check if we need to start a new batch
        // Group by shader/entity (ignore fog and dlight bits for grouping)
        // Use the same mask pattern as in R_DecomposeSort
        unsigned int group_key = sort & ~((0xFF << QSORT_FOGNUM_SHIFT) | 0xFF);
        
        if (group_key != current_sort || state.batch_count >= state.batch_capacity) {
            // Save previous batch
            if (batch_start && batch_size > 0) {
                if (state.batch_count < state.batch_capacity) {
                    state.batches[state.batch_count].surfaces = batch_start;
                    state.batches[state.batch_count].count = batch_size;
                    state.batches[state.batch_count].sort_key = current_sort;
                    state.batch_count++;
                }
            }
            
            // Start new batch
            batch_start = surf;
            batch_size = 1;
            current_sort = group_key;
        } else {
            batch_size++;
        }
    }
    
    // Save final batch
    if (batch_start && batch_size > 0 && state.batch_count < state.batch_capacity) {
        state.batches[state.batch_count].surfaces = batch_start;
        state.batches[state.batch_count].count = batch_size;
        state.batches[state.batch_count].sort_key = current_sort;
        state.batch_count++;
    }
}

// Record a batch of draw surfaces into a secondary command buffer
// This is a simplified version - the actual draw calls would be recorded here
// For now, this is a placeholder that shows the structure
static qboolean record_batch_to_buffer(VkCommandBuffer buffer, draw_surf_batch_t *batch) {
    if (!buffer || !batch || batch->count <= 0) {
        return qfalse;
    }
    
    // Set up inheritance info for secondary command buffer
    VkCommandBufferInheritanceInfo inheritanceInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = NULL,
        .renderPass = state.render_pass,
        .subpass = state.subpass,
        .framebuffer = state.framebuffer,
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = 0,
        .pipelineStatistics = 0
    };
    
    // Begin secondary command buffer
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        .pInheritanceInfo = &inheritanceInfo
    };
    
    VkResult result = qvkBeginCommandBuffer(buffer, &beginInfo);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during secondary buffer begin\n");
        } else {
            ri.Printf(PRINT_ERROR, "vk_secondary_buffers: Failed to begin secondary buffer: %d\n", result);
        }
        return qfalse;
    }
    
    // NOTE: Actual draw call recording would happen here
    // This would call the existing rendering functions but record to the secondary buffer
    // For now, this is a placeholder structure
    
    // End secondary command buffer
    result = qvkEndCommandBuffer(buffer);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
        } else {
            ri.Printf(PRINT_ERROR, "vk_secondary_buffers: Failed to end secondary buffer: %d\n", result);
        }
        return qfalse;
    }
    
    return qtrue;
}

// Record draw surfaces into secondary command buffers
void vk_secondary_buffers_record_draw_surfs(drawSurf_t *drawSurfs, int numDrawSurfs) {
    if (!state.recording || !state.enabled || !drawSurfs || numDrawSurfs <= 0) {
        return;
    }
    
    if (vk.device_lost) {
        return;
    }
    
    // Group draw surfaces
    group_draw_surfaces(drawSurfs, numDrawSurfs);
    
    // Allocate secondary buffers for batches
    if (state.batch_count > 0) {
        if (!allocate_secondary_buffers(state.batch_count)) {
            ri.Printf(PRINT_WARNING, "vk_secondary_buffers: Failed to allocate buffers, falling back to primary\n");
            state.recording = qfalse;
            return;
        }
        
        // Record each batch into a secondary buffer
        uint32_t buffer_index = state.buffer_count - state.batch_count;
        for (uint32_t i = 0; i < state.batch_count; i++) {
            if (buffer_index + i < state.buffer_count) {
                record_batch_to_buffer(state.buffers[buffer_index + i], &state.batches[i]);
            }
        }
    }
}

// End recording and return array of secondary command buffers
VkCommandBuffer* vk_secondary_buffers_end(uint32_t *count) {
    if (!state.recording) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }
    
    state.recording = qfalse;
    
    if (count) {
        *count = state.buffer_count;
    }
    
    return state.buffers;
}
