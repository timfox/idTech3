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

// Vulkan function pointers
extern PFN_vkCmdSetViewport qvkCmdSetViewport;
extern PFN_vkCmdSetScissor qvkCmdSetScissor;

// Scene Rendering Functions for Vulkan Renderer
// Handles 3D scene rendering, entities, polygons, and world geometry

void vk_add_entity(const refEntity_t *re, int intShaderTime) {
    // Add entity to scene for rendering
    if (!vk.active) {
        return;
    }

    if (vk.scene.entityCount >= MAX_REFENTITIES) {
        ri.Printf(PRINT_WARNING, "vk_add_entity: Too many entities, dropping\n");
        return;
    }

    // Copy entity data
    vk.scene.entities[vk.scene.entityCount] = *re;

    // Handle intShaderTime for shader animation
    // Store shader time for potential animation calculations
    // Note: shaderTime type needs to be verified - for now, we'll skip this
    // vk.scene.entities[vk.scene.entityCount].shaderTime = intShaderTime;
    Q_UNUSED(intShaderTime);

    vk.scene.entityCount++;
}

void vk_add_polygon(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num) {
    // Add polygon to scene
    Q_UNUSED(num); // num parameter seems unused in original code

    if (vk.scene.polygonCount >= MAX_INDICES / 3) {
        ri.Printf(PRINT_WARNING, "vk_add_polygon: Too many polygons, dropping\n");
        return;
    }

    // Store polygon data for rendering
    // Copy vertex data to scene storage
    int baseIndex = vk.scene.polygonCount * 3;
    for (int i = 0; i < numVerts && baseIndex + i < MAX_VERTS; i++) {
        vk.scene.polygonVerts[baseIndex + i] = verts[i];
    }

    // Store indices for this polygon (assuming triangles)
    int indexBase = vk.scene.polygonCount * 3;
    for (int i = 0; i < numVerts && indexBase + i < MAX_INDICES; i++) {
        vk.scene.polygonIndexes[indexBase + i] = baseIndex + i;
    }

    // Store shader handle for this polygon
    // Note: We'll need to extend the scene structure to store per-polygon shaders
    Q_UNUSED(hShader); // For now, not storing per-polygon shader

    vk.scene.polygonCount++;
}

void vk_clear_scene(void) {
    // Clear the scene for new frame
    vk.scene.entityCount = 0;
    vk.scene.polygonCount = 0;
    memset(vk.scene.entities, 0, sizeof(vk.scene.entities));
    memset(vk.scene.polygonVerts, 0, sizeof(vk.scene.polygonVerts));
    memset(vk.scene.polygonIndexes, 0, sizeof(vk.scene.polygonIndexes));

    ri.Printf(PRINT_DEVELOPER, "vk_clear_scene: Scene cleared\n");
}

