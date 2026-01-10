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
#include <math.h>

// Helper function declarations
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
extern VkCommandBuffer begin_command_buffer(void);
extern void end_command_buffer(VkCommandBuffer command_buffer, const char *location);

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
    // Off by default until the rendering path is fully validated.
    r_surfaceSprites = ri.Cvar_Get("r_surfaceSprites", "0", CVAR_ARCHIVE);
    r_surfaceSpritesMax = ri.Cvar_Get("r_surfaceSpritesMax", "4096", CVAR_ARCHIVE);
    r_surfaceSpritesDistance = ri.Cvar_Get("r_surfaceSpritesDistance", "1000.0", CVAR_ARCHIVE);
    r_surfaceSpritesWind = ri.Cvar_Get("r_surfaceSpritesWind", "1", CVAR_ARCHIVE);

    if (!r_surfaceSprites->integer) {
        return;
    }

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

    // Allocated from the hunk; do not free individually.
    ss_system.sprites = NULL;
    ss_system.batches = NULL;

    ss_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Surface sprites system shut down\n");
}

// Update surface sprites system
void vk_surface_sprites_update(void) {
    if (!ss_system.initialized || !ss_system.enabled) {
        return;
    }
    
    float current_time = tr.refdef.floatTime;
    qboolean needs_batch_update = qfalse;
    
    // Update sprite animations and wind effects
    for (int i = 0; i < ss_system.num_sprites; i++) {
        surface_sprite_t *sprite = &ss_system.sprites[i];
        
        // Find sprite type
        int type_index = -1;
        for (int t = 0; t < ss_system.num_types; t++) {
            // Note: We'd need to track type_index per sprite
            // For now, assume all sprites are type 0
            type_index = 0;
            break;
        }
        
        if (type_index >= 0 && type_index < ss_system.num_types) {
            surface_sprite_type_t *type = &ss_system.types[type_index];
            
            // Update animation
            if (type->animated) {
                float delta_time = tr.refdef.floatTime - sprite->animation_time;
                sprite->animation_time = tr.refdef.floatTime;
                if (delta_time >= type->frame_time) {
                    sprite->current_frame = (sprite->current_frame + 1) % type->num_frames;
                    needs_batch_update = qtrue;
                }
            }
            
            // Apply wind effects
            if (type->wind_affected && r_surfaceSpritesWind->integer) {
                vk_apply_wind_effect(sprite, type, current_time);
                needs_batch_update = qtrue;
            }
        }
    }
    
    // Update batches if needed
    if (needs_batch_update) {
        vk_update_sprite_batches();
    }
}

