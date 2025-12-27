/*
=============================================================================
Decals System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk_decals.h"
#include "vk_utils.h"
#include "vk_pipeline.h"
#include "vk.h"

// Embedded shader data
extern const unsigned char decal_vert_spv[];
extern const unsigned char decal_frag_spv[];
#include <string.h>

#ifdef USE_VULKAN

// CVars
cvar_t *r_decals;
cvar_t *r_decalsMax;
cvar_t *r_decalsFadeTime;

// Global system state
static decal_system_t ds_system;

// Forward declarations
static qboolean vk_create_decal_resources(void);
static void vk_destroy_decal_resources(void);
static void vk_create_decal_pipeline(void);
static void vk_destroy_decal_pipeline(void);
static void vk_update_decal_descriptors(void);
static void vk_build_decal_geometry(decal_t *decal);
static void vk_update_decal_vertex_buffer(void);
static int vk_find_free_decal_slot(void);

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

// Initialize decals system
void vk_decals_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing decals system\n");

    memset(&ds_system, 0, sizeof(ds_system));

    // Register CVars
    // Off by default until the rendering path is fully validated.
    r_decals = ri.Cvar_Get("r_decals", "0", CVAR_ARCHIVE);
    r_decalsMax = ri.Cvar_Get("r_decalsMax", "512", CVAR_ARCHIVE);
    r_decalsFadeTime = ri.Cvar_Get("r_decalsFadeTime", "5.0", CVAR_ARCHIVE);

    if (!r_decals->integer) {
        return;
    }

    // Allocate vertex/index buffers
    ds_system.vertices = ri.Hunk_Alloc(MAX_DECAL_VERTICES * sizeof(vec4_t), h_low);
    ds_system.indices = ri.Hunk_Alloc(MAX_DECAL_INDICES * sizeof(uint32_t), h_low);

    if (!ds_system.vertices || !ds_system.indices) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to allocate decal vertex/index buffers\n");
        return;
    }

    // Create Vulkan resources
    if (!vk_create_decal_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create decal resources\n");
        return;
    }

    // Create pipeline
    vk_create_decal_pipeline();

    // Update descriptors
    vk_update_decal_descriptors();

    ds_system.initialized = qtrue;
    ds_system.enabled = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: Decals system initialized\n");
}

// Shutdown decals system
void vk_decals_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down decals system\n");

    vk_destroy_decal_pipeline();
    vk_destroy_decal_resources();

    if (ds_system.vertices) {
        ri.Hunk_Free(ds_system.vertices);
        ds_system.vertices = NULL;
    }

    if (ds_system.indices) {
        ri.Hunk_Free(ds_system.indices);
        ds_system.indices = NULL;
    }

    ds_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Decals system shut down\n");
}

// Update decals (handle lifetime, fading, etc.)
void vk_decals_update(void) {
    if (!ds_system.initialized || !ds_system.enabled) {
        return;
    }

    float current_time = tr.refdef.floatTime;
    qboolean needs_update = qfalse;

    for (int i = 0; i < MAX_DECALS; i++) {
        decal_t *decal = &ds_system.decals[i];

        if (!decal->active) {
            continue;
        }

        float age = current_time - decal->start_time;

        // Check if decal has expired
        if (age >= decal->lifetime) {
            decal->active = qfalse;
            needs_update = qtrue;
            continue;
        }

        // Handle fading
        if (decal->fade_out && age >= (decal->lifetime - decal->fade_time)) {
            float fade_progress = (age - (decal->lifetime - decal->fade_time)) / decal->fade_time;
            decal->alpha = 1.0f - fade_progress;
        } else {
            decal->alpha = 1.0f;
        }

        // Apply color tint
        decal->color[3] = decal->alpha;
    }

    // Update vertex buffer if needed
    if (needs_update) {
        vk_update_decal_vertex_buffer();
    }
}

// Render all active decals
void vk_decals_render(void) {
    if (!ds_system.initialized || !ds_system.enabled || ds_system.num_decals == 0) {
        return;
    }

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ds_system.pipeline);

    // Bind vertex/index buffers
    VkDeviceSize offset = 0;
    qvkCmdBindVertexBuffers(vk.cmd->command_buffer, 0, 1, &ds_system.vertex_buffer, &offset);
    qvkCmdBindIndexBuffer(vk.cmd->command_buffer, ds_system.index_buffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ds_system.pipeline_layout, 0, 1, &ds_system.descriptor_set, 0, NULL);

    // TODO: Push constants / per-frame uniforms once the pipeline is implemented.

    // Render decals
    int vertex_offset = 0;
    int index_offset = 0;

    for (int i = 0; i < MAX_DECALS; i++) {
        decal_t *decal = &ds_system.decals[i];

        if (!decal->active) {
            continue;
        }

        // Frustum culling (basic)
        if (R_CullPoint(decal->position)) {
            continue;
        }

        // Bind material texture
        if (decal->material_index >= 0 && decal->material_index < ds_system.num_materials) {
            decal_material_t *material = &ds_system.materials[decal->material_index];
            // TODO: Bind material texture descriptor
        }

        // Draw decal
        qvkCmdDrawIndexed(vk.cmd->command_buffer, decal->index_count, 1,
                         index_offset, vertex_offset, 0);

        vertex_offset += decal->vertex_count;
        index_offset += decal->index_count;
    }
}

// Create a new decal
int vk_decals_create_decal(decal_type_t type, const vec3_t position, const vec3_t normal,
                          float radius, float angle, float lifetime, const vec4_t color) {
    if (!ds_system.initialized || ds_system.num_decals >= MAX_DECALS) {
        return -1;
    }

    int index = vk_find_free_decal_slot();
    if (index == -1) {
        return -1;
    }

    decal_t *decal = &ds_system.decals[index];

    // Initialize decal
    decal->active = qtrue;
    decal->type = type;
    VectorCopy(position, decal->position);
    VectorCopy(normal, decal->normal);
    VectorNormalize(decal->normal);

    // Calculate tangent/binormal for orientation
    vec3_t up = {0, 0, 1};
    if (fabs(DotProduct(decal->normal, up)) > 0.9f) {
        up[0] = 1; up[1] = 0; up[2] = 0;
    }
    CrossProduct(decal->normal, up, decal->tangent);
    VectorNormalize(decal->tangent);
    CrossProduct(decal->normal, decal->tangent, decal->binormal);
    VectorNormalize(decal->binormal);

    decal->radius = radius;
    decal->angle = angle;
    decal->lifetime = lifetime;
    decal->fade_time = r_decalsFadeTime->value;
    decal->start_time = tr.refdef.floatTime;
    VectorCopy4(color, decal->color);
    decal->alpha = 1.0f;
    decal->material_index = 0; // Default material
    decal->fade_out = qtrue;

    // Build geometry
    vk_build_decal_geometry(decal);

    ds_system.num_decals++;
    vk_update_decal_vertex_buffer();

    return index;
}

// Remove a decal
void vk_decals_remove_decal(int decal_index) {
    if (decal_index < 0 || decal_index >= MAX_DECALS) {
        return;
    }

    decal_t *decal = &ds_system.decals[decal_index];
    if (decal->active) {
        decal->active = qfalse;
        ds_system.num_decals--;
        vk_update_decal_vertex_buffer();
    }
}

// Clear all decals
void vk_decals_clear_all(void) {
    for (int i = 0; i < MAX_DECALS; i++) {
        ds_system.decals[i].active = qfalse;
    }
    ds_system.num_decals = 0;
    vk_update_decal_vertex_buffer();
}

// Register a decal material
int vk_decals_register_material(const char *name, qhandle_t shader, qboolean animated,
                               float frame_time, int num_frames) {
    if (ds_system.num_materials >= 32) {
        return -1;
    }

    decal_material_t *material = &ds_system.materials[ds_system.num_materials];

    Q_strncpyz(material->name, name, sizeof(material->name));
    material->shader = shader;
    material->animated = animated;
    material->frame_time = frame_time;
    material->num_frames = num_frames;

    return ds_system.num_materials++;
}

// Get material shader
qhandle_t vk_decals_get_material_shader(int material_index) {
    if (material_index < 0 || material_index >= ds_system.num_materials) {
        return 0;
    }
    return ds_system.materials[material_index].shader;
}

// Project decal onto surface
void vk_decals_project_on_surface(const vec3_t start, const vec3_t end, vec3_t position, vec3_t normal) {
    trace_t trace;

    ri.CM_BoxTrace(&trace, start, end, NULL, NULL, 0, CONTENTS_SOLID, qfalse);

    if (trace.fraction < 1.0f) {
        VectorCopy(trace.endpos, position);
        VectorCopy(trace.plane.normal, normal);
    } else {
        VectorCopy(end, position);
        VectorSet(normal, 0, 0, 1); // Default up normal
    }
}

// Trace for surface normal
qboolean vk_decals_trace_surface(const vec3_t start, const vec3_t end, vec3_t position, vec3_t normal) {
    trace_t trace;

    ri.CM_BoxTrace(&trace, start, end, NULL, NULL, 0, CONTENTS_SOLID, qfalse);

    if (trace.fraction < 1.0f) {
        VectorCopy(trace.endpos, position);
        VectorCopy(trace.plane.normal, normal);
        return qtrue;
    }

    return qfalse;
}

// Create Vulkan resources for decals
static qboolean vk_create_decal_resources(void) {
    VkResult result;

    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = MAX_DECAL_VERTICES * sizeof(vec4_t),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    result = qvkCreateBuffer(vk.device, &vertexBufferInfo, NULL, &ds_system.vertex_buffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_resources: Failed to create vertex buffer\n");
        return qfalse;
    }

    SET_OBJECT_NAME(ds_system.vertex_buffer, "decal_vertex_buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

    // Allocate vertex buffer memory
    VkMemoryRequirements memReqs;
    qvkGetBufferMemoryRequirements(vk.device, ds_system.vertex_buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &ds_system.vertex_memory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_resources: Failed to allocate vertex buffer memory\n");
        return qfalse;
    }

    qvkBindBufferMemory(vk.device, ds_system.vertex_buffer, ds_system.vertex_memory, 0);

    // Create index buffer
    VkBufferCreateInfo indexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = MAX_DECAL_INDICES * sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    result = qvkCreateBuffer(vk.device, &indexBufferInfo, NULL, &ds_system.index_buffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_resources: Failed to create index buffer\n");
        return qfalse;
    }

    SET_OBJECT_NAME(ds_system.index_buffer, "decal_index_buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

    // Allocate index buffer memory
    qvkGetBufferMemoryRequirements(vk.device, ds_system.index_buffer, &memReqs);

    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &ds_system.index_memory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_resources: Failed to allocate index buffer memory\n");
        return qfalse;
    }

    qvkBindBufferMemory(vk.device, ds_system.index_buffer, ds_system.index_memory, 0);

    ri.Printf(PRINT_ALL, "Vulkan: Decal resources created successfully\n");
    return qtrue;
}

// Destroy Vulkan resources for decals
static void vk_destroy_decal_resources(void) {
    if (ds_system.vertex_buffer) {
        qvkDestroyBuffer(vk.device, ds_system.vertex_buffer, NULL);
        ds_system.vertex_buffer = VK_NULL_HANDLE;
    }

    if (ds_system.index_buffer) {
        qvkDestroyBuffer(vk.device, ds_system.index_buffer, NULL);
        ds_system.index_buffer = VK_NULL_HANDLE;
    }

    if (ds_system.vertex_memory) {
        qvkFreeMemory(vk.device, ds_system.vertex_memory, NULL);
        ds_system.vertex_memory = VK_NULL_HANDLE;
    }

    if (ds_system.index_memory) {
        qvkFreeMemory(vk.device, ds_system.index_memory, NULL);
        ds_system.index_memory = VK_NULL_HANDLE;
    }
}

// Create graphics pipeline for decals
static void vk_create_decal_pipeline(void) {
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        // Combined image sampler for decal texture
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

    VkResult result = qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &ds_system.descriptor_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_pipeline: Failed to create descriptor set layout\n");
        return;
    }

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ds_system.descriptor_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    result = qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &ds_system.pipeline_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_pipeline: Failed to create pipeline layout\n");
        return;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_LEN(poolSizes),
        .pPoolSizes = poolSizes,
        .maxSets = 32
    };

    result = qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &ds_system.descriptor_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_pipeline: Failed to create descriptor pool\n");
        return;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ds_system.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ds_system.descriptor_layout
    };

    result = qvkAllocateDescriptorSets(vk.device, &allocInfo, &ds_system.descriptor_set);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_pipeline: Failed to allocate descriptor set\n");
        return;
    }

    // Create shader modules from embedded data
    VkShaderModule vertModule = SHADER_MODULE(decal_vert_spv);
    VkShaderModule fragModule = SHADER_MODULE(decal_frag_spv);

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

    // Vertex input
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
        .cullMode = VK_CULL_MODE_NONE,
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
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // Color blend
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
        .layout = ds_system.pipeline_layout,
        .renderPass = vk.renderPass,
        .subpass = 0
    };

    result = qvkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &ds_system.pipeline);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_decal_pipeline: Failed to create graphics pipeline\n");
    } else {
        SET_OBJECT_NAME(ds_system.pipeline, "decal_pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
    }

    // Cleanup shader modules
    qvkDestroyShaderModule(vk.device, vertModule, NULL);
    qvkDestroyShaderModule(vk.device, fragModule, NULL);
}

// Destroy graphics pipeline for decals
static void vk_destroy_decal_pipeline(void) {
    if (ds_system.pipeline) {
        qvkDestroyPipeline(vk.device, ds_system.pipeline, NULL);
        ds_system.pipeline = VK_NULL_HANDLE;
    }

    if (ds_system.pipeline_layout) {
        qvkDestroyPipelineLayout(vk.device, ds_system.pipeline_layout, NULL);
        ds_system.pipeline_layout = VK_NULL_HANDLE;
    }

    if (ds_system.descriptor_layout) {
        qvkDestroyDescriptorSetLayout(vk.device, ds_system.descriptor_layout, NULL);
        ds_system.descriptor_layout = VK_NULL_HANDLE;
    }

    if (ds_system.descriptor_pool) {
        qvkDestroyDescriptorPool(vk.device, ds_system.descriptor_pool, NULL);
        ds_system.descriptor_pool = VK_NULL_HANDLE;
    }
}

// Update descriptor sets for decals
static void vk_update_decal_descriptors(void) {
    // Default texture (white)
    VkDescriptorImageInfo imageInfo = {
        .sampler = vk.samplers[0],
        .imageView = vk.white_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds_system.descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    };

    qvkUpdateDescriptorSets(vk.device, 1, &write, 0, NULL);
}

// Build geometry for a decal
static void vk_build_decal_geometry(decal_t *decal) {
    // Simple quad geometry for now - can be expanded for more complex shapes
    vec4_t *vertices = &ds_system.vertices[decal->vertex_offset];
    uint32_t *indices = &ds_system.indices[decal->index_offset];

    // Calculate quad vertices in local space
    float half_size = decal->radius * 0.5f;

    // Bottom-left
    vertices[0][0] = -half_size;
    vertices[0][1] = -half_size;
    vertices[0][2] = 0.0f;
    vertices[0][3] = 1.0f;

    // Bottom-right
    vertices[1][0] = half_size;
    vertices[1][1] = -half_size;
    vertices[1][2] = 0.0f;
    vertices[1][3] = 1.0f;

    // Top-right
    vertices[2][0] = half_size;
    vertices[2][1] = half_size;
    vertices[2][2] = 0.0f;
    vertices[2][3] = 1.0f;

    // Top-left
    vertices[3][0] = -half_size;
    vertices[3][1] = half_size;
    vertices[3][2] = 0.0f;
    vertices[3][3] = 1.0f;

    // Indices
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    indices[3] = 0; indices[4] = 2; indices[5] = 3;

    decal->vertex_count = 4;
    decal->index_count = 6;
}

// Update vertex buffer with current decal data
static void vk_update_decal_vertex_buffer(void) {
    if (!ds_system.initialized) {
        return;
    }

    // For now, just rebuild all active decals
    // In a more optimized version, we'd only update changed decals

    ds_system.vertex_count = 0;
    ds_system.index_count = 0;

    for (int i = 0; i < MAX_DECALS; i++) {
        decal_t *decal = &ds_system.decals[i];

        if (!decal->active) {
            continue;
        }

        decal->vertex_offset = ds_system.vertex_count;
        decal->index_offset = ds_system.index_count;

        vk_build_decal_geometry(decal);

        ds_system.vertex_count += decal->vertex_count;
        ds_system.index_count += decal->index_count;
    }

    if (ds_system.vertex_count == 0) {
        return;
    }

    // Upload vertex data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    create_staging_buffer(ds_system.vertex_count * sizeof(vec4_t), &stagingBuffer, &stagingMemory);

    void *data;
    qvkMapMemory(vk.device, stagingMemory, 0, ds_system.vertex_count * sizeof(vec4_t), 0, &data);
    memcpy(data, ds_system.vertices, ds_system.vertex_count * sizeof(vec4_t));
    qvkUnmapMemory(vk.device, stagingMemory);

    VkCommandBuffer cmdBuf = begin_command_buffer();

    VkBufferCopy copyRegion = {
        .size = ds_system.vertex_count * sizeof(vec4_t)
    };

    qvkCmdCopyBuffer(cmdBuf, stagingBuffer, ds_system.vertex_buffer, 1, &copyRegion);

    // Upload index data
    create_staging_buffer(ds_system.index_count * sizeof(uint32_t), &stagingBuffer, &stagingMemory);

    qvkMapMemory(vk.device, stagingMemory, 0, ds_system.index_count * sizeof(uint32_t), 0, &data);
    memcpy(data, ds_system.indices, ds_system.index_count * sizeof(uint32_t));
    qvkUnmapMemory(vk.device, stagingMemory);

    copyRegion.size = ds_system.index_count * sizeof(uint32_t);
    qvkCmdCopyBuffer(cmdBuf, stagingBuffer, ds_system.index_buffer, 1, &copyRegion);

    end_command_buffer(cmdBuf);

    // Cleanup staging buffer
    qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
    qvkFreeMemory(vk.device, stagingMemory, NULL);
}

// Find a free decal slot
static int vk_find_free_decal_slot(void) {
    // Simple linear search for now - can be optimized with free list
    for (int i = 0; i < MAX_DECALS; i++) {
        if (!ds_system.decals[i].active) {
            return i;
        }
    }
    return -1;
}

#endif // USE_VULKAN