void vk_render_scene_vulkan(const refdef_t *fd) {
    if (vk.device == (VkDevice)0x20000000) {
        return; // Skip for fake devices
    }

    // Save current command buffer context
    VkCommandBuffer saved_command_buffer = VK_NULL_HANDLE;
    if (vk.cmd && vk.cmd->command_buffer != VK_NULL_HANDLE) {
        saved_command_buffer = vk.cmd->command_buffer;
    }

    // Begin command buffer
    VkCommandBuffer command_buffer = begin_command_buffer();
    
    // Temporarily set command buffer context for helper functions
    if (vk.cmd) {
        vk.cmd->command_buffer = command_buffer;
    }

    // Begin render pass
    VkClearValue clear_values[2];
    clear_values[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.2f, 1.0f}}; // Blue clear color
    clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};

    VkRenderPassBeginInfo render_pass_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = vk.render_pass.main,
        .framebuffer = vk.framebuffers.main[vk.current_swapchain_image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = {(uint32_t)glConfig.vidWidth, (uint32_t)glConfig.vidHeight}
        },
        .clearValueCount = 2,
        .pClearValues = clear_values
    };

    qvkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

    // Set up viewport and scissor for 3D rendering
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)glConfig.vidWidth,
        .height = (float)glConfig.vidHeight,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    qvkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {(uint32_t)glConfig.vidWidth, (uint32_t)glConfig.vidHeight}
    };
    qvkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Render polygons (if any) using tessellation system
    if (vk.scene.polygonCount > 0) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Rendering %d polygons\n", vk.scene.polygonCount);

        // Use tessellation system to render polygons
        // Convert stored polygons to tessellation buffer format for rendering
        extern shaderCommands_t tess;
        
        tess.numVertexes = 0;
        tess.numIndexes = 0;
        
        for (int i = 0; i < vk.scene.polygonCount; i++) {
            int baseVertex = i * 3;
            if (baseVertex + 2 < MAX_VERTS && tess.numVertexes + 3 < MAX_SHADER_VERTEXES) {
                // Add triangle vertices to tessellation buffer
                for (int j = 0; j < 3; j++) {
                    int vertIdx = vk.scene.polygonIndexes[baseVertex + j];
                    if (vertIdx < MAX_VERTS && tess.numVertexes < MAX_SHADER_VERTEXES) {
                        const polyVert_t *vert = &vk.scene.polygonVerts[vertIdx];
                        
                        // Copy vertex position
                        VectorCopy(vert->xyz, tess.xyz[tess.numVertexes]);
                        
                        // Copy normal (default to up if not set)
                        VectorCopy(vert->normal, tess.normal[tess.numVertexes]);
                        if (VectorLength(tess.normal[tess.numVertexes]) < 0.1f) {
                            VectorSet(tess.normal[tess.numVertexes], 0, 0, 1);
                        }
                        
                        // Copy texture coordinates
                        Vector2Copy(vert->st, tess.texCoords[0][tess.numVertexes]);
                        
                        // Copy vertex color (modulate)
                        tess.vertexColors[tess.numVertexes][0] = vert->modulate[0];
                        tess.vertexColors[tess.numVertexes][1] = vert->modulate[1];
                        tess.vertexColors[tess.numVertexes][2] = vert->modulate[2];
                        tess.vertexColors[tess.numVertexes][3] = vert->modulate[3];
                        
                        tess.indexes[tess.numIndexes] = tess.numVertexes;
                        tess.numIndexes++;
                        tess.numVertexes++;
                    }
                }
            }
        }
        
        // Render accumulated polygons if we have any
        if (tess.numVertexes > 0 && tess.numIndexes > 0) {
            // Get default shader for polygons
            shader_t *shader = tr.defaultShader;
            if (shader) {
                extern VkPipeline vk_find_pipeline(shader_t *shader);
                extern void vk_bind_pipeline(VkPipeline pipeline);
                extern void vk_bind_geometry(uint32_t flags);
                extern void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed);
                
                VkPipeline pipeline = vk_find_pipeline(shader);
                if (pipeline != VK_NULL_HANDLE) {
                    // Set shader for tessellation system
                    tess.shader = shader;
                    
                    // Bind pipeline (uses vk.cmd->command_buffer which we set above)
                    vk_bind_pipeline(pipeline);
                    
                    // Bind geometry using existing system (sets up vertex/index buffers)
                    // Use TESS_XYZ | TESS_RGBA0 | TESS_ST0 flags for standard polygon rendering
                    vk_bind_geometry(TESS_XYZ | TESS_RGBA0 | TESS_ST0);
                    
                    // Draw indexed geometry (uses vk.cmd->command_buffer)
                    vk_draw_geometry(DEPTH_RANGE_NORMAL, qtrue);
                }
            }
        }
    }

    // Render entities (if any) by adding them to refdef
    if (vk.scene.entityCount > 0) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Rendering %d entities\n", vk.scene.entityCount);

        // Entities should be added to refdef before rendering
        // For now, we'll render them directly if refdef is available
        if (fd && fd->entities) {
            // Copy entities to refdef (if space available)
            int maxEntities = MIN(vk.scene.entityCount, fd->numEntities + MAX_REFENTITIES - fd->numEntities);
            for (int i = 0; i < maxEntities; i++) {
                fd->entities[fd->numEntities + i] = vk.scene.entities[i];
            }
            fd->numEntities += maxEntities;
        } else {
            // Fallback: render entities directly using model rendering
            for (int i = 0; i < vk.scene.entityCount; i++) {
                const refEntity_t *ent = &vk.scene.entities[i];
                
                // Render model if entity has one
                if (ent->hModel > 0) {
                    extern void R_AddRefEntityToScene(const refEntity_t *re, qboolean intShaderTime);
                    // Add entity to main scene for rendering by normal pipeline
                    R_AddRefEntityToScene(ent, qfalse);
                }
            }
        }
    }

    // End render pass
    qvkCmdEndRenderPass(command_buffer);

    // Restore original command buffer context
    if (vk.cmd) {
        vk.cmd->command_buffer = saved_command_buffer;
    }

    // End command buffer
    end_command_buffer(command_buffer, "scene render");

    ri.Printf(PRINT_DEVELOPER, "Vulkan: Recorded render commands for frame\n");

    Q_UNUSED(fd); // Scene rendering doesn't directly use refdef yet
}