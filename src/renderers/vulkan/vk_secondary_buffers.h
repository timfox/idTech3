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

#ifndef VK_SECONDARY_BUFFERS_H
#define VK_SECONDARY_BUFFERS_H

#include <vulkan/vulkan.h>
#include "../common/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct drawSurf_s;
typedef struct drawSurf_s drawSurf_t;

// Secondary command buffer system for parallel command recording
// Groups independent draw calls into secondary command buffers

// Initialize secondary command buffer system
void vk_secondary_buffers_init(void);

// Shutdown secondary command buffer system
void vk_secondary_buffers_shutdown(void);

// Begin recording secondary command buffers for a render pass
// Returns the number of secondary buffers allocated
uint32_t vk_secondary_buffers_begin(VkCommandBuffer primary_buffer, 
                                    VkRenderPass render_pass,
                                    uint32_t subpass,
                                    VkFramebuffer framebuffer);

// Record draw surfaces into secondary command buffers
// Groups surfaces by shader/entity for independent recording
void vk_secondary_buffers_record_draw_surfs(drawSurf_t *drawSurfs, int numDrawSurfs);

// End recording and return array of secondary command buffers
// Caller should execute these buffers using vkCmdExecuteCommands
VkCommandBuffer* vk_secondary_buffers_end(uint32_t *count);

// Reset secondary buffers for next frame
void vk_secondary_buffers_reset(void);

// Check if secondary buffers are enabled
qboolean vk_secondary_buffers_enabled(void);

// Enable/disable secondary buffers (for testing/fallback)
void vk_secondary_buffers_set_enabled(qboolean enabled);

#ifdef __cplusplus
}
#endif

#endif // VK_SECONDARY_BUFFERS_H