// Render surface sprites
void vk_surface_sprites_render(void) {
    if (!ss_system.initialized || !ss_system.enabled || ss_system.num_batches == 0) {
        return;
    }

    if (ss_system.pipeline == VK_NULL_HANDLE || ss_system.pipeline_layout == VK_NULL_HANDLE) {
        return;
    }

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ss_system.pipeline);

    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ss_system.pipeline_layout, 0, 1, &ss_system.descriptor_set, 0, NULL);

    // Push constants for MVP matrix, camera position, and time
    // Note: Push constants would need to be added to pipeline layout if shaders require them
    // For now, we rely on the standard renderer pipeline state

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
    type->texture = RE_RegisterShader(texture_path);
    // VectorCopy2 doesn't exist, use VectorCopy for vec2_t
    type->size[0] = size[0];
    type->size[1] = size[1];
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
        // TODO: Check sprite type and remove if matches.
        //       Implementation:
        //       if (ss_system.sprites[i].type == type) {
        //           Remove sprite at index i (shift remaining sprites)
        //           ss_system.num_sprites--
        //       }
        //       For now, this is a stub
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
    num_sprites_x = MIN(num_sprites_x, 100);
    num_sprites_z = MIN(num_sprites_z, 100);

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
    // TODO: Implement sprite ray tracing for interaction.
    //       Implementation steps:
    //       1. Iterate through all active sprites in ss_system.sprites[]
    //       2. For each sprite, compute billboard quad geometry (4 vertices)
    //       3. Perform ray-quad intersection test
    //       4. If intersection found, store hit position and sprite index
    //       5. Return qtrue if any intersection found, qfalse otherwise
    //       Note: Sprites are billboarded quads, so intersection requires transforming
    //       ray to sprite's local space and testing against quad plane
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
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        // Sprite texture atlas
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(bindings),
        .pBindings = bindings
    };

    VkResult result = qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &ss_system.descriptor_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_surface_sprites_pipeline: Failed to create descriptor set layout\n");
        return;
    }

    // Pipeline layout with push constants
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(float) * 16 + sizeof(float) * 3 + sizeof(float) // MVP + view pos + time
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ss_system.descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    result = qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &ss_system.pipeline_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_surface_sprites_pipeline: Failed to create pipeline layout\n");
        return;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_LEN(poolSizes),
        .pPoolSizes = poolSizes,
        .maxSets = 1
    };

    result = qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &ss_system.descriptor_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_surface_sprites_pipeline: Failed to create descriptor pool\n");
        return;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ss_system.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ss_system.descriptor_layout
    };

    result = qvkAllocateDescriptorSets(vk.device, &allocInfo, &ss_system.descriptor_set);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_surface_sprites_pipeline: Failed to allocate descriptor set\n");
        return;
    }

    // Load shader modules
    extern VkShaderModule vk_load_shader(const char *shader_name, VkShaderStageFlagBits stage);
    VkShaderModule vertModule = vk_load_shader("surface_sprite_vert", VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule fragModule = vk_load_shader("surface_sprite_frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    
    // Fallback to generic shaders if sprite-specific shaders don't exist
    if (vertModule == VK_NULL_HANDLE) {
        vertModule = vk_load_shader("color_vert", VK_SHADER_STAGE_VERTEX_BIT);
    }
    if (fragModule == VK_NULL_HANDLE) {
        fragModule = vk_load_shader("color_frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "vk_create_surface_sprites_pipeline: Failed to load shader modules\n");
        if (vertModule != VK_NULL_HANDLE) qvkDestroyShaderModule(vk.device, vertModule, NULL);
        if (fragModule != VK_NULL_HANDLE) qvkDestroyShaderModule(vk.device, fragModule, NULL);
        return;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragModule,
            .pName = "main"
        }
    };

    // Vertex input (position + UV)
    VkVertexInputBindingDescription vertexBinding = {
        .binding = 0,
        .stride = sizeof(vec4_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertexAttributes[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = ARRAY_LEN(vertexAttributes),
        .pVertexAttributeDescriptions = vertexAttributes
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport and scissor
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE, // Billboards face camera
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_FALSE, // Sprites don't write depth
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // Color blend (alpha blending for sprites)
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_LEN(dynamicStates),
        .pDynamicStates = dynamicStates
    };

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = ARRAY_LEN(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = ss_system.pipeline_layout,
        .renderPass = vk.render_pass.main,  // Use main render pass
        .subpass = 0
    };

    result = qvkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &ss_system.pipeline);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_surface_sprites_pipeline: Failed to create graphics pipeline: %s\n", vk_result_string(result));
    } else {
        SET_OBJECT_NAME(ss_system.pipeline, "surface_sprites_pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
        ri.Printf(PRINT_ALL, "Vulkan: Surface sprites pipeline created successfully\n");
    }

    // Cleanup shader modules (they're cached by the shader manager)
    // Don't destroy them here as they may be used elsewhere
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
    // Update descriptor set with default white texture
    // Individual sprite textures would be updated per-batch in a more advanced implementation
    VkDescriptorImageInfo imageInfo = {
        .sampler = vk.samplers.samplers[0],
        .imageView = tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ss_system.descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    };

    qvkUpdateDescriptorSets(vk.device, 1, &write, 0, NULL);
}

// Update sprite batches
static void vk_update_sprite_batches(void) {
    int sprites_per_batch = SURFACE_SPRITE_BATCH_SIZE;
    ss_system.num_batches = (ss_system.num_sprites + sprites_per_batch - 1) / sprites_per_batch;

    for (int i = 0; i < ss_system.num_batches; i++) {
        int start_sprite = i * sprites_per_batch;
        int num_sprites = MIN(sprites_per_batch, ss_system.num_sprites - start_sprite);

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
    // Create vertex buffer if needed
    if (batch->vertex_buffer == VK_NULL_HANDLE) {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = batch->vertex_count * sizeof(vec4_t),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        
        if (qvkCreateBuffer(vk.device, &bufferInfo, NULL, &batch->vertex_buffer) == VK_SUCCESS) {
            VkMemoryRequirements memReqs;
            qvkGetBufferMemoryRequirements(vk.device, batch->vertex_buffer, &memReqs);
            
            VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            };
            
            if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &batch->vertex_memory) == VK_SUCCESS) {
                qvkBindBufferMemory(vk.device, batch->vertex_buffer, batch->vertex_memory, 0);
            }
        }
    }
    
    // Create index buffer if needed
    if (batch->index_buffer == VK_NULL_HANDLE) {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = batch->index_count * sizeof(uint32_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        
        if (qvkCreateBuffer(vk.device, &bufferInfo, NULL, &batch->index_buffer) == VK_SUCCESS) {
            VkMemoryRequirements memReqs;
            qvkGetBufferMemoryRequirements(vk.device, batch->index_buffer, &memReqs);
            
            VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            };
            
            if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &batch->index_memory) == VK_SUCCESS) {
                qvkBindBufferMemory(vk.device, batch->index_buffer, batch->index_memory, 0);
            }
        }
    }
    
    // Upload vertex data via staging buffer
    if (batch->vertex_buffer != VK_NULL_HANDLE) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        VkBufferCreateInfo stagingInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = batch->vertex_count * sizeof(vec4_t),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        
        if (qvkCreateBuffer(vk.device, &stagingInfo, NULL, &stagingBuffer) == VK_SUCCESS) {
            VkMemoryRequirements memReqs;
            qvkGetBufferMemoryRequirements(vk.device, stagingBuffer, &memReqs);
            
            VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            };
            
            if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &stagingMemory) == VK_SUCCESS) {
                qvkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0);
                
                void *stagingData;
                qvkMapMemory(vk.device, stagingMemory, 0, batch->vertex_count * sizeof(vec4_t), 0, &stagingData);
                memcpy(stagingData, vertices, batch->vertex_count * sizeof(vec4_t));
                qvkUnmapMemory(vk.device, stagingMemory);
                
                VkCommandBuffer cmdBuf = begin_command_buffer();
                VkBufferCopy copyRegion = {.size = batch->vertex_count * sizeof(vec4_t)};
                extern PFN_vkCmdCopyBuffer qvkCmdCopyBuffer;
                if (qvkCmdCopyBuffer) {
                    qvkCmdCopyBuffer(cmdBuf, stagingBuffer, batch->vertex_buffer, 1, &copyRegion);
                }
                end_command_buffer(cmdBuf, __func__);
                
                qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
                qvkFreeMemory(vk.device, stagingMemory, NULL);
            }
        }
    }
    
    // Upload index data via staging buffer
    if (batch->index_buffer != VK_NULL_HANDLE) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        VkBufferCreateInfo stagingInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = batch->index_count * sizeof(uint32_t),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        
        if (qvkCreateBuffer(vk.device, &stagingInfo, NULL, &stagingBuffer) == VK_SUCCESS) {
            VkMemoryRequirements memReqs;
            qvkGetBufferMemoryRequirements(vk.device, stagingBuffer, &memReqs);
            
            VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            };
            
            if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &stagingMemory) == VK_SUCCESS) {
                qvkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0);
                
                void *stagingData;
                qvkMapMemory(vk.device, stagingMemory, 0, batch->index_count * sizeof(uint32_t), 0, &stagingData);
                memcpy(stagingData, indices, batch->index_count * sizeof(uint32_t));
                qvkUnmapMemory(vk.device, stagingMemory);
                
                VkCommandBuffer cmdBuf = begin_command_buffer();
                VkBufferCopy copyRegion = {.size = batch->index_count * sizeof(uint32_t)};
                qvkCmdCopyBuffer(cmdBuf, stagingBuffer, batch->index_buffer, 1, &copyRegion);
                end_command_buffer(cmdBuf, __func__);
                
                qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
                qvkFreeMemory(vk.device, stagingMemory, NULL);
            }
        }
    }

    // Free temporary buffers (hunk-allocated, will be freed with hunk)
    // Note: Don't call ri.Hunk_Free here as these are temporary allocations
}

// Check if sprite is visible
static qboolean vk_is_sprite_visible(const surface_sprite_t *sprite, const surface_sprite_type_t *type) {
    // Distance culling
    float distance = Distance(tr.refdef.vieworg, sprite->position);
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

    // Convert world-space bounds to local space for culling
    vec3_t localBounds[2];
    VectorCopy(mins, localBounds[0]);
    VectorCopy(maxs, localBounds[1]);
    return R_CullLocalBox(localBounds);
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