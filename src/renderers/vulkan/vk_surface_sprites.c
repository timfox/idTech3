/*
=============================================================================
Surface Sprites System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk_surface_sprites.h"
#include "vk_utils.h"
#include "vk_pipeline.h"
#include "vk_terrain.h"
#include <string.h>
#include <stdlib.h>

#ifdef USE_VULKAN

// CVars
cvar_t *r_surfaceSprites;
cvar_t *r_surfaceSpritesMax;
cvar_t *r_surfaceSpritesDistance;
cvar_t *r_surfaceSpritesWind;

// Global system state
static surface_sprites_system_t ss_system;

// Forward declarations
static qboolean vk_create_surface_sprites_resources(void);
static void vk_destroy_surface_sprites_resources(void);
static void vk_create_surface_sprites_pipeline(void);
static void vk_destroy_surface_sprites_pipeline(void);
static void vk_update_surface_sprites_descriptors(void);
static void vk_update_sprite_batches(void);
static void vk_build_sprite_geometry(surface_sprite_batch_t *batch, int start_sprite, int num_sprites);
static qboolean vk_is_sprite_visible(const surface_sprite_t *sprite, const surface_sprite_type_t *type);
static void vk_apply_wind_effect(surface_sprite_t *sprite, const surface_sprite_type_t *type, float time);

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name((uint64_t)(obj), (objName), (objType))

// Vulkan function pointers
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool qvkCreateDescriptorPool;
extern PFN_vkDestroyDescriptorPool qvkDestroyDescriptorPool;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers qvkCmdBindVertexBuffers;
extern PFN_vkCmdBindIndexBuffer qvkCmdBindIndexBuffer;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdDrawIndexed qvkCmdDrawIndexed;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Initialize surface sprites system
void vk_surface_sprites_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing surface sprites system\n");

    memset(&ss_system, 0, sizeof(ss_system));

    // Register CVars
    r_surfaceSprites = ri.Cvar_Get("r_surfaceSprites", "1", CVAR_ARCHIVE);
    r_surfaceSpritesMax = ri.Cvar_Get("r_surfaceSpritesMax", "4096", CVAR_ARCHIVE);
    r_surfaceSpritesDistance = ri.Cvar_Get("r_surfaceSpritesDistance", "1000.0", CVAR_ARCHIVE);
    r_surfaceSpritesWind = ri.Cvar_Get("r_surfaceSpritesWind", "1", CVAR_ARCHIVE);

    // Allocate sprite arrays
    ss_system.max_sprites = r_surfaceSpritesMax->integer;
    ss_system.sprites = ri.Hunk_Alloc(ss_system.max_sprites * sizeof(surface_sprite_t), h_low);

    if (!ss_system.sprites) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to allocate surface sprite array\n");
        return;
    }

    // Allocate batch array
    ss_system.max_batches = (ss_system.max_sprites + SURFACE_SPRITE_BATCH_SIZE - 1) / SURFACE_SPRITE_BATCH_SIZE;
    ss_system.batches = ri.Hunk_Alloc(ss_system.max_batches * sizeof(surface_sprite_batch_t), h_low);

    if (!ss_system.batches) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to allocate surface sprite batch array\n");
        return;
    }

    // Initialize wind system
    VectorSet(ss_system.wind_direction, 1.0f, 0.0f, 0.0f);
    ss_system.wind_strength = 0.5f;
    ss_system.wind_frequency = 0.5f;

    // Create Vulkan resources
    if (!vk_create_surface_sprites_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create surface sprites resources\n");
        return;
    }

    // Create pipeline
    vk_create_surface_sprites_pipeline();

    // Update descriptors
    vk_update_surface_sprites_descriptors();

    ss_system.initialized = qtrue;
    ss_system.enabled = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Surface sprites system initialized with capacity for %d sprites\n", ss_system.max_sprites);
}

// Shutdown surface sprites system
void vk_surface_sprites_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down surface sprites system\n");

    vk_destroy_surface_sprites_pipeline();
    vk_destroy_surface_sprites_resources();

    if (ss_system.sprites) {
        ri.Hunk_Free(ss_system.sprites);
        ss_system.sprites = NULL;
    }

    if (ss_system.batches) {
        ri.Hunk_Free(ss_system.batches);
        ss_system.batches = NULL;
    }

    ss_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Surface sprites system shut down\n");
}

// Update surface sprites system
void vk_surface_sprites_update(void) {
    if (!ss_system.initialized || !ss_system.enabled) {
        return;
    }

    float current_time = vk.refdef.floatTime;
    qboolean needs_update = qfalse;

    // Update sprite animations and wind effects
    for (int i = 0; i < ss_system.num_sprites; i++) {
        surface_sprite_t *sprite = &ss_system.sprites[i];
        surface_sprite_type_t *type = &ss_system.types[sprite->current_frame]; // This needs to be fixed

        // Update animation
        if (type->animated && type->num_frames > 1) {
            sprite->animation_time += vk.frametime * 0.001f;
            int new_frame = (int)(sprite->animation_time / type->frame_time) % type->num_frames;
            if (new_frame != sprite->current_frame) {
                sprite->current_frame = new_frame;
                needs_update = qtrue;
            }
        }

        // Apply wind effect
        if (r_surfaceSpritesWind->integer && type->wind_affected) {
            vk_apply_wind_effect(sprite, type, current_time);
        }
    }

    // Update batches if needed
    if (needs_update) {
        vk_update_sprite_batches();
    }
}

// Render surface sprites
void vk_surface_sprites_render(void) {
    if (!ss_system.initialized || !ss_system.enabled || ss_system.num_batches == 0) {
        return;
    }

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ss_system.pipeline);

    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ss_system.pipeline_layout, 0, 1, &ss_system.descriptor_set, 0, NULL);

    // Push constants
    struct {
        matrix_t mvp_matrix;
        vec4_t camera_pos;
        vec4_t wind_params;
        float time;
        int num_types;
    } pushConstants;

    MatrixMultiply(vk.view_matrix, vk.projection_matrix, pushConstants.mvp_matrix);
    VectorCopy(vk.refdef.vieworg, pushConstants.camera_pos);
    pushConstants.wind_params[0] = ss_system.wind_direction[0];
    pushConstants.wind_params[1] = ss_system.wind_direction[1];
    pushConstants.wind_params[2] = ss_system.wind_direction[2];
    pushConstants.wind_params[3] = ss_system.wind_strength;
    pushConstants.time = vk.refdef.floatTime;
    pushConstants.num_types = ss_system.num_types;

    qvkCmdPushConstants(vk.cmd->command_buffer, ss_system.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pushConstants), &pushConstants);

    // Render batches
    ss_system.visible_sprites = 0;
    ss_system.culled_sprites = 0;

    for (int i = 0; i < ss_system.num_batches; i++) {
        surface_sprite_batch_t *batch = &ss_system.batches[i];

        if (batch->sprite_count == 0) {
            continue;
        }

        // Bind vertex/index buffers
        VkDeviceSize offset = 0;
        qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 0, 1, &batch->vertex_buffer, &offset);
        qvkCmdBindIndexBuffer(vk.cmd->command_buffer, batch->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw batch
        qvkCmdDrawIndexed(vk.cmd->command_buffer, batch->index_count, 1, 0, 0, 0);

        ss_system.visible_sprites += batch->sprite_count;
    }
}

// Register a new sprite type
int vk_surface_sprites_register_type(const char *name, const char *texture_path,
                                   vec2_t size, vec3_t color, float alpha,
                                   qboolean animated, int num_frames, float frame_time) {
    if (!ss_system.initialized || ss_system.num_types >= MAX_SURFACE_SPRITE_TYPES) {
        return -1;
    }

    surface_sprite_type_t *type = &ss_system.types[ss_system.num_types];

    Q_strncpyz(type->name, name, sizeof(type->name));
    type->texture = ri.RE_RegisterShader(texture_path);
    VectorCopy2(size, type->size);
    VectorCopy(color, type->color);
    type->alpha = alpha;
    type->animated = animated;
    type->num_frames = animated ? num_frames : 1;
    type->frame_time = frame_time;
    type->wind_affected = qtrue;
    type->wind_strength = 0.5f;
    type->fade_with_distance = qtrue;
    type->max_distance = r_surfaceSpritesDistance->value;
    type->density = 1.0f;

    return ss_system.num_types++;
}

// Unregister a sprite type
void vk_surface_sprites_unregister_type(int type_index) {
    if (type_index < 0 || type_index >= ss_system.num_types) {
        return;
    }

    // Remove all sprites of this type
    for (int i = ss_system.num_sprites - 1; i >= 0; i--) {
        // TODO: Check sprite type and remove if matches
        // For now, this is a stub
    }

    // Shift remaining types
    for (int i = type_index; i < ss_system.num_types - 1; i++) {
        ss_system.types[i] = ss_system.types[i + 1];
    }

    ss_system.num_types--;
}

// Add a sprite instance
int vk_surface_sprites_add_sprite(int type_index, const vec3_t position, const vec3_t normal,
                                float scale, float rotation) {
    if (!ss_system.initialized || type_index < 0 || type_index >= ss_system.num_types ||
        ss_system.num_sprites >= ss_system.max_sprites) {
        return -1;
    }

    surface_sprite_t *sprite = &ss_system.sprites[ss_system.num_sprites];

    VectorCopy(position, sprite->position);
    VectorCopy(normal, sprite->normal);

    // Calculate tangent
    vec3_t up = {0, 1, 0};
    if (fabs(DotProduct(sprite->normal, up)) > 0.9f) {
        up[0] = 1; up[1] = 0; up[2] = 0;
    }
    CrossProduct(sprite->normal, up, sprite->tangent);
    VectorNormalize(sprite->tangent);

    sprite->scale = scale;
    sprite->rotation = rotation;
    sprite->animation_time = 0.0f;
    sprite->current_frame = 0;

    // Add some random variation
    sprite->color_offset[0] = (rand() % 100 - 50) * 0.01f;
    sprite->color_offset[1] = (rand() % 100 - 50) * 0.01f;
    sprite->color_offset[2] = (rand() % 100 - 50) * 0.01f;
    sprite->alpha_offset = (rand() % 50) * 0.01f;

    // Mark batches as dirty
    vk_update_sprite_batches();

    return ss_system.num_sprites++;
}

// Remove a sprite
void vk_surface_sprites_remove_sprite(int sprite_index) {
    if (sprite_index < 0 || sprite_index >= ss_system.num_sprites) {
        return;
    }

    // Shift remaining sprites
    for (int i = sprite_index; i < ss_system.num_sprites - 1; i++) {
        ss_system.sprites[i] = ss_system.sprites[i + 1];
    }

    ss_system.num_sprites--;
    vk_update_sprite_batches();
}

// Clear all sprites
void vk_surface_sprites_clear_all(void) {
    ss_system.num_sprites = 0;
    for (int i = 0; i < ss_system.num_batches; i++) {
        ss_system.batches[i].sprite_count = 0;
        ss_system.batches[i].dirty = qtrue;
    }
}

// Populate terrain with sprites
void vk_surface_sprites_populate_terrain(int type_index, const vec3_t mins, const vec3_t maxs,
                                       float density, qboolean use_heightmap) {
    if (!ss_system.initialized || type_index < 0 || type_index >= ss_system.num_types) {
        return;
    }

    surface_sprite_type_t *type = &ss_system.types[type_index];
    vec3_t area_size;
    VectorSubtract(maxs, mins, area_size);

    int num_sprites_x = (int)(area_size[0] * density * type->density);
    int num_sprites_z = (int)(area_size[2] * density * type->density);

    // Clamp to reasonable limits
    num_sprites_x = ri.Min(num_sprites_x, 100);
    num_sprites_z = ri.Min(num_sprites_z, 100);

    float spacing_x = area_size[0] / num_sprites_x;
    float spacing_z = area_size[2] / num_sprites_z;

    for (int x = 0; x < num_sprites_x; x++) {
        for (int z = 0; z < num_sprites_z; z++) {
            vec3_t position;
            position[0] = mins[0] + x * spacing_x + (rand() % 100 - 50) * 0.01f * spacing_x;
            position[2] = mins[2] + z * spacing_z + (rand() % 100 - 50) * 0.01f * spacing_z;

            if (use_heightmap && vk_terrain_get_height) {
                position[1] = vk_terrain_get_height((int)position[0], (int)position[2]);
            } else {
                position[1] = mins[1];
            }

            // Add some random rotation and scale variation
            float rotation = (rand() % 360) * (M_PI / 180.0f);
            float scale = 0.8f + (rand() % 40 - 20) * 0.01f;

            vk_surface_sprites_add_sprite(type_index, position, (vec3_t){0, 1, 0}, scale, rotation);
        }
    }
}

// Set wind parameters
void vk_surface_sprites_set_wind(const vec3_t direction, float strength, float frequency) {
    VectorCopy(direction, ss_system.wind_direction);
    VectorNormalize(ss_system.wind_direction);
    ss_system.wind_strength = strength;
    ss_system.wind_frequency = frequency;
}

// Get wind parameters
void vk_surface_sprites_get_wind(vec3_t direction, float *strength, float *frequency) {
    VectorCopy(ss_system.wind_direction, direction);
    if (strength) *strength = ss_system.wind_strength;
    if (frequency) *frequency = ss_system.wind_frequency;
}

// Get system info
int vk_surface_sprites_get_type_count(void) {
    return ss_system.num_types;
}

int vk_surface_sprites_get_sprite_count(void) {
    return ss_system.num_sprites;
}

surface_sprite_type_t *vk_surface_sprites_get_type(int index) {
    if (index < 0 || index >= ss_system.num_types) {
        return NULL;
    }
    return &ss_system.types[index];
}

// Ray tracing for sprite interaction
qboolean vk_surface_sprites_trace(const vec3_t start, const vec3_t end, vec3_t hit_pos, int *sprite_index) {
    // TODO: Implement sprite ray tracing for interaction
    return qfalse;
}

// Create Vulkan resources for surface sprites
static qboolean vk_create_surface_sprites_resources(void) {
    // Resources are created per batch as needed
    ri.Printf(PRINT_ALL, "Vulkan: Surface sprites resources created\n");
    return qtrue;
}

// Destroy Vulkan resources for surface sprites
static void vk_destroy_surface_sprites_resources(void) {
    for (int i = 0; i < ss_system.num_batches; i++) {
        surface_sprite_batch_t *batch = &ss_system.batches[i];

        if (batch->vertex_buffer) {
            qvkDestroyBuffer(vk.device, batch->vertex_buffer, NULL);
            batch->vertex_buffer = VK_NULL_HANDLE;
        }
        if (batch->index_buffer) {
            qvkDestroyBuffer(vk.device, batch->index_buffer, NULL);
            batch->index_buffer = VK_NULL_HANDLE;
        }
        if (batch->vertex_memory) {
            qvkFreeMemory(vk.device, batch->vertex_memory, NULL);
            batch->vertex_memory = VK_NULL_HANDLE;
        }
        if (batch->index_memory) {
            qvkFreeMemory(vk.device, batch->index_memory, NULL);
            batch->index_memory = VK_NULL_HANDLE;
        }
    }
}

// Create graphics pipeline for surface sprites
static void vk_create_surface_sprites_pipeline(void) {
    // TODO: Create surface sprite rendering pipeline
    // This would include vertex, geometry (for billboarding), and fragment shaders
    ri.Printf(PRINT_WARNING, "vk_create_surface_sprites_pipeline: Not implemented yet\n");
}

// Destroy graphics pipeline for surface sprites
static void vk_destroy_surface_sprites_pipeline(void) {
    if (ss_system.pipeline) {
        qvkDestroyPipeline(vk.device, ss_system.pipeline, NULL);
        ss_system.pipeline = VK_NULL_HANDLE;
    }

    if (ss_system.pipeline_layout) {
        qvkDestroyPipelineLayout(vk.device, ss_system.pipeline_layout, NULL);
        ss_system.pipeline_layout = VK_NULL_HANDLE;
    }

    if (ss_system.descriptor_layout) {
        qvkDestroyDescriptorSetLayout(vk.device, ss_system.descriptor_layout, NULL);
        ss_system.descriptor_layout = VK_NULL_HANDLE;
    }

    if (ss_system.descriptor_pool) {
        qvkDestroyDescriptorPool(vk.device, ss_system.descriptor_pool, NULL);
        ss_system.descriptor_pool = VK_NULL_HANDLE;
    }
}

// Update descriptor sets for surface sprites
static void vk_update_surface_sprites_descriptors(void) {
    // TODO: Update surface sprite descriptors
}

// Update sprite batches
static void vk_update_sprite_batches(void) {
    int sprites_per_batch = SURFACE_SPRITE_BATCH_SIZE;
    ss_system.num_batches = (ss_system.num_sprites + sprites_per_batch - 1) / sprites_per_batch;

    for (int i = 0; i < ss_system.num_batches; i++) {
        int start_sprite = i * sprites_per_batch;
        int num_sprites = ri.Min(sprites_per_batch, ss_system.num_sprites - start_sprite);

        surface_sprite_batch_t *batch = &ss_system.batches[i];
        batch->sprite_count = num_sprites;
        batch->dirty = qtrue;

        if (batch->dirty) {
            vk_build_sprite_geometry(batch, start_sprite, num_sprites);
            batch->dirty = qfalse;
        }
    }
}

// Build geometry for a sprite batch
static void vk_build_sprite_geometry(surface_sprite_batch_t *batch, int start_sprite, int num_sprites) {
    batch->vertex_count = num_sprites * 4; // 4 vertices per sprite (quad)
    batch->index_count = num_sprites * 6;  // 6 indices per sprite (2 triangles)

    // Allocate vertex data
    vec4_t *vertices = ri.Hunk_Alloc(batch->vertex_count * sizeof(vec4_t), h_low);
    uint32_t *indices = ri.Hunk_Alloc(batch->index_count * sizeof(uint32_t), h_low);

    if (!vertices || !indices) {
        ri.Printf(PRINT_ERROR, "vk_build_sprite_geometry: Failed to allocate geometry memory\n");
        return;
    }

    // Generate geometry for each sprite
    for (int i = 0; i < num_sprites; i++) {
        surface_sprite_t *sprite = &ss_system.sprites[start_sprite + i];
        int base_vertex = i * 4;
        int base_index = i * 6;

        // Calculate billboarded quad vertices
        vec3_t right, up;
        VectorScale(sprite->tangent, sprite->scale, right);
        VectorScale(sprite->normal, sprite->scale, up);

        // Apply rotation
        float cos_rot = cosf(sprite->rotation);
        float sin_rot = sinf(sprite->rotation);

        vec3_t rotated_right, rotated_up;
        rotated_right[0] = right[0] * cos_rot - up[0] * sin_rot;
        rotated_right[1] = right[1] * cos_rot - up[1] * sin_rot;
        rotated_right[2] = right[2] * cos_rot - up[2] * sin_rot;

        rotated_up[0] = right[0] * sin_rot + up[0] * cos_rot;
        rotated_up[1] = right[1] * sin_rot + up[1] * cos_rot;
        rotated_up[2] = right[2] * sin_rot + up[2] * cos_rot;

        // Bottom-left
        VectorAdd(sprite->position, rotated_right, vertices[base_vertex + 0]);
        VectorSubtract(vertices[base_vertex + 0], rotated_up, vertices[base_vertex + 0]);
        vertices[base_vertex + 0][3] = 0.0f; // UV u
        ((float*)&vertices[base_vertex + 0])[3] = 1.0f; // UV v

        // Bottom-right
        VectorAdd(sprite->position, rotated_right, vertices[base_vertex + 1]);
        VectorAdd(vertices[base_vertex + 1], rotated_up, vertices[base_vertex + 1]);
        vertices[base_vertex + 1][3] = 1.0f; // UV u
        ((float*)&vertices[base_vertex + 1])[3] = 1.0f; // UV v

        // Top-right
        VectorSubtract(sprite->position, rotated_right, vertices[base_vertex + 2]);
        VectorAdd(vertices[base_vertex + 2], rotated_up, vertices[base_vertex + 2]);
        vertices[base_vertex + 2][3] = 1.0f; // UV u
        ((float*)&vertices[base_vertex + 2])[3] = 0.0f; // UV v

        // Top-left
        VectorSubtract(sprite->position, rotated_right, vertices[base_vertex + 3]);
        VectorSubtract(vertices[base_vertex + 3], rotated_up, vertices[base_vertex + 3]);
        vertices[base_vertex + 3][3] = 0.0f; // UV u
        ((float*)&vertices[base_vertex + 3])[3] = 0.0f; // UV v

        // Indices
        indices[base_index + 0] = base_vertex + 0;
        indices[base_index + 1] = base_vertex + 1;
        indices[base_index + 2] = base_vertex + 2;
        indices[base_index + 3] = base_vertex + 0;
        indices[base_index + 4] = base_vertex + 2;
        indices[base_index + 5] = base_vertex + 3;
    }

    // Upload to GPU
    // TODO: Implement vertex buffer creation and upload

    // Free temporary buffers
    ri.Hunk_Free(vertices);
    ri.Hunk_Free(indices);
}

// Check if sprite is visible
static qboolean vk_is_sprite_visible(const surface_sprite_t *sprite, const surface_sprite_type_t *type) {
    // Distance culling
    float distance = Distance(vk.refdef.vieworg, sprite->position);
    if (type->fade_with_distance && distance > type->max_distance) {
        return qfalse;
    }

    // Frustum culling
    vec3_t mins, maxs;
    mins[0] = sprite->position[0] - type->size[0] * 0.5f;
    mins[1] = sprite->position[1];
    mins[2] = sprite->position[2] - type->size[1] * 0.5f;
    maxs[0] = sprite->position[0] + type->size[0] * 0.5f;
    maxs[1] = sprite->position[1] + type->size[1];
    maxs[2] = sprite->position[2] + type->size[1] * 0.5f;

    return R_CullBox(mins, maxs);
}

// Apply wind effect to sprite
static void vk_apply_wind_effect(surface_sprite_t *sprite, const surface_sprite_type_t *type, float time) {
    // Simple wind animation - bend the sprite based on wind direction and strength
    float wind_factor = sinf(time * ss_system.wind_frequency + sprite->position[0] * 0.1f + sprite->position[2] * 0.1f);
    wind_factor *= ss_system.wind_strength * type->wind_strength;

    // Apply wind to rotation (simple bending effect)
    sprite->rotation += wind_factor * 0.1f;

    // Clamp rotation to reasonable bounds
    if (sprite->rotation > M_PI * 0.25f) sprite->rotation = M_PI * 0.25f;
    if (sprite->rotation < -M_PI * 0.25f) sprite->rotation = -M_PI * 0.25f;
}

#endif // USE_VULKAN