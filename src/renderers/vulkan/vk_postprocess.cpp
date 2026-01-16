#include "tr_local.h"
#include "vk_postprocess.h"
#include "vk_renderpass.h"
#include "vk_utils.h"
#include "vk_images.h"
#include "vk.h"
// C++ helpers
#include <algorithm>

// Renderer interface
extern refimport_t ri;

// CVAR externs
extern cvar_t *r_bloom;
extern cvar_t *r_postQuality;
extern cvar_t *r_gamma;
extern cvar_t *r_greyscale;
extern cvar_t *r_dither;
extern "C" qboolean vk_fsr_is_enabled(void);
static cvar_t *r_vk_compute_post = NULL;

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
// Use SET_OBJECT_NAME from vk.h

// Vulkan function pointer extern declarations
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdDispatch qvkCmdDispatch;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Forward declarations
static void vk_create_bloom_extract_pipeline(void);
static void vk_create_blur_pipelines(void);
static void vk_update_bloom_descriptors(void);
static qboolean vk_create_tonemap_pipeline(void);
static qboolean vk_create_gamma_pipeline(void);
static qboolean vk_create_tonemap_resources(void);
static qboolean vk_create_gamma_resources(void);
extern "C" VkPipeline vk_create_compute_pipeline(VkShaderModule computeShader, VkPipelineLayout layout, const char *name);

typedef struct {
    int tonemapMode;
    float exposure;
} vk_tonemap_params_t;

typedef struct {
    float gamma;
    float obScale;
    float greyscale;
    int ditherMode;
    int depth[3];
    float styleStrength;
} vk_gamma_params_t;

static VkFormat vk_postprocess_color_format(void) {
    return VK_FORMAT_R8G8B8A8_UNORM;
}

// Initialize post-processing system
qboolean vk_init_post_processing(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing post-processing system\n");

    // Check if render dimensions are available
    if (vk.renderWidth == 0 || vk.renderHeight == 0) {
        ri.Printf(PRINT_WARNING, "Vulkan: Render dimensions not set yet, deferring post-processing resource creation\n");
        // Don't fail - resources will be created later when dimensions are known
        return qtrue;
    }

    if (vk.color_format == VK_FORMAT_UNDEFINED) {
        ri.Printf(PRINT_WARNING, "Vulkan: Color format not set yet, deferring post-processing resource creation\n");
        return qtrue;
    }

    // Create bloom resources if enabled
    if (r_bloom && r_bloom->integer) {
        if (!vk_create_bloom_resources()) {
            ri.Printf(PRINT_ERROR, "Vulkan: Failed to create bloom resources\n");
            return qfalse;
        }
        vk_create_bloom_extract_pipeline();
        vk_create_blur_pipelines();
        vk_update_bloom_descriptors();
    }

    ri.Printf(PRINT_ALL, "Vulkan: Post-processing system initialized\n");
    return qtrue;
}

// Shutdown post-processing system
extern "C" void vk_shutdown_post_processing(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down post-processing system\n");

    // Destroy bloom resources
    vk_destroy_bloom_resources();

    vk_destroy_image_resources(&vk.tonemap_image, &vk.tonemap_image_view);
    vk_destroy_image_resources(&vk.gamma_image, &vk.gamma_image_view);

    if (vk.tonemap_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.tonemap_pipeline, NULL);
        vk.tonemap_pipeline = VK_NULL_HANDLE;
    }
    if (vk.gamma_compute_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.gamma_compute_pipeline, NULL);
        vk.gamma_compute_pipeline = VK_NULL_HANDLE;
    }
    if (vk.tonemap_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.tonemap_layout, NULL);
        vk.tonemap_layout = VK_NULL_HANDLE;
    }
    if (vk.gamma_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.gamma_layout, NULL);
        vk.gamma_layout = VK_NULL_HANDLE;
    }
    if (vk.tonemap_descriptor_layout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk.tonemap_descriptor_layout, NULL);
        vk.tonemap_descriptor_layout = VK_NULL_HANDLE;
    }
    if (vk.gamma_descriptor_layout != VK_NULL_HANDLE) {
        qvkDestroyDescriptorSetLayout(vk.device, vk.gamma_descriptor_layout, NULL);
        vk.gamma_descriptor_layout = VK_NULL_HANDLE;
    }
    vk.tonemap_descriptor = VK_NULL_HANDLE;
    vk.gamma_descriptor = VK_NULL_HANDLE;
    vk.postprocess_output_image = VK_NULL_HANDLE;

    ri.Printf(PRINT_ALL, "Vulkan: Post-processing system shut down\n");
}

