/*
=============================================================================
God Rays/Light Shafts System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk_god_rays.h"
#include "vk_utils.h"
#include "vk_pipeline.h"
#include "vk.h"
#include <string.h>

#ifdef USE_VULKAN

// CVars
cvar_t *r_godRays;
cvar_t *r_godRaysDensity;
cvar_t *r_godRaysWeight;
cvar_t *r_godRaysDecay;
cvar_t *r_godRaysExposure;
cvar_t *r_godRaysSamples;

// Global system state
static god_rays_system_t gr_system;

// Forward declarations
static qboolean vk_create_god_rays_resources(void);
static void vk_destroy_god_rays_resources(void);
static void vk_create_god_rays_pipeline(void);
static void vk_destroy_god_rays_pipeline(void);
static void vk_create_light_detect_pipeline(void);
static void vk_destroy_light_detect_pipeline(void);
static void vk_update_god_rays_descriptors(void);
static void vk_detect_bright_lights(void);
static void vk_project_lights_to_screen(void);

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);

// Vulkan function pointers
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
extern PFN_vkCreateComputePipelines qvkCreateComputePipelines;
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
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdDispatch qvkCmdDispatch;
extern PFN_vkCmdDraw qvkCmdDraw;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Initialize god rays system
void vk_god_rays_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing god rays system\n");

    memset(&gr_system, 0, sizeof(gr_system));

    // Register CVars
    // Off by default until the compute path is fully validated.
    r_godRays = ri.Cvar_Get("r_godRays", "0", CVAR_ARCHIVE);
    r_godRaysDensity = ri.Cvar_Get("r_godRaysDensity", "0.5", CVAR_ARCHIVE);
    r_godRaysWeight = ri.Cvar_Get("r_godRaysWeight", "0.1", CVAR_ARCHIVE);
    r_godRaysDecay = ri.Cvar_Get("r_godRaysDecay", "0.95", CVAR_ARCHIVE);
    r_godRaysExposure = ri.Cvar_Get("r_godRaysExposure", "0.1", CVAR_ARCHIVE);
    r_godRaysSamples = ri.Cvar_Get("r_godRaysSamples", "64", CVAR_ARCHIVE);

    if (!r_godRays->integer) {
        return;
    }

    // Set default parameters
    gr_system.params.density = r_godRaysDensity->value;
    gr_system.params.weight = r_godRaysWeight->value;
    gr_system.params.decay = r_godRaysDecay->value;
    gr_system.params.exposure = r_godRaysExposure->value;
    gr_system.params.num_samples = r_godRaysSamples->integer;
    gr_system.params.max_distance = 1.0f;
    gr_system.params.enabled = r_godRays->integer != 0;
    gr_system.params.auto_lights = qtrue;

    // Initialize sun
    VectorSet(gr_system.sun_position, 1000.0f, 1000.0f, 1000.0f);
    VectorSet(gr_system.sun_color, 1.0f, 0.9f, 0.8f);
    gr_system.sun_intensity = 1.0f;

    // Create Vulkan resources
    if (!vk_create_god_rays_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create god rays resources\n");
        return;
    }

    // Create pipelines
    vk_create_god_rays_pipeline();
    vk_create_light_detect_pipeline();

    // Update descriptors
    vk_update_god_rays_descriptors();

    gr_system.initialized = qtrue;
    gr_system.enabled = qtrue;

    ri.Printf(PRINT_ALL, "Vulkan: God rays system initialized\n");
}

// Shutdown god rays system
void vk_god_rays_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down god rays system\n");

    vk_destroy_light_detect_pipeline();
    vk_destroy_god_rays_pipeline();
    vk_destroy_god_rays_resources();

    gr_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: God rays system shut down\n");
}

// Update god rays system
void vk_god_rays_update(void) {
    if (!gr_system.initialized || !gr_system.enabled) {
        return;
    }

    // Update parameters from CVars
    gr_system.params.density = r_godRaysDensity->value;
    gr_system.params.weight = r_godRaysWeight->value;
    gr_system.params.decay = r_godRaysDecay->value;
    gr_system.params.exposure = r_godRaysExposure->value;
    gr_system.params.num_samples = r_godRaysSamples->integer;
    gr_system.params.enabled = r_godRays->integer != 0;

    // Auto-detect bright lights if enabled
    if (gr_system.params.auto_lights) {
        vk_detect_bright_lights();
    }

    // Project lights to screen space
    vk_project_lights_to_screen();
}

// Render god rays effect
void vk_god_rays_render(VkCommandBuffer cmd_buffer) {
    if (!gr_system.initialized || !gr_system.enabled || gr_system.num_lights == 0) {
        return;
    }

    // Bind pipeline
    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gr_system.pipeline);

    // Bind descriptor set
    qvkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            gr_system.pipeline_layout, 0, 1, &gr_system.descriptor_set, 0, NULL);

    // Push constants for each light
    for (int i = 0; i < gr_system.num_lights; i++) {
        god_rays_light_t *light = &gr_system.lights[i];

        if (!light->active) {
            continue;
        }

        struct {
            vec2_t light_pos;
            vec3_t light_color;
            float light_intensity;
            float density;
            float weight;
            float decay;
            float exposure;
            int num_samples;
        } pushConstants;

        VectorCopy2(light->screen_pos, pushConstants.light_pos);
        VectorCopy(light->color, pushConstants.light_color);
        pushConstants.light_intensity = light->intensity;
        pushConstants.density = gr_system.params.density;
        pushConstants.weight = gr_system.params.weight;
        pushConstants.decay = gr_system.params.decay;
        pushConstants.exposure = gr_system.params.exposure;
        pushConstants.num_samples = gr_system.params.num_samples;

        qvkCmdPushConstants(cmd_buffer, gr_system.pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);

        // Dispatch compute shader
        uint32_t groupCountX = (vk.renderWidth + 15) / 16;
        uint32_t groupCountY = (vk.renderHeight + 15) / 16;
        qvkCmdDispatch(cmd_buffer, groupCountX, groupCountY, 1);
    }
}

// Add a light source for god rays
void vk_god_rays_add_light(const vec3_t position, const vec3_t color, float intensity, float radius) {
    if (gr_system.num_lights >= MAX_LIGHT_SOURCES) {
        return;
    }

    god_rays_light_t *light = &gr_system.lights[gr_system.num_lights++];
    VectorCopy(position, light->position);
    VectorCopy(color, light->color);
    light->intensity = intensity;
    light->radius = radius;
    light->active = qtrue;
    light->sun_light = qfalse;
}

// Set sun/moon position
void vk_god_rays_set_sun(const vec3_t position, const vec3_t color, float intensity) {
    VectorCopy(position, gr_system.sun_position);
    VectorCopy(color, gr_system.sun_color);
    gr_system.sun_intensity = intensity;

    // Add sun as a special light source
    if (gr_system.num_lights < MAX_LIGHT_SOURCES) {
        god_rays_light_t *light = &gr_system.lights[gr_system.num_lights++];
        VectorCopy(position, light->position);
        VectorCopy(color, light->color);
        light->intensity = intensity;
        light->radius = 0.1f; // Large sun radius
        light->active = qtrue;
        light->sun_light = qtrue;
    }
}

// Clear all light sources
void vk_god_rays_clear_lights(void) {
    for (int i = 0; i < MAX_LIGHT_SOURCES; i++) {
        gr_system.lights[i].active = qfalse;
    }
    gr_system.num_lights = 0;
}

// Set god rays parameters
void vk_god_rays_set_params(const god_rays_params_t *params) {
    if (!params) return;
    memcpy(&gr_system.params, params, sizeof(god_rays_params_t));
}

// Get god rays parameters
void vk_god_rays_get_params(god_rays_params_t *params) {
    if (!params) return;
    memcpy(params, &gr_system.params, sizeof(god_rays_params_t));
}

// Create Vulkan resources for god rays
static qboolean vk_create_god_rays_resources(void) {
    VkResult result;

    // Create rays image
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .extent = { vk.renderWidth, vk.renderHeight, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    result = qvkCreateImage(vk.device, &imageInfo, NULL, &gr_system.rays_image);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_resources: Failed to create rays image\n");
        return qfalse;
    }

    SET_OBJECT_NAME(gr_system.rays_image, "god_rays_image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);

    // Allocate memory
    VkMemoryRequirements memReqs;
    qvkGetImageMemoryRequirements(vk.device, gr_system.rays_image, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &gr_system.rays_image_memory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_resources: Failed to allocate rays image memory\n");
        return qfalse;
    }

    qvkBindImageMemory(vk.device, gr_system.rays_image, gr_system.rays_image_memory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = gr_system.rays_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    result = qvkCreateImageView(vk.device, &viewInfo, NULL, &gr_system.rays_image_view);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_resources: Failed to create rays image view\n");
        return qfalse;
    }

    // Create sampler
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    result = qvkCreateSampler(vk.device, &samplerInfo, NULL, &gr_system.rays_sampler);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_resources: Failed to create rays sampler\n");
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan: God rays resources created successfully\n");
    return qtrue;
}

// Destroy Vulkan resources for god rays
static void vk_destroy_god_rays_resources(void) {
    if (gr_system.rays_sampler) {
        qvkDestroySampler(vk.device, gr_system.rays_sampler, NULL);
        gr_system.rays_sampler = VK_NULL_HANDLE;
    }

    if (gr_system.rays_image_view) {
        qvkDestroyImageView(vk.device, gr_system.rays_image_view, NULL);
        gr_system.rays_image_view = VK_NULL_HANDLE;
    }

    if (gr_system.rays_image) {
        qvkDestroyImage(vk.device, gr_system.rays_image, NULL);
        gr_system.rays_image = VK_NULL_HANDLE;
    }

    if (gr_system.rays_image_memory) {
        qvkFreeMemory(vk.device, gr_system.rays_image_memory, NULL);
        gr_system.rays_image_memory = VK_NULL_HANDLE;
    }
}

// Create compute pipeline for god rays
static void vk_create_god_rays_pipeline(void) {
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        // HDR color buffer
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        },
        // Depth buffer
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        },
        // Output rays image
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_LEN(bindings),
        .pBindings = bindings
    };

    VkResult result = qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &gr_system.descriptor_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_pipeline: Failed to create descriptor set layout\n");
        return;
    }

    // Push constant range
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(vec2_t) + sizeof(vec3_t) + sizeof(float) * 5 + sizeof(int)
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &gr_system.descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    result = qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &gr_system.pipeline_layout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_pipeline: Failed to create pipeline layout\n");
        return;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_LEN(poolSizes),
        .pPoolSizes = poolSizes,
        .maxSets = 1
    };

    result = qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &gr_system.descriptor_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_pipeline: Failed to create descriptor pool\n");
        return;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gr_system.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &gr_system.descriptor_layout
    };

    result = qvkAllocateDescriptorSets(vk.device, &allocInfo, &gr_system.descriptor_set);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_pipeline: Failed to allocate descriptor set\n");
        return;
    }

    // Load shader
    extern const unsigned char god_rays_comp_spv[];
    VkShaderModule shaderModule = SHADER_MODULE(god_rays_comp_spv);

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName = "main"
        },
        .layout = gr_system.pipeline_layout
    };

    result = qvkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &gr_system.pipeline);
    qvkDestroyShaderModule(vk.device, shaderModule, NULL);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_god_rays_pipeline: Failed to create compute pipeline\n");
        return;
    }

    SET_OBJECT_NAME(gr_system.pipeline, "god_rays_pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
}

// Destroy compute pipeline for god rays
static void vk_destroy_god_rays_pipeline(void) {
    if (gr_system.pipeline) {
        qvkDestroyPipeline(vk.device, gr_system.pipeline, NULL);
        gr_system.pipeline = VK_NULL_HANDLE;
    }

    if (gr_system.pipeline_layout) {
        qvkDestroyPipelineLayout(vk.device, gr_system.pipeline_layout, NULL);
        gr_system.pipeline_layout = VK_NULL_HANDLE;
    }

    if (gr_system.descriptor_layout) {
        qvkDestroyDescriptorSetLayout(vk.device, gr_system.descriptor_layout, NULL);
        gr_system.descriptor_layout = VK_NULL_HANDLE;
    }

    if (gr_system.descriptor_pool) {
        qvkDestroyDescriptorPool(vk.device, gr_system.descriptor_pool, NULL);
        gr_system.descriptor_pool = VK_NULL_HANDLE;
    }
}

// Create light detection pipeline
static void vk_create_light_detect_pipeline(void) {
    // This would create a pipeline for automatically detecting bright lights
    // For now, we'll implement manual light detection
}

// Destroy light detection pipeline
static void vk_destroy_light_detect_pipeline(void) {
    // Cleanup light detection resources
}

// Update descriptor sets for god rays
static void vk_update_god_rays_descriptors(void) {
    VkDescriptorImageInfo colorInfo = {
        .sampler = vk.samplers[0],
        .imageView = vk.color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo depthInfo = {
        .sampler = vk.depth_sampler,
        .imageView = vk.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo raysInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = gr_system.rays_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = gr_system.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &colorInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = gr_system.descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &depthInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = gr_system.descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &raysInfo
        }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);
}

// Detect bright lights automatically
static void vk_detect_bright_lights(void) {
    // TODO: Implement light detection (requires access to view/projection transforms).
    vk_god_rays_clear_lights();
}

// Project lights to screen space
static void vk_project_lights_to_screen(void) {
    // TODO: Implement projection (requires access to view/projection transforms).
}

// Helper function to project sun to screen (simplified)
static qboolean project_sun_to_screen(const vec3_t world_pos, vec3_t screen_pos) {
    (void)world_pos;
    (void)screen_pos;
    return qfalse;
}

#endif // USE_VULKAN