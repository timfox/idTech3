/*
=============================================================================
Volumetric Fog System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk_volumetric_fog.h"
#include "vk_utils.h"
#include "vk_images.h"
#include "vk_pipeline.h"
#include "vk.h"

// Embedded shader data
extern const unsigned char volumetric_fog_comp_spv[];

#ifdef USE_VULKAN

// CVars
cvar_t *r_volumetricFog;
cvar_t *r_volumetricFogDensity;
cvar_t *r_volumetricFogHeight;
cvar_t *r_volumetricFogFalloff;
cvar_t *r_volumetricFogSamples;
cvar_t *r_volumetricFogScattering;
cvar_t *r_volumetricFogAbsorption;

// Global system state
static volumetric_fog_system_t vf_system;

// Forward declarations
static qboolean vk_create_volumetric_fog_resources(void);
static void vk_destroy_volumetric_fog_resources(void);
static void vk_create_volumetric_fog_pipeline(void);
static void vk_destroy_volumetric_fog_pipeline(void);
static void vk_generate_noise_texture(void);
static void vk_update_volumetric_fog_descriptors(void);

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name((uint64_t)(obj), (objName), (objType))

// Vulkan function pointers
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
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Initialize volumetric fog system
void vk_volumetric_fog_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing volumetric fog system\n");

    memset(&vf_system, 0, sizeof(vf_system));

    // Register CVars
    r_volumetricFog = ri.Cvar_Get("r_volumetricFog", "1", CVAR_ARCHIVE);
    r_volumetricFogDensity = ri.Cvar_Get("r_volumetricFogDensity", "0.01", CVAR_ARCHIVE);
    r_volumetricFogHeight = ri.Cvar_Get("r_volumetricFogHeight", "0.0", CVAR_ARCHIVE);
    r_volumetricFogFalloff = ri.Cvar_Get("r_volumetricFogFalloff", "0.001", CVAR_ARCHIVE);
    r_volumetricFogSamples = ri.Cvar_Get("r_volumetricFogSamples", "64", CVAR_ARCHIVE);
    r_volumetricFogScattering = ri.Cvar_Get("r_volumetricFogScattering", "0.1", CVAR_ARCHIVE);
    r_volumetricFogAbsorption = ri.Cvar_Get("r_volumetricFogAbsorption", "0.05", CVAR_ARCHIVE);

    // Set default parameters
    vf_system.params.density = r_volumetricFogDensity->value;
    vf_system.params.height = r_volumetricFogHeight->value;
    vf_system.params.falloff = r_volumetricFogFalloff->value;
    vf_system.params.scattering = r_volumetricFogScattering->value;
    vf_system.params.absorption = r_volumetricFogAbsorption->value;
    vf_system.params.numSamples = r_volumetricFogSamples->integer;
    vf_system.params.noiseScale = 0.1f;
    vf_system.params.noiseSpeed = 0.1f;
    vf_system.params.lightIntensity = 1.0f;
    VectorSet(vf_system.params.lightColor, 1.0f, 1.0f, 1.0f);
    vf_system.params.enabled = r_volumetricFog->integer != 0;
    vf_system.params.heightFog = qtrue;
    vf_system.params.animated = qtrue;

    vf_system.noiseSize = 64; // 64x64x64 noise texture

    // Create resources
    if (!vk_create_volumetric_fog_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create volumetric fog resources\n");
        return;
    }

    // Create pipeline
    vk_create_volumetric_fog_pipeline();

    // Generate noise texture
    vk_generate_noise_texture();

    // Update descriptors
    vk_update_volumetric_fog_descriptors();

    vf_system.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan: Volumetric fog system initialized\n");
}

// Shutdown volumetric fog system
void vk_volumetric_fog_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down volumetric fog system\n");

    vk_destroy_volumetric_fog_pipeline();
    vk_destroy_volumetric_fog_resources();

    if (vf_system.noiseData) {
        ri.Free(vf_system.noiseData);
        vf_system.noiseData = NULL;
    }

    vf_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Volumetric fog system shut down\n");
}

// Update volumetric fog parameters
void vk_volumetric_fog_update(void) {
    if (!vf_system.initialized || !vf_system.enabled) {
        return;
    }

    // Update parameters from CVars
    vf_system.params.density = r_volumetricFogDensity->value;
    vf_system.params.height = r_volumetricFogHeight->value;
    vf_system.params.falloff = r_volumetricFogFalloff->value;
    vf_system.params.scattering = r_volumetricFogScattering->value;
    vf_system.params.absorption = r_volumetricFogAbsorption->value;
    vf_system.params.numSamples = r_volumetricFogSamples->integer;
    vf_system.params.enabled = r_volumetricFog->integer != 0;
}

// Render volumetric fog
void vk_volumetric_fog_render(void) {
    if (!vf_system.initialized || !vf_system.enabled) {
        return;
    }

    // Ensure we have valid dimensions
    if (vk.renderWidth == 0 || vk.renderHeight == 0) {
        return;
    }

    // Bind pipeline
    qvkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vf_system.pipeline);

    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            vf_system.pipelineLayout, 0, 1, &vf_system.descriptorSet, 0, NULL);

    // Push constants
    struct {
        matrix_t projectionMatrix;
        matrix_t viewMatrix;
        matrix_t invProjectionMatrix;
        matrix_t invViewMatrix;
        vec2_t resolution;
        vec2_t invResolution;
        vec3_t cameraPos;
        vec3_t lightDir;
        vec3_t lightColor;
        float lightIntensity;
        float fogDensity;
        float fogHeight;
        float fogFalloff;
        int numSamples;
        float scattering;
        float absorption;
        float time;
    } pushConstants;

    // Set up matrices
    MatrixCopy(vk.projection_matrix, pushConstants.projectionMatrix);
    MatrixCopy(vk.view_matrix, pushConstants.viewMatrix);
    MatrixInverse(pushConstants.projectionMatrix, pushConstants.invProjectionMatrix);
    MatrixInverse(pushConstants.viewMatrix, pushConstants.invViewMatrix);

    pushConstants.resolution[0] = (float)vk.renderWidth;
    pushConstants.resolution[1] = (float)vk.renderHeight;
    pushConstants.invResolution[0] = 1.0f / vk.renderWidth;
    pushConstants.invResolution[1] = 1.0f / vk.renderHeight;

    VectorCopy(vk.refdef.vieworg, pushConstants.cameraPos);

    // Get light direction (simplified - use main directional light)
    VectorSet(pushConstants.lightDir, 0.0f, -1.0f, 0.5f);
    VectorNormalize(pushConstants.lightDir);

    VectorCopy(vf_system.params.lightColor, pushConstants.lightColor);
    pushConstants.lightIntensity = vf_system.params.lightIntensity;
    pushConstants.fogDensity = vf_system.params.density;
    pushConstants.fogHeight = vf_system.params.height;
    pushConstants.fogFalloff = vf_system.params.falloff;
    pushConstants.numSamples = vf_system.params.numSamples;
    pushConstants.scattering = vf_system.params.scattering;
    pushConstants.absorption = vf_system.params.absorption;
    pushConstants.time = vk.refdef.floatTime;

    qvkCmdPushConstants(vk.command_buffer, vf_system.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);

    // Dispatch compute shader
    uint32_t groupCountX = (vk.renderWidth + 7) / 8;
    uint32_t groupCountY = (vk.renderHeight + 7) / 8;
    qvkCmdDispatch(vk.command_buffer, groupCountX, groupCountY, 1);
}

// Set volumetric fog parameters
void vk_volumetric_fog_set_params(const volumetric_fog_params_t *params) {
    if (!params) return;

    memcpy(&vf_system.params, params, sizeof(volumetric_fog_params_t));
}

// Get volumetric fog parameters
void vk_volumetric_fog_get_params(volumetric_fog_params_t *params) {
    if (!params) return;

    memcpy(params, &vf_system.params, sizeof(volumetric_fog_params_t));
}

// Create Vulkan resources for volumetric fog
static qboolean vk_create_volumetric_fog_resources(void) {
    VkResult result;

    // Create fog image
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .extent = { vk.renderWidth, vk.renderHeight, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    result = qvkCreateImage(vk.device, &imageInfo, NULL, &vf_system.fogImage);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to create fog image\n");
        return qfalse;
    }

    SET_OBJECT_NAME(vf_system.fogImage, "volumetric_fog_image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);

    // Allocate memory for fog image
    VkMemoryRequirements memReqs;
    qvkGetImageMemoryRequirements(vk.device, vf_system.fogImage, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &vf_system.fogImageMemory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to allocate fog image memory\n");
        return qfalse;
    }

    qvkBindImageMemory(vk.device, vf_system.fogImage, vf_system.fogImageMemory, 0);

    // Create fog image view
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vf_system.fogImage,
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

    result = qvkCreateImageView(vk.device, &viewInfo, NULL, &vf_system.fogImageView);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to create fog image view\n");
        return qfalse;
    }

    // Create noise texture (3D)
    VkImageCreateInfo noiseImageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_3D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = { (uint32_t)vf_system.noiseSize, (uint32_t)vf_system.noiseSize, (uint32_t)vf_system.noiseSize },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    result = qvkCreateImage(vk.device, &noiseImageInfo, NULL, &vf_system.noiseTexture);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to create noise texture\n");
        return qfalse;
    }

    SET_OBJECT_NAME(vf_system.noiseTexture, "volumetric_fog_noise_texture", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);

    // Allocate memory for noise texture
    qvkGetImageMemoryRequirements(vk.device, vf_system.noiseTexture, &memReqs);

    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = qvkAllocateMemory(vk.device, &allocInfo, NULL, &vf_system.noiseTextureMemory);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to allocate noise texture memory\n");
        return qfalse;
    }

    qvkBindImageMemory(vk.device, vf_system.noiseTexture, vf_system.noiseTextureMemory, 0);

    // Create noise texture view
    VkImageViewCreateInfo noiseViewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vf_system.noiseTexture,
        .viewType = VK_IMAGE_VIEW_TYPE_3D,
        .format = VK_FORMAT_R8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    result = qvkCreateImageView(vk.device, &noiseViewInfo, NULL, &vf_system.noiseTextureView);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to create noise texture view\n");
        return qfalse;
    }

    // Create samplers
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    result = qvkCreateSampler(vk.device, &samplerInfo, NULL, &vf_system.fogSampler);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to create fog sampler\n");
        return qfalse;
    }

    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT; // For 3D texture
    result = qvkCreateSampler(vk.device, &samplerInfo, NULL, &vf_system.noiseSampler);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_resources: Failed to create noise sampler\n");
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Volumetric fog resources created successfully\n");
    return qtrue;
}

// Destroy Vulkan resources for volumetric fog
static void vk_destroy_volumetric_fog_resources(void) {
    if (vf_system.fogSampler) {
        qvkDestroySampler(vk.device, vf_system.fogSampler, NULL);
        vf_system.fogSampler = VK_NULL_HANDLE;
    }

    if (vf_system.noiseSampler) {
        qvkDestroySampler(vk.device, vf_system.noiseSampler, NULL);
        vf_system.noiseSampler = VK_NULL_HANDLE;
    }

    if (vf_system.fogImageView) {
        qvkDestroyImageView(vk.device, vf_system.fogImageView, NULL);
        vf_system.fogImageView = VK_NULL_HANDLE;
    }

    if (vf_system.noiseTextureView) {
        qvkDestroyImageView(vk.device, vf_system.noiseTextureView, NULL);
        vf_system.noiseTextureView = VK_NULL_HANDLE;
    }

    if (vf_system.fogImage) {
        qvkDestroyImage(vk.device, vf_system.fogImage, NULL);
        vf_system.fogImage = VK_NULL_HANDLE;
    }

    if (vf_system.noiseTexture) {
        qvkDestroyImage(vk.device, vf_system.noiseTexture, NULL);
        vf_system.noiseTexture = VK_NULL_HANDLE;
    }

    if (vf_system.fogImageMemory) {
        qvkFreeMemory(vk.device, vf_system.fogImageMemory, NULL);
        vf_system.fogImageMemory = VK_NULL_HANDLE;
    }

    if (vf_system.noiseTextureMemory) {
        qvkFreeMemory(vk.device, vf_system.noiseTextureMemory, NULL);
        vf_system.noiseTextureMemory = VK_NULL_HANDLE;
    }
}

// Create compute pipeline for volumetric fog
static void vk_create_volumetric_fog_pipeline(void) {
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        // Depth buffer
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        },
        // Noise texture
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        },
        // Output image
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

    VkResult result = qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vf_system.descriptorSetLayout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_pipeline: Failed to create descriptor set layout\n");
        return;
    }

    // Push constant range
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(matrix_t) * 4 + sizeof(vec2_t) * 2 + sizeof(vec3_t) * 3 + sizeof(float) * 7 + sizeof(int)
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vf_system.descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    result = qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vf_system.pipelineLayout);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_pipeline: Failed to create pipeline layout\n");
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

    result = qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &vf_system.descriptorPool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_pipeline: Failed to create descriptor pool\n");
        return;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vf_system.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vf_system.descriptorSetLayout
    };

    result = qvkAllocateDescriptorSets(vk.device, &allocInfo, &vf_system.descriptorSet);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_pipeline: Failed to allocate descriptor set\n");
        return;
    }

    // Create shader module from embedded data
    extern const unsigned char volumetric_fog_comp_spv[];
    VkShaderModule shaderModule = SHADER_MODULE(volumetric_fog_comp_spv);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_pipeline: Failed to create shader module\n");
        return;
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName = "main"
        },
        .layout = vf_system.pipelineLayout
    };

    result = qvkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &vf_system.pipeline);
    qvkDestroyShaderModule(vk.device, shaderModule, NULL);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_create_volumetric_fog_pipeline: Failed to create compute pipeline\n");
        return;
    }

    SET_OBJECT_NAME(vf_system.pipeline, "volumetric_fog_pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
}

// Destroy compute pipeline for volumetric fog
static void vk_destroy_volumetric_fog_pipeline(void) {
    if (vf_system.pipeline) {
        qvkDestroyPipeline(vk.device, vf_system.pipeline, NULL);
        vf_system.pipeline = VK_NULL_HANDLE;
    }

    if (vf_system.pipelineLayout) {
        qvkDestroyPipelineLayout(vk.device, vf_system.pipelineLayout, NULL);
        vf_system.pipelineLayout = VK_NULL_HANDLE;
    }

    if (vf_system.descriptorSetLayout) {
        qvkDestroyDescriptorSetLayout(vk.device, vf_system.descriptorSetLayout, NULL);
        vf_system.descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (vf_system.descriptorPool) {
        qvkDestroyDescriptorPool(vk.device, vf_system.descriptorPool, NULL);
        vf_system.descriptorPool = VK_NULL_HANDLE;
    }
}

// Generate 3D noise texture for volumetric fog
static void vk_generate_noise_texture(void) {
    int size = vf_system.noiseSize;
    int totalSize = size * size * size;

    vf_system.noiseData = ri.Malloc(totalSize);

    // Generate 3D Perlin-like noise
    for (int z = 0; z < size; z++) {
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                // Simple noise function - could be improved with proper Perlin noise
                float nx = (float)x / size;
                float ny = (float)y / size;
                float nz = (float)z / size;

                // Combine multiple octaves
                float noise = 0.0f;
                float amplitude = 1.0f;
                float frequency = 1.0f;

                for (int octave = 0; octave < 4; octave++) {
                    noise += sinf(nx * frequency * 6.28f) * cosf(ny * frequency * 6.28f) * sinf(nz * frequency * 6.28f) * amplitude;
                    amplitude *= 0.5f;
                    frequency *= 2.0f;
                }

                // Normalize to 0-1 range
                noise = (noise + 1.0f) * 0.5f;
                vf_system.noiseData[z * size * size + y * size + x] = (uint8_t)(noise * 255.0f);
            }
        }
    }

    // Upload to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    create_staging_buffer(totalSize, &stagingBuffer, &stagingMemory);

    void *data;
    qvkMapMemory(vk.device, stagingMemory, 0, totalSize, 0, &data);
    memcpy(data, vf_system.noiseData, totalSize);
    qvkUnmapMemory(vk.device, stagingMemory);

    // Copy to image
    VkCommandBuffer cmdBuf = begin_command_buffer();

    // Transition noise texture to transfer dst
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vf_system.noiseTexture,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageExtent = { (uint32_t)size, (uint32_t)size, (uint32_t)size }
    };

    qvkCmdCopyBufferToImage(cmdBuf, stagingBuffer, vf_system.noiseTexture,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);

    end_command_buffer(cmdBuf);

    // Cleanup staging buffer
    qvkDestroyBuffer(vk.device, stagingBuffer, NULL);
    qvkFreeMemory(vk.device, stagingMemory, NULL);
}

// Update descriptor sets for volumetric fog
static void vk_update_volumetric_fog_descriptors(void) {
    VkDescriptorImageInfo depthInfo = {
        .sampler = vk.depth_sampler,
        .imageView = vk.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo noiseInfo = {
        .sampler = vf_system.noiseSampler,
        .imageView = vf_system.noiseTextureView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = vf_system.fogImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vf_system.descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &depthInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vf_system.descriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &noiseInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vf_system.descriptorSet,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputInfo
        }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);
}

#endif // USE_VULKAN