// Create bloom resources
qboolean vk_create_bloom_resources(void) {
    uint32_t width = vk.renderWidth;
    uint32_t height = vk.renderHeight;

    // Create bloom extract image
    if (!create_color_attachment(width, height, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                 &vk.bloom_image[0], &vk.bloom_image_view[0],
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qfalse, 0)) {
        ri.Printf(PRINT_ERROR, "vk_create_bloom_resources: Failed to create bloom extract image\n");
        return qfalse;
    }

    // Create blur images (ping-pong)
    for (int i = 0; i < VK_NUM_BLOOM_PASSES*2; i++) {
        if (!create_color_attachment(width, height, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                     &vk.bloom_image[1+i], &vk.bloom_image_view[1+i],
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qfalse, 0)) {
            ri.Printf(PRINT_ERROR, "vk_create_bloom_resources: Failed to create blur image %d\n", i);
            return qfalse;
        }
    }

    ri.Printf(PRINT_ALL, "Vulkan: Bloom resources created successfully\n");
    return qtrue;
}

static qboolean vk_create_tonemap_resources(void) {
    if (vk.tonemap_image != VK_NULL_HANDLE) {
        return qtrue;
    }
    VkFormat format = vk_postprocess_color_format();
    if (!create_color_attachment(vk.renderWidth, vk.renderHeight, VK_SAMPLE_COUNT_1_BIT, format,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                 &vk.tonemap_image, &vk.tonemap_image_view, VK_IMAGE_LAYOUT_GENERAL, qfalse, 0)) {
        ri.Printf(PRINT_ERROR, "vk_create_tonemap_resources: Failed to create tonemap image\n");
        return qfalse;
    }
    return qtrue;
}

static qboolean vk_create_gamma_resources(void) {
    if (vk.gamma_image != VK_NULL_HANDLE) {
        return qtrue;
    }
    VkFormat format = vk_postprocess_color_format();
    if (!create_color_attachment(vk.renderWidth, vk.renderHeight, VK_SAMPLE_COUNT_1_BIT, format,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                 &vk.gamma_image, &vk.gamma_image_view, VK_IMAGE_LAYOUT_GENERAL, qfalse, 0)) {
        ri.Printf(PRINT_ERROR, "vk_create_gamma_resources: Failed to create gamma image\n");
        return qfalse;
    }
    return qtrue;
}

static qboolean vk_create_tonemap_pipeline(void) {
    if (vk.tonemap_pipeline != VK_NULL_HANDLE) {
        return qtrue;
    }
    if (vk.modules.tonemap_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Tonemap compute shader not loaded\n");
        return qfalse;
    }

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.tonemap_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create tonemap descriptor set layout\n");
        return qfalse;
    }

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vk_tonemap_params_t) };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.tonemap_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.tonemap_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create tonemap pipeline layout\n");
        return qfalse;
    }

    vk.tonemap_pipeline = vk_create_compute_pipeline(vk.modules.tonemap_comp, vk.tonemap_layout, "Tonemap");
    return vk.tonemap_pipeline != VK_NULL_HANDLE;
}

static qboolean vk_create_gamma_pipeline(void) {
    if (vk.gamma_compute_pipeline != VK_NULL_HANDLE) {
        return qtrue;
    }
    if (vk.modules.gamma_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Gamma compute shader not loaded\n");
        return qfalse;
    }

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.gamma_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create gamma descriptor set layout\n");
        return qfalse;
    }

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vk_gamma_params_t) };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.gamma_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.gamma_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create gamma pipeline layout\n");
        return qfalse;
    }

    vk.gamma_compute_pipeline = vk_create_compute_pipeline(vk.modules.gamma_comp, vk.gamma_layout, "Gamma");
    return vk.gamma_compute_pipeline != VK_NULL_HANDLE;
}

// Destroy bloom resources
extern "C" void vk_destroy_bloom_resources(void) {
    // Destroy all bloom images
    for (int i = 0; i < 1+VK_NUM_BLOOM_PASSES*2; i++) {
        vk_destroy_image_resources(&vk.bloom_image[i], &vk.bloom_image_view[i]);
    }

    // Destroy pipelines
    if (vk.bloom_extract_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.bloom_extract_pipeline, NULL);
        vk.bloom_extract_pipeline = VK_NULL_HANDLE;
    }

    for (int i = 0; i < VK_NUM_BLOOM_PASSES*2; i++) {
        if (vk.blur_pipeline[i] != VK_NULL_HANDLE) {
            qvkDestroyPipeline(vk.device, vk.blur_pipeline[i], NULL);
            vk.blur_pipeline[i] = VK_NULL_HANDLE;
        }
    }

    ri.Printf(PRINT_ALL, "Vulkan: Bloom resources destroyed\n");
}

// Create bloom extract pipeline
static void vk_create_bloom_extract_pipeline(void) {
    // This would create a graphics pipeline for bloom extraction
    // Implementation depends on specific shader requirements
    ri.Printf(PRINT_ALL, "Vulkan: Bloom extract pipeline creation (stub)\n");
}

// Create blur pipelines
static void vk_create_blur_pipelines(void) {
    // Create horizontal and vertical blur pipelines
    for (int i = 0; i < VK_NUM_BLOOM_PASSES*2; i++) {
        qboolean horizontal = ((i % 2 == 0) ? qtrue : qfalse);
        // Implementation would create compute or graphics pipelines for blurring
        ri.Printf(PRINT_ALL, "Vulkan: Blur pipeline %d creation (stub, horizontal=%d)\n", i, horizontal);
    }
}

// Update bloom descriptors
static void vk_update_bloom_descriptors(void) {
    // Update descriptor sets for bloom processing
    ri.Printf(PRINT_ALL, "Vulkan: Bloom descriptors update (stub)\n");
}

// Apply bloom effect
extern "C" void vk_apply_bloom(void) {
    if (!r_bloom || !r_bloom->integer) {
        return;
    }

    // Extract bright areas
    vk_begin_bloom_extract_render_pass();
    // Render bright pixels to bloom extract image
    vk_end_render_pass();

    // Apply blur passes (multiple iterations for better quality)
    int blur_iterations = r_postQuality ? MAX(1, r_postQuality->integer) : 2;

    for (int iteration = 0; iteration < blur_iterations; iteration++) {
        // Horizontal blur
        vk_begin_blur_render_pass(0);
        // Bind horizontal blur pipeline and dispatch
        vk_end_render_pass();

        // Vertical blur
        vk_begin_blur_render_pass(1);
        // Bind vertical blur pipeline and dispatch
        vk_end_render_pass();
    }
}

// Create blur pipeline (compute shader based)
extern "C" void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass) {
    if (!vk_bounds_check(index, VK_NUM_BLOOM_PASSES*2, "blur pipeline")) {
        return;
    }

    // Create compute pipeline for blur effect
    // This is a placeholder - actual implementation would create the pipeline
    ri.Printf(PRINT_ALL, "Vulkan: Creating blur pipeline %u (%s, %ux%u)\n",
        index, horizontal_pass ? "horizontal" : "vertical", width, height);
}

// Update post-processing pipelines
extern "C" void vk_update_post_process_pipelines(void) {
    // Update pipelines based on current settings
    if (r_bloom && r_bloom->integer) {
        vk_update_bloom_descriptors();
    }

    ri.Printf(PRINT_ALL, "Vulkan: Post-processing pipelines updated\n");
}

// Apply tone mapping
extern "C" void vk_apply_tone_mapping(void) {
    static qboolean tonemap_initialized = qfalse;
    if (!vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE) {
        return;
    }
    if (!r_vk_compute_post) {
        r_vk_compute_post = ri.Cvar_Get("r_vk_compute_post", "0", CVAR_ARCHIVE);
        ri.Cvar_SetDescription(r_vk_compute_post, "Enable compute tonemap/gamma (experimental)");
    }
    if (!r_vk_compute_post->integer) {
        return;
    }
    if (vk_fsr_is_enabled()) {
        return;
    }
    if (!vk_create_tonemap_pipeline() || !vk_create_tonemap_resources()) {
        return;
    }

    if (vk.tonemap_descriptor == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = vk.descriptor_pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &vk.tonemap_descriptor_layout;
        if (qvkAllocateDescriptorSets(vk.device, &alloc, &vk.tonemap_descriptor) != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_apply_tone_mapping: Failed to allocate descriptor set\n");
            return;
        }
    }

    Vk_Sampler_Def sd{};
    sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
    sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sd.max_lod_1_0 = qtrue;
    sd.noAnisotropy = qtrue;

    VkDescriptorImageInfo input_info{};
    input_info.sampler = vk_find_sampler(&sd);
    input_info.imageView = vk.color_image_view;
    input_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo output_info{};
    output_info.sampler = VK_NULL_HANDLE;
    output_info.imageView = vk.tonemap_image_view;
    output_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk.tonemap_descriptor;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &input_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk.tonemap_descriptor;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &output_info;

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    VkImageMemoryBarrier tonemap_barrier{};
    tonemap_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    tonemap_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    tonemap_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    tonemap_barrier.image = vk.tonemap_image;
    tonemap_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    tonemap_barrier.oldLayout = tonemap_initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    tonemap_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    tonemap_barrier.srcAccessMask = tonemap_initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    tonemap_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        tonemap_initialized ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &tonemap_barrier);

    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.tonemap_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.tonemap_layout,
                             0, 1, &vk.tonemap_descriptor, 0, NULL);

    vk_tonemap_params_t params{};
    params.tonemapMode = 1;
    params.exposure = 1.0f;
    qvkCmdPushConstants(vk.cmd->command_buffer, vk.tonemap_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(params), &params);

    uint32_t groups_x = (vk.renderWidth + 7) / 8;
    uint32_t groups_y = (vk.renderHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groups_x, groups_y, 1);

    tonemap_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    tonemap_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    tonemap_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    tonemap_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &tonemap_barrier);

    tonemap_initialized = qtrue;
}

// Apply gamma correction
extern "C" void vk_apply_gamma_correction(void) {
    static qboolean gamma_initialized = qfalse;
    if (!vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE) {
        return;
    }
    if (!r_vk_compute_post || !r_vk_compute_post->integer) {
        return;
    }
    if (!vk_create_gamma_pipeline() || !vk_create_gamma_resources()) {
        return;
    }

    if (vk.gamma_descriptor == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = vk.descriptor_pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &vk.gamma_descriptor_layout;
        if (qvkAllocateDescriptorSets(vk.device, &alloc, &vk.gamma_descriptor) != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_apply_gamma_correction: Failed to allocate descriptor set\n");
            return;
        }
    }

    Vk_Sampler_Def sd{};
    sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
    sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sd.max_lod_1_0 = qtrue;
    sd.noAnisotropy = qtrue;

    VkDescriptorImageInfo input_info{};
    input_info.sampler = vk_find_sampler(&sd);
    input_info.imageView = vk.tonemap_image_view;
    input_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo output_info{};
    output_info.sampler = VK_NULL_HANDLE;
    output_info.imageView = vk.gamma_image_view;
    output_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk.gamma_descriptor;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &input_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk.gamma_descriptor;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &output_info;

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    VkImageMemoryBarrier gamma_barrier{};
    gamma_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    gamma_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    gamma_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    gamma_barrier.image = vk.gamma_image;
    gamma_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    gamma_barrier.oldLayout = gamma_initialized ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    gamma_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    gamma_barrier.srcAccessMask = gamma_initialized ? VK_ACCESS_TRANSFER_READ_BIT : 0;
    gamma_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        gamma_initialized ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &gamma_barrier);

    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.gamma_compute_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.gamma_layout,
                             0, 1, &vk.gamma_descriptor, 0, NULL);

    vk_gamma_params_t params{};
    params.gamma = (r_gamma && r_gamma->value != 0.0f) ? (1.0f / r_gamma->value) : 1.0f;
    params.obScale = (float)(1 << tr.overbrightBits);
    params.greyscale = r_greyscale ? r_greyscale->value : 0.0f;
    params.ditherMode = r_dither ? r_dither->integer : 0;
    params.depth[0] = 255;
    params.depth[1] = 255;
    params.depth[2] = 255;
    params.styleStrength = 0.0f;
    qvkCmdPushConstants(vk.cmd->command_buffer, vk.gamma_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(params), &params);

    uint32_t groups_x = (vk.renderWidth + 7) / 8;
    uint32_t groups_y = (vk.renderHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groups_x, groups_y, 1);

    gamma_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    gamma_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    gamma_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    gamma_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &gamma_barrier);

    gamma_initialized = qtrue;
    vk.postprocess_output_image = vk.gamma_image;
    vk.postprocess_output_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vk.postprocess_output_format = vk_postprocess_color_format();
}

// Post-processing quality settings
int vk_get_post_process_quality(void) {
    return r_postQuality ? std::clamp(r_postQuality->integer, 0, 4) : 2;
}

// Check if post-processing is enabled
qboolean vk_has_post_processing(void) {
    return ((r_bloom && r_bloom->integer != 0) || vk_get_post_process_quality() > 0) ? qtrue : qfalse;
}
