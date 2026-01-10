/*
=============================================================================
Vulkan Enhanced Post-Processing System Implementation

Modular post-processing effects for cinematic quality rendering
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../renderercommon/tr_backend_iface.h"
#ifdef USE_VULKAN
#include "vk.h"
#include "vk_post_process.h"
#include "vk_postprocess.h"
#include "vk_descriptor_batch.h"
#include "tr_math_optimized.h"

// Vulkan function pointers (extern declarations)
extern PFN_vkCreateComputePipelines qvkCreateComputePipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdDispatch qvkCmdDispatch;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Utility functions
extern const char *vk_result_string(VkResult result);
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);

#endif

// CVAR definitions
cvar_t *r_pp_ssao;
cvar_t *r_pp_ssao_lissao;
cvar_t *r_pp_ssr;
cvar_t *r_pp_bloom;
cvar_t *r_pp_bloom_kawase;
cvar_t *r_pp_dof;
cvar_t *r_pp_dof_bokeh_shape;
cvar_t *r_pp_motion_blur;
cvar_t *r_pp_motion_blur_tiles;
cvar_t *r_pp_color_grading;
cvar_t *r_pp_heat_distortion;

// All Vulkan resources are accessed via the vk struct

// All Vulkan resources are accessed via the vk struct

// All Vulkan resources are accessed via the vk struct

/*
===============
vk_init_enhanced_post_processing
===============
*/
void vk_init_enhanced_post_processing(void)
{
    // Register CVARs
    r_pp_ssao = ri.Cvar_Get("r_pp_ssao", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_ssao, "Enable enhanced SSAO (Dual SSAO with LISSAO)");

    r_pp_ssao_lissao = ri.Cvar_Get("r_pp_ssao_lissao", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_ssao_lissao, "Enable LISSAO indirect lighting");

    r_pp_ssr = ri.Cvar_Get("r_pp_ssr", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_ssr, "Enable screen space reflections");

    r_pp_bloom = ri.Cvar_Get("r_pp_bloom", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_bloom, "Enable enhanced bloom");

    r_pp_bloom_kawase = ri.Cvar_Get("r_pp_bloom_kawase", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_bloom_kawase, "Use Kawase blur for bloom (faster, higher quality)");

    r_pp_dof = ri.Cvar_Get("r_pp_dof", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_dof, "Enable depth of field with sprite bokeh");

    r_pp_dof_bokeh_shape = ri.Cvar_Get("r_pp_dof_bokeh_shape", "0", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_dof_bokeh_shape, "Bokeh shape: 0=circular, 1=hexagonal, 2=sprite");

    r_pp_motion_blur = ri.Cvar_Get("r_pp_motion_blur", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_motion_blur, "Enable motion blur");

    r_pp_motion_blur_tiles = ri.Cvar_Get("r_pp_motion_blur_tiles", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_motion_blur_tiles, "Use velocity tiles for motion blur");

    r_pp_color_grading = ri.Cvar_Get("r_pp_color_grading", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_color_grading, "Enable 3D LUT color grading");

    r_pp_heat_distortion = ri.Cvar_Get("r_pp_heat_distortion", "1", CVAR_ARCHIVE);
    ri.Cvar_SetDescription(r_pp_heat_distortion, "Enable heat distortion effects");
}

/*
===============
vk_shutdown_enhanced_post_processing
===============
*/
void vk_shutdown_enhanced_post_processing(void)
{
    // Safety check - ensure Vulkan device is valid
    if (!vk.active || vk.device == VK_NULL_HANDLE || qvkDestroyPipeline == NULL) {
        return;
    }

    // Destroy output images
    if (vk.ssao_output_image_view != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk.ssao_output_image_view, NULL);
        vk.ssao_output_image_view = VK_NULL_HANDLE;
    }
    if (vk.ssao_output_image != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk.ssao_output_image, NULL);
        vk.ssao_output_image = VK_NULL_HANDLE;
    }
    if (vk.ssao_output_image_memory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk.ssao_output_image_memory, NULL);
        vk.ssao_output_image_memory = VK_NULL_HANDLE;
    }
    
    if (vk.ssr_output_image_view != VK_NULL_HANDLE) {
        qvkDestroyImageView(vk.device, vk.ssr_output_image_view, NULL);
        vk.ssr_output_image_view = VK_NULL_HANDLE;
    }
    if (vk.ssr_output_image != VK_NULL_HANDLE) {
        qvkDestroyImage(vk.device, vk.ssr_output_image, NULL);
        vk.ssr_output_image = VK_NULL_HANDLE;
    }
    if (vk.ssr_output_image_memory != VK_NULL_HANDLE) {
        qvkFreeMemory(vk.device, vk.ssr_output_image_memory, NULL);
        vk.ssr_output_image_memory = VK_NULL_HANDLE;
    }
    
    // Destroy pipelines
    if (vk.ssao_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.ssao_pipeline, NULL);
        vk.ssao_pipeline = VK_NULL_HANDLE;
    }
    if (vk.ssr_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.ssr_pipeline, NULL);
        vk.ssr_pipeline = VK_NULL_HANDLE;
    }
    if (vk.bloom_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.bloom_pipeline, NULL);
        vk.bloom_pipeline = VK_NULL_HANDLE;
    }
    if (vk.dof_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.dof_pipeline, NULL);
        vk.dof_pipeline = VK_NULL_HANDLE;
    }
    if (vk.motion_blur_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.motion_blur_pipeline, NULL);
        vk.motion_blur_pipeline = VK_NULL_HANDLE;
    }
    if (vk.velocity_tiles_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.velocity_tiles_pipeline, NULL);
        vk.velocity_tiles_pipeline = VK_NULL_HANDLE;
    }
    if (vk.color_grading_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.color_grading_pipeline, NULL);
        vk.color_grading_pipeline = VK_NULL_HANDLE;
    }
    if (vk.heat_distortion_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.heat_distortion_pipeline, NULL);
        vk.heat_distortion_pipeline = VK_NULL_HANDLE;
    }

    // Destroy pipeline layouts
    if (vk.ssao_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.ssao_layout, NULL);
        vk.ssao_layout = VK_NULL_HANDLE;
    }
    if (vk.ssr_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.ssr_layout, NULL);
        vk.ssr_layout = VK_NULL_HANDLE;
    }
    if (vk.bloom_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.bloom_layout, NULL);
        vk.bloom_layout = VK_NULL_HANDLE;
    }
    if (vk.dof_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.dof_layout, NULL);
        vk.dof_layout = VK_NULL_HANDLE;
    }
    if (vk.motion_blur_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.motion_blur_layout, NULL);
        vk.motion_blur_layout = VK_NULL_HANDLE;
    }
    if (vk.velocity_tiles_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.velocity_tiles_layout, NULL);
        vk.velocity_tiles_layout = VK_NULL_HANDLE;
    }
    if (vk.color_grading_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.color_grading_layout, NULL);
        vk.color_grading_layout = VK_NULL_HANDLE;
    }
    if (vk.heat_distortion_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, vk.heat_distortion_layout, NULL);
        vk.heat_distortion_layout = VK_NULL_HANDLE;
    }
}

/*
===============
vk_create_compute_pipeline
===============
*/
VkPipeline vk_create_compute_pipeline(VkShaderModule computeShader, VkPipelineLayout layout, const char *name)
{
    VkComputePipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    createInfo.stage.module = computeShader;
    createInfo.stage.pName = "main";
    createInfo.layout = layout;

    VkPipeline pipeline;
    VkResult result = qvkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &createInfo, NULL, &pipeline);

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create %s pipeline: %s\n", name, vk_result_string(result));
        return VK_NULL_HANDLE;
    }

    SET_OBJECT_NAME(pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
    ri.Printf(PRINT_ALL, "Vulkan: Created %s pipeline\n", name);

    return pipeline;
}

/*
===============
vk_create_ssao_pipeline
===============
*/
qboolean vk_create_ssao_pipeline(void)
{
    if (vk.modules.ssao_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "SSAO compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // depthBuffer
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // normalBuffer
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // blueNoise
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // colorBuffer (for LISSAO)
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // outputImage
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.ssao_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSAO descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.ssao_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange ssaoPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssaoConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &ssaoPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.ssao_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSAO pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.ssao_pipeline = vk_create_compute_pipeline(vk.modules.ssao_comp, vk.ssao_layout, "SSAO");
    return vk.ssao_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_ssr_pipeline
===============
*/
qboolean vk_create_ssr_pipeline(void)
{
    if (vk.modules.ssr_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "SSR compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for SSR
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // colorBuffer
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // normalBuffer
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // depthBuffer
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // blueNoise
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // outputImage
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.ssr_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSR descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.ssr_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange ssrPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssrConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &ssrPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.ssr_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSR pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.ssr_pipeline = vk_create_compute_pipeline(vk.modules.ssr_comp, vk.ssr_layout, "SSR");
    return vk.ssr_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_bloom_pipeline
===============
*/
qboolean vk_create_bloom_pipeline(void)
{
    if (vk.modules.bloom_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Bloom compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for bloom
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // input_texture
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // output_image
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.bloom_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create bloom descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.bloom_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange bloomPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bloomConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &bloomPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.bloom_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create bloom pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.bloom_pipeline = vk_create_compute_pipeline(vk.modules.bloom_comp, vk.bloom_layout, "Bloom");
    return vk.bloom_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_dof_pipeline
===============
*/
qboolean vk_create_dof_pipeline(void)
{
    if (vk.modules.depth_of_field_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Depth of field compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for DoF
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // colorBuffer
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // depthBuffer
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // cocBuffer
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // bokehSprite
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // outputImage
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.dof_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create DoF descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.dof_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange dofPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dofConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &dofPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.dof_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create DoF pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.dof_pipeline = vk_create_compute_pipeline(vk.modules.depth_of_field_comp, vk.dof_layout, "Depth of Field");
    return vk.dof_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_velocity_tiles_pipeline
===============
*/
qboolean vk_create_velocity_tiles_pipeline(void)
{
    if (vk.modules.velocity_tiles_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Velocity tiles compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for velocity tiles
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // velocityBuffer
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // depthBuffer
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // velocityTiles
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.velocity_tiles_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create velocity tiles descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.velocity_tiles_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange velocityTilesPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vec4_t) * 2 }; // tileResolution, invTileResolution, pixelsPerTile, invPixelsPerTile
    pipelineLayoutInfo.pPushConstantRanges = &velocityTilesPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.velocity_tiles_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create velocity tiles pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.velocity_tiles_pipeline = vk_create_compute_pipeline(vk.modules.velocity_tiles_comp, vk.velocity_tiles_layout, "Velocity Tiles");
    return vk.velocity_tiles_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_motion_blur_pipeline
===============
*/
qboolean vk_create_motion_blur_pipeline(void)
{
    if (vk.modules.motion_blur_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Motion blur compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for motion blur
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // colorBuffer
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // velocityBuffer
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // depthBuffer
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // velocityTiles
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // outputImage
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.motion_blur_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create motion blur descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.motion_blur_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange motionBlurPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(motionBlurConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &motionBlurPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.motion_blur_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create motion blur pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.motion_blur_pipeline = vk_create_compute_pipeline(vk.modules.motion_blur_comp, vk.motion_blur_layout, "Motion Blur");
    return vk.motion_blur_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_color_grading_pipeline
===============
*/
qboolean vk_create_color_grading_pipeline(void)
{
    if (vk.modules.color_grading_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Color grading compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for color grading
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // inputImage
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // lutTexture
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // outputImage
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.color_grading_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create color grading descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.color_grading_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange colorGradingPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(colorGradingConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &colorGradingPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.color_grading_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create color grading pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.color_grading_pipeline = vk_create_compute_pipeline(vk.modules.color_grading_comp, vk.color_grading_layout, "Color Grading");
    return vk.color_grading_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_heat_distortion_pipeline
===============
*/
qboolean vk_create_heat_distortion_pipeline(void)
{
    if (vk.modules.heat_distortion_comp == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Heat distortion compute shader not loaded\n");
        return qfalse;
    }

    // Create descriptor set layout for heat distortion
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // colorBuffer
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // depthBuffer
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // heatMask
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // noiseTexture
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},          // outputImage
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.heat_distortion_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create heat distortion descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.heat_distortion_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VkPushConstantRange heatDistortionPushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(heatDistortionConfig_t) };
    pipelineLayoutInfo.pPushConstantRanges = &heatDistortionPushRange;

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &vk.heat_distortion_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create heat distortion pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    vk.heat_distortion_pipeline = vk_create_compute_pipeline(vk.modules.heat_distortion_comp, vk.heat_distortion_layout, "Heat Distortion");
    return vk.heat_distortion_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_enhanced_post_process_pipelines
===============
*/
qboolean vk_create_enhanced_post_process_pipelines(void)
{
    qboolean success = qtrue;

    // Ensure bloom resources exist before creating pipelines
    if (r_pp_bloom && r_pp_bloom->integer && vk.renderWidth > 0 && vk.renderHeight > 0 && vk.color_format != VK_FORMAT_UNDEFINED) {
        if (vk.bloom_image[0] == VK_NULL_HANDLE) {
            if (!vk_create_bloom_resources()) {
                ri.Printf(PRINT_ERROR, "Failed to create bloom resources for pipelines\n");
                success = qfalse;
            }
        }
    }

    success &= vk_create_ssao_pipeline();
    success &= vk_create_ssr_pipeline();
    success &= vk_create_bloom_pipeline();
    success &= vk_create_dof_pipeline();
    success &= vk_create_velocity_tiles_pipeline();
    success &= vk_create_motion_blur_pipeline();
    success &= vk_create_color_grading_pipeline();
    success &= vk_create_heat_distortion_pipeline();

    return success;
}

/*
===============
vk_execute_post_processing
===============
*/
void vk_execute_post_processing(const postProcessConfig_t *config)
{
    // Execute effects in order based on enabled flags
    if (config->enabledEffects & PP_EFFECT_SSAO) {
        vk_ssao_pass(&config->ssao);
    }
    if (config->enabledEffects & PP_EFFECT_SSR) {
        vk_ssr_pass(&config->ssr);
    }
    if (config->enabledEffects & PP_EFFECT_BLOOM) {
        vk_bloom_pass(&config->bloom);
    }
    if (config->enabledEffects & PP_EFFECT_DOF) {
        vk_dof_pass(&config->dof);
    }
    if (config->enabledEffects & PP_EFFECT_MOTION_BLUR) {
        vk_motion_blur_pass(&config->motionBlur);
    }
    if (config->enabledEffects & PP_EFFECT_COLOR_GRADING) {
        vk_color_grading_pass(&config->colorGrading);
    }
    if (config->enabledEffects & PP_EFFECT_HEAT_DISTORTION) {
        vk_heat_distortion_pass(&config->heatDistortion);
    }
}

/*
===============
vk_update_post_process_config
===============
Populate post-processing configuration structure from CVARs and current render state
*/
void vk_update_post_process_config(postProcessConfig_t *config)
{
    if (!config) {
        return;
    }
    
    // Get current render resolution
    vec2_t resolution = { (float)vk.renderWidth, (float)vk.renderHeight };
    vec2_t invResolution = { 1.0f / vk.renderWidth, 1.0f / vk.renderHeight };
    
    // Initialize config structure
    memset(config, 0, sizeof(postProcessConfig_t));
    
    // Set enabled effects based on CVARs
    if (r_pp_ssao && r_pp_ssao->integer) {
        config->enabledEffects |= PP_EFFECT_SSAO;
        
        // Configure SSAO
        config->ssao.resolution[0] = resolution[0];
        config->ssao.resolution[1] = resolution[1];
        config->ssao.invResolution[0] = invResolution[0];
        config->ssao.invResolution[1] = invResolution[1];
        config->ssao.radius = 0.5f; // Default radius
        config->ssao.bias = 0.025f; // Default bias
        config->ssao.intensity = 1.0f; // Default intensity
        config->ssao.numSamples = 16; // Default sample count
        config->ssao.enableLISSAO = (r_pp_ssao_lissao && r_pp_ssao_lissao->integer) ? qtrue : qfalse;
        config->ssao.indirectIntensity = 0.5f; // Default indirect intensity
        config->ssao.indirectRadius = 1.0f; // Default indirect radius
    }
    
    if (r_pp_ssr && r_pp_ssr->integer) {
        config->enabledEffects |= PP_EFFECT_SSR;
        
        // Configure SSR
        config->ssr.resolution[0] = resolution[0];
        config->ssr.resolution[1] = resolution[1];
        config->ssr.invResolution[0] = invResolution[0];
        config->ssr.invResolution[1] = invResolution[1];
        // Copy camera position from refdef (vieworg is an array, not a pointer)
        VectorCopy(tr.refdef.vieworg, config->ssr.cameraPos);
        config->ssr.maxDistance = 100.0f; // Default max distance
        config->ssr.thickness = 0.1f; // Default thickness
        config->ssr.numSteps = 32; // Default step count
        config->ssr.numBinarySteps = 8; // Default binary search steps
        config->ssr.roughnessThreshold = 0.5f; // Default roughness threshold
    }
    
    if (r_pp_bloom && r_pp_bloom->integer) {
        config->enabledEffects |= PP_EFFECT_BLOOM;
        
        // Configure Bloom
        config->bloom.resolution[0] = resolution[0];
        config->bloom.resolution[1] = resolution[1];
        config->bloom.invResolution[0] = invResolution[0];
        config->bloom.invResolution[1] = invResolution[1];
        config->bloom.threshold = 1.0f; // Default threshold
        config->bloom.extractMode = 2; // Default: luma
        config->bloom.modulateMode = 1; // Default: square
        config->bloom.useKawase = (r_pp_bloom_kawase && r_pp_bloom_kawase->integer) ? qtrue : qfalse;
        config->bloom.numPasses = 4; // Default number of passes
    }
    
    if (r_pp_dof && r_pp_dof->integer) {
        config->enabledEffects |= PP_EFFECT_DOF;
        
        // Configure DoF
        config->dof.resolution[0] = resolution[0];
        config->dof.resolution[1] = resolution[1];
        config->dof.invResolution[0] = invResolution[0];
        config->dof.invResolution[1] = invResolution[1];
        config->dof.focalDistance = 10.0f; // Default focal distance
        config->dof.focalRange = 2.0f; // Default focal range
        config->dof.aperture = 5.6f; // Default aperture
        config->dof.numSamples = 16; // Default sample count
        config->dof.bokehShape = (r_pp_dof_bokeh_shape) ? r_pp_dof_bokeh_shape->integer : 0;
        config->dof.bokehRotation = 0.0f; // Default rotation
    }
    
    if (r_pp_motion_blur && r_pp_motion_blur->integer) {
        config->enabledEffects |= PP_EFFECT_MOTION_BLUR;
        
        // Configure Motion Blur
        config->motionBlur.resolution[0] = resolution[0];
        config->motionBlur.resolution[1] = resolution[1];
        config->motionBlur.invResolution[0] = invResolution[0];
        config->motionBlur.invResolution[1] = invResolution[1];
        config->motionBlur.numSamples = 8; // Default sample count
        config->motionBlur.exposureTime = 0.5f; // Default exposure time
        config->motionBlur.useVelocityTiles = (r_pp_motion_blur_tiles && r_pp_motion_blur_tiles->integer) ? qtrue : qfalse;
        config->motionBlur.tileSize[0] = 16.0f; // Default tile size
        config->motionBlur.tileSize[1] = 16.0f;
    }
    
    if (r_pp_color_grading && r_pp_color_grading->integer) {
        config->enabledEffects |= PP_EFFECT_COLOR_GRADING;
        
        // Configure Color Grading
        config->colorGrading.resolution[0] = resolution[0];
        config->colorGrading.resolution[1] = resolution[1];
        config->colorGrading.invResolution[0] = invResolution[0];
        config->colorGrading.invResolution[1] = invResolution[1];
        config->colorGrading.exposure = 1.0f; // Default exposure
        config->colorGrading.contrast = 1.0f; // Default contrast
        config->colorGrading.saturation = 1.0f; // Default saturation
        config->colorGrading.brightness = 0.0f; // Default brightness
        VectorSet(config->colorGrading.colorFilter, 1.0f, 1.0f, 1.0f); // Default: no filter
        config->colorGrading.gamma = 2.2f; // Default gamma
        VectorSet(config->colorGrading.shadows, 0.0f, 0.0f, 0.0f); // Default shadows
        VectorSet(config->colorGrading.midtones, 0.0f, 0.0f, 0.0f); // Default midtones
        VectorSet(config->colorGrading.highlights, 0.0f, 0.0f, 0.0f); // Default highlights
        config->colorGrading.shadowsStart = 0.0f;
        config->colorGrading.shadowsEnd = 0.3f;
        config->colorGrading.highlightsStart = 0.7f;
        config->colorGrading.highlightsEnd = 1.0f;
        config->colorGrading.useLUT = qfalse; // Default: no LUT
    }
    
    if (r_pp_heat_distortion && r_pp_heat_distortion->integer) {
        config->enabledEffects |= PP_EFFECT_HEAT_DISTORTION;
        
        // Configure Heat Distortion
        config->heatDistortion.resolution[0] = resolution[0];
        config->heatDistortion.resolution[1] = resolution[1];
        config->heatDistortion.invResolution[0] = invResolution[0];
        config->heatDistortion.invResolution[1] = invResolution[1];
        config->heatDistortion.time = (tr.refdef.time) ? tr.refdef.time * 0.001f : 0.0f; // Convert to seconds
        config->heatDistortion.distortionStrength = 0.1f; // Default strength
        config->heatDistortion.heatWaveFrequency = 1.0f; // Default frequency
        config->heatDistortion.heatWaveSpeed = 1.0f; // Default speed
        config->heatDistortion.numSamples = 8; // Default sample count
        config->heatDistortion.atmosphericDistortion = 0.05f; // Default atmospheric distortion
        VectorSet(config->heatDistortion.atmosphericTint, 1.0f, 0.9f, 0.8f); // Default warm tint
    }
}

/*
===============
vk_ssao_pass
===============
Execute SSAO (Screen Space Ambient Occlusion) compute pass
*/
qboolean vk_ssao_pass(const ssaoConfig_t *config)
{
    if (!config || vk.device_lost || vk.device == VK_NULL_HANDLE) {
        return qfalse;
    }
    
    // Check if pipeline is available
    if (vk.ssao_pipeline == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSAO pipeline not created, skipping pass\n");
        return qfalse;
    }
    
    // Check if command buffer is available
    if (!vk.cmd || !vk.cmd->command_buffer || vk.cmd->command_buffer == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSAO pass - no command buffer available\n");
        return qfalse;
    }
    
    // Ensure output image exists
    if (vk.ssao_output_image == VK_NULL_HANDLE) {
        // Create SSAO output image if it doesn't exist
        uint32_t width = (uint32_t)config->resolution[0];
        uint32_t height = (uint32_t)config->resolution[1];
        
        if (width == 0 || height == 0) {
            width = vk.renderWidth;
            height = vk.renderHeight;
        }
        
        if (width > 0 && height > 0) {
            VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = VK_FORMAT_R8_UNORM; // Single channel for occlusion
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            if (qvkCreateImage(vk.device, &imageInfo, NULL, &vk.ssao_output_image) == VK_SUCCESS) {
                VkMemoryRequirements memReqs;
                qvkGetImageMemoryRequirements(vk.device, vk.ssao_output_image, &memReqs);
                
                VkMemoryAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memReqs.size;
                allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                
                if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk.ssao_output_image_memory) == VK_SUCCESS) {
                    qvkBindImageMemory(vk.device, vk.ssao_output_image, vk.ssao_output_image_memory, 0);
                    
                    VkImageViewCreateInfo viewInfo = {};
                    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    viewInfo.image = vk.ssao_output_image;
                    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    viewInfo.format = VK_FORMAT_R8_UNORM;
                    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    viewInfo.subresourceRange.baseMipLevel = 0;
                    viewInfo.subresourceRange.levelCount = 1;
                    viewInfo.subresourceRange.baseArrayLayer = 0;
                    viewInfo.subresourceRange.layerCount = 1;
                    
                    if (qvkCreateImageView(vk.device, &viewInfo, NULL, &vk.ssao_output_image_view) != VK_SUCCESS) {
                        qvkFreeMemory(vk.device, vk.ssao_output_image_memory, NULL);
                        qvkDestroyImage(vk.device, vk.ssao_output_image, NULL);
                        vk.ssao_output_image = VK_NULL_HANDLE;
                        vk.ssao_output_image_memory = VK_NULL_HANDLE;
                        ri.Printf(PRINT_WARNING, "Vulkan: Failed to create SSAO output image view\n");
                        return qfalse;
                    }
                } else {
                    qvkDestroyImage(vk.device, vk.ssao_output_image, NULL);
                    vk.ssao_output_image = VK_NULL_HANDLE;
                    ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate SSAO output image memory\n");
                    return qfalse;
                }
            } else {
                ri.Printf(PRINT_WARNING, "Vulkan: Failed to create SSAO output image\n");
                return qfalse;
            }
        }
    }
    
    // Check required input resources
    if (vk.depth_image_view == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSAO pass - depth buffer not available\n");
        return qfalse;
    }
    
    // Get normal buffer (use RT denoise normal buffer if available, otherwise fallback)
    VkImageView normalBufferView = VK_NULL_HANDLE;
    if (vk.rt.denoiseNormalBufferView != VK_NULL_HANDLE) {
        normalBufferView = vk.rt.denoiseNormalBufferView;
    } else {
        // Fallback: use white image as placeholder (will produce no occlusion, but won't crash)
        if (tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE) {
            normalBufferView = tr.whiteImage->view;
            ri.Printf(PRINT_DEVELOPER, "Vulkan: SSAO using fallback normal buffer\n");
        } else {
            ri.Printf(PRINT_DEVELOPER, "Vulkan: SSAO pass - normal buffer not available\n");
            return qfalse;
        }
    }
    
    // Get blue noise texture
    VkImageView blueNoiseView = VK_NULL_HANDLE;
    if (vk.rt.blueNoiseTexture && vk.rt.blueNoiseTexture->view != VK_NULL_HANDLE) {
        blueNoiseView = vk.rt.blueNoiseTexture->view;
    } else if (tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE) {
        blueNoiseView = tr.whiteImage->view; // Fallback
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSAO using fallback blue noise texture\n");
    }
    
    // Get color buffer for LISSAO (optional)
    VkImageView colorBufferView = vk.color_image_view;
    if (colorBufferView == VK_NULL_HANDLE && tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE) {
        colorBufferView = tr.whiteImage->view; // Fallback
    }
    
    // Update descriptor set
    VkDescriptorImageInfo depthImageInfo = {};
    depthImageInfo.sampler = vk.samplers.handle[0]; // Use first sampler
    depthImageInfo.imageView = vk.depth_image_view;
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo normalImageInfo = {};
    normalImageInfo.sampler = vk.samplers.handle[0];
    normalImageInfo.imageView = normalBufferView;
    normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo blueNoiseImageInfo = {};
    blueNoiseImageInfo.sampler = vk.samplers.handle[0];
    blueNoiseImageInfo.imageView = blueNoiseView;
    blueNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo colorImageInfo = {};
    colorImageInfo.sampler = vk.samplers.handle[0];
    colorImageInfo.imageView = colorBufferView;
    colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo outputImageInfo = {};
    outputImageInfo.imageView = vk.ssao_output_image_view;
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // Storage images use GENERAL layout
    
    VkWriteDescriptorSet writes[5] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk.ssao_descriptor;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &depthImageInfo;
    
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk.ssao_descriptor;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &normalImageInfo;
    
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = vk.ssao_descriptor;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &blueNoiseImageInfo;
    
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = vk.ssao_descriptor;
    writes[3].dstBinding = 3;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &colorImageInfo;
    
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = vk.ssao_descriptor;
    writes[4].dstBinding = 4;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &outputImageInfo;
    
    // Use batched descriptor updates
    extern qboolean vk_descriptor_batch_defer_update(const VkWriteDescriptorSet *write);
    for (int i = 0; i < 5; i++) {
        if (!vk_descriptor_batch_defer_update(&writes[i])) {
            // Fallback to immediate update if batching fails
            qvkUpdateDescriptorSets(vk.device, 1, &writes[i], 0, NULL);
        }
    }
    
    // Transition output image to GENERAL layout for compute shader write
    VkImageMemoryBarrier outputBarrier = {};
    outputBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    outputBarrier.srcAccessMask = 0;
    outputBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.image = vk.ssao_output_image;
    outputBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    outputBarrier.subresourceRange.baseMipLevel = 0;
    outputBarrier.subresourceRange.levelCount = 1;
    outputBarrier.subresourceRange.baseArrayLayer = 0;
    outputBarrier.subresourceRange.layerCount = 1;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 0, NULL, 0, NULL, 1, &outputBarrier);
    
    // Get matrices from backEnd
    extern backEndState_t backEnd;
    mat4_t projectionMatrix, invProjectionMatrix;
    Com_Memcpy(projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(mat4_t));
    // Use Matrix16Inverse (standard function) instead of optimized version for compatibility
    Matrix16Inverse(projectionMatrix, invProjectionMatrix);
    
    // Prepare push constants
    struct {
        float projectionMatrix[16];
        float invProjectionMatrix[16];
        float resolution[2];
        float invResolution[2];
        float radius;
        float bias;
        float intensity;
        int numSamples;
        int enableLISSAO;
        float indirectIntensity;
        float indirectRadius;
    } pushConstants;
    
    Com_Memcpy(pushConstants.projectionMatrix, projectionMatrix, sizeof(float) * 16);
    Com_Memcpy(pushConstants.invProjectionMatrix, invProjectionMatrix, sizeof(float) * 16);
    pushConstants.resolution[0] = config->resolution[0];
    pushConstants.resolution[1] = config->resolution[1];
    pushConstants.invResolution[0] = config->invResolution[0];
    pushConstants.invResolution[1] = config->invResolution[1];
    pushConstants.radius = config->radius;
    pushConstants.bias = config->bias;
    pushConstants.intensity = config->intensity;
    pushConstants.numSamples = config->numSamples;
    pushConstants.enableLISSAO = config->enableLISSAO ? 1 : 0;
    pushConstants.indirectIntensity = config->indirectIntensity;
    pushConstants.indirectRadius = config->indirectRadius;
    
    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.ssao_pipeline);
    
    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                             vk.ssao_layout, 0, 1, &vk.ssao_descriptor, 0, NULL);
    
    // Push constants
    qvkCmdPushConstants(vk.cmd->command_buffer, vk.ssao_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pushConstants), &pushConstants);
    
    // Dispatch compute shader (workgroup size is 8x8)
    uint32_t groupCountX = ((uint32_t)config->resolution[0] + 7) / 8;
    uint32_t groupCountY = ((uint32_t)config->resolution[1] + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);
    
    // Memory barrier to ensure compute shader completes before using output
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Transition output image to SHADER_READ_ONLY_OPTIMAL for sampling
    outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          0, 0, NULL, 0, NULL, 1, &outputBarrier);
    
    return qtrue;
}

/*
===============
vk_ssr_pass
===============
Execute SSR (Screen Space Reflections) compute pass
*/
qboolean vk_ssr_pass(const ssrConfig_t *config)
{
    if (!config || vk.device_lost || vk.device == VK_NULL_HANDLE) {
        return qfalse;
    }
    
    // Check if pipeline is available
    if (vk.ssr_pipeline == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR pipeline not created, skipping pass\n");
        return qfalse;
    }
    
    // Check if command buffer is available
    if (!vk.cmd || !vk.cmd->command_buffer || vk.cmd->command_buffer == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR pass - no command buffer available\n");
        return qfalse;
    }
    
    // Ensure output image exists
    if (vk.ssr_output_image == VK_NULL_HANDLE) {
        // Create SSR output image if it doesn't exist
        uint32_t width = (uint32_t)config->resolution[0];
        uint32_t height = (uint32_t)config->resolution[1];
        
        if (width == 0 || height == 0) {
            width = vk.renderWidth;
            height = vk.renderHeight;
        }
        
        if (width > 0 && height > 0) {
            VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format for reflections
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            if (qvkCreateImage(vk.device, &imageInfo, NULL, &vk.ssr_output_image) == VK_SUCCESS) {
                VkMemoryRequirements memReqs;
                qvkGetImageMemoryRequirements(vk.device, vk.ssr_output_image, &memReqs);
                
                VkMemoryAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memReqs.size;
                allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                
                if (qvkAllocateMemory(vk.device, &allocInfo, NULL, &vk.ssr_output_image_memory) == VK_SUCCESS) {
                    qvkBindImageMemory(vk.device, vk.ssr_output_image, vk.ssr_output_image_memory, 0);
                    
                    VkImageViewCreateInfo viewInfo = {};
                    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    viewInfo.image = vk.ssr_output_image;
                    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
                    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    viewInfo.subresourceRange.baseMipLevel = 0;
                    viewInfo.subresourceRange.levelCount = 1;
                    viewInfo.subresourceRange.baseArrayLayer = 0;
                    viewInfo.subresourceRange.layerCount = 1;
                    
                    if (qvkCreateImageView(vk.device, &viewInfo, NULL, &vk.ssr_output_image_view) != VK_SUCCESS) {
                        qvkFreeMemory(vk.device, vk.ssr_output_image_memory, NULL);
                        qvkDestroyImage(vk.device, vk.ssr_output_image, NULL);
                        vk.ssr_output_image = VK_NULL_HANDLE;
                        vk.ssr_output_image_memory = VK_NULL_HANDLE;
                        ri.Printf(PRINT_WARNING, "Vulkan: Failed to create SSR output image view\n");
                        return qfalse;
                    }
                } else {
                    qvkDestroyImage(vk.device, vk.ssr_output_image, NULL);
                    vk.ssr_output_image = VK_NULL_HANDLE;
                    ri.Printf(PRINT_WARNING, "Vulkan: Failed to allocate SSR output image memory\n");
                    return qfalse;
                }
            } else {
                ri.Printf(PRINT_WARNING, "Vulkan: Failed to create SSR output image\n");
                return qfalse;
            }
        }
    }
    
    // Check required input resources
    if (vk.depth_image_view == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR pass - depth buffer not available\n");
        return qfalse;
    }
    
    if (vk.color_image_view == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR pass - color buffer not available\n");
        return qfalse;
    }
    
    // Get normal buffer (use RT denoise normal buffer if available, otherwise fallback)
    VkImageView normalBufferView = VK_NULL_HANDLE;
    if (vk.rt.denoiseNormalBufferView != VK_NULL_HANDLE) {
        normalBufferView = vk.rt.denoiseNormalBufferView;
    } else {
        // Fallback: use white image as placeholder
        if (tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE) {
            normalBufferView = tr.whiteImage->view;
            ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR using fallback normal buffer\n");
        } else {
            ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR pass - normal buffer not available\n");
            return qfalse;
        }
    }
    
    // Get blue noise texture
    VkImageView blueNoiseView = VK_NULL_HANDLE;
    if (vk.rt.blueNoiseTexture && vk.rt.blueNoiseTexture->view != VK_NULL_HANDLE) {
        blueNoiseView = vk.rt.blueNoiseTexture->view;
    } else if (tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE) {
        blueNoiseView = tr.whiteImage->view; // Fallback
        ri.Printf(PRINT_DEVELOPER, "Vulkan: SSR using fallback blue noise texture\n");
    }
    
    // Update descriptor set
    VkDescriptorImageInfo colorImageInfo = {};
    colorImageInfo.sampler = vk.samplers.handle[0];
    colorImageInfo.imageView = vk.color_image_view;
    colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo normalImageInfo = {};
    normalImageInfo.sampler = vk.samplers.handle[0];
    normalImageInfo.imageView = normalBufferView;
    normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo depthImageInfo = {};
    depthImageInfo.sampler = vk.samplers.handle[0];
    depthImageInfo.imageView = vk.depth_image_view;
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo blueNoiseImageInfo = {};
    blueNoiseImageInfo.sampler = vk.samplers.handle[0];
    blueNoiseImageInfo.imageView = blueNoiseView;
    blueNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo outputImageInfo = {};
    outputImageInfo.imageView = vk.ssr_output_image_view;
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // Storage images use GENERAL layout
    
    VkWriteDescriptorSet writes[5] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk.ssr_descriptor;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &colorImageInfo;
    
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk.ssr_descriptor;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &normalImageInfo;
    
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = vk.ssr_descriptor;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &depthImageInfo;
    
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = vk.ssr_descriptor;
    writes[3].dstBinding = 3;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &blueNoiseImageInfo;
    
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = vk.ssr_descriptor;
    writes[4].dstBinding = 4;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &outputImageInfo;
    
    // Use batched descriptor updates
    extern qboolean vk_descriptor_batch_defer_update(const VkWriteDescriptorSet *write);
    for (int i = 0; i < 5; i++) {
        if (!vk_descriptor_batch_defer_update(&writes[i])) {
            // Fallback to immediate update if batching fails
            qvkUpdateDescriptorSets(vk.device, 1, &writes[i], 0, NULL);
        }
    }
    
    // Flush descriptor updates before binding
    extern void vk_descriptor_batch_flush(void);
    vk_descriptor_batch_flush();
    
    // Transition output image to GENERAL layout for compute shader write
    VkImageMemoryBarrier outputBarrier = {};
    outputBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    outputBarrier.srcAccessMask = 0;
    outputBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.image = vk.ssr_output_image;
    outputBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    outputBarrier.subresourceRange.baseMipLevel = 0;
    outputBarrier.subresourceRange.levelCount = 1;
    outputBarrier.subresourceRange.baseArrayLayer = 0;
    outputBarrier.subresourceRange.layerCount = 1;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 0, NULL, 0, NULL, 1, &outputBarrier);
    
    // Get matrices from backEnd
    extern backEndState_t backEnd;
    mat4_t projectionMatrix, invProjectionMatrix;
    mat4_t viewMatrix, invViewMatrix;
    Com_Memcpy(projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof(mat4_t));
    // Use Matrix16Inverse (standard function) instead of optimized version for compatibility
    Matrix16Inverse(projectionMatrix, invProjectionMatrix);
    Com_Memcpy(viewMatrix, backEnd.viewParms.world.modelViewMatrix, sizeof(mat4_t));
    Matrix16Inverse(viewMatrix, invViewMatrix);
    
    // Prepare push constants
    struct {
        float projectionMatrix[16];
        float viewMatrix[16];
        float invProjectionMatrix[16];
        float invViewMatrix[16];
        float resolution[2];
        float invResolution[2];
        float cameraPos[3];
        float maxDistance;
        float thickness;
        int numSteps;
        int numBinarySteps;
        float roughnessThreshold;
    } pushConstants;
    
    Com_Memcpy(pushConstants.projectionMatrix, projectionMatrix, sizeof(float) * 16);
    Com_Memcpy(pushConstants.viewMatrix, viewMatrix, sizeof(float) * 16);
    Com_Memcpy(pushConstants.invProjectionMatrix, invProjectionMatrix, sizeof(float) * 16);
    Com_Memcpy(pushConstants.invViewMatrix, invViewMatrix, sizeof(float) * 16);
    pushConstants.resolution[0] = config->resolution[0];
    pushConstants.resolution[1] = config->resolution[1];
    pushConstants.invResolution[0] = config->invResolution[0];
    pushConstants.invResolution[1] = config->invResolution[1];
    pushConstants.cameraPos[0] = config->cameraPos[0];
    pushConstants.cameraPos[1] = config->cameraPos[1];
    pushConstants.cameraPos[2] = config->cameraPos[2];
    pushConstants.maxDistance = config->maxDistance;
    pushConstants.thickness = config->thickness;
    pushConstants.numSteps = config->numSteps;
    pushConstants.numBinarySteps = config->numBinarySteps;
    pushConstants.roughnessThreshold = config->roughnessThreshold;
    
    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.ssr_pipeline);
    
    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                             vk.ssr_layout, 0, 1, &vk.ssr_descriptor, 0, NULL);
    
    // Push constants
    qvkCmdPushConstants(vk.cmd->command_buffer, vk.ssr_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pushConstants), &pushConstants);
    
    // Dispatch compute shader (workgroup size is 8x8)
    uint32_t groupCountX = ((uint32_t)config->resolution[0] + 7) / 8;
    uint32_t groupCountY = ((uint32_t)config->resolution[1] + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);
    
    // Memory barrier to ensure compute shader completes before using output
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Transition output image to SHADER_READ_ONLY_OPTIMAL for sampling
    outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          0, 0, NULL, 0, NULL, 1, &outputBarrier);
    
    return qtrue;
}

/*
===============
vk_bloom_pass
===============
Execute enhanced bloom compute pass (Kawase blur or traditional)
Note: Basic bloom is already implemented in vk_postprocess.cpp via vk_apply_bloom()
This is the enhanced version with Kawase blur support
*/
qboolean vk_bloom_pass(const bloomConfig_t *config)
{
    if (!config) {
        return qfalse;
    }
    
    // Check if pipeline is available
    if (vk.bloom_pipeline == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Enhanced bloom pipeline not created, skipping pass\n");
        // Fall back to basic bloom if available
        if (r_pp_bloom && r_pp_bloom->integer) {
            extern void vk_apply_bloom(void);
            vk_apply_bloom();
            return qtrue;
        }
        return qfalse;
    }
    
    // Implement enhanced bloom compute pass
    if (vk.device_lost || vk.device == VK_NULL_HANDLE) {
        return qfalse;
    }
    
    // Check if command buffer is available
    if (!vk.cmd || !vk.cmd->command_buffer || vk.cmd->command_buffer == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Bloom pass - no command buffer available\n");
        return qfalse;
    }
    
    // Ensure bloom output image exists (use first bloom image if available)
    VkImageView bloomOutputView = VK_NULL_HANDLE;
    if (vk.bloom_image_view[0] != VK_NULL_HANDLE) {
        bloomOutputView = vk.bloom_image_view[0];
    } else {
        // Create bloom output image if it doesn't exist
        uint32_t width = (uint32_t)config->resolution[0];
        uint32_t height = (uint32_t)config->resolution[1];
        
        if (width == 0 || height == 0) {
            width = vk.renderWidth;
            height = vk.renderHeight;
        }
        
        if (width > 0 && height > 0 && vk.bloom_image[0] == VK_NULL_HANDLE) {
            // Bloom images are created during initialization, not here
            // If they don't exist, we can't proceed with compute bloom
            ri.Printf(PRINT_DEVELOPER, "Vulkan: Bloom pass - bloom images not initialized\n");
            return qfalse;
        }
        bloomOutputView = vk.bloom_image_view[0];
    }
    
    if (bloomOutputView == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Bloom pass - bloom output image not available\n");
        return qfalse;
    }
    
    // Get input color buffer
    if (vk.color_image_view == VK_NULL_HANDLE) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Bloom pass - color buffer not available\n");
        return qfalse;
    }
    
    // Update descriptor set
    VkDescriptorImageInfo inputImageInfo = {};
    inputImageInfo.sampler = vk.samplers.handle[0];
    inputImageInfo.imageView = vk.color_image_view;
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo outputImageInfo = {};
    outputImageInfo.imageView = bloomOutputView;
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // Storage images use GENERAL layout
    
    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk.bloom_descriptor;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &inputImageInfo;
    
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk.bloom_descriptor;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &outputImageInfo;
    
    // Use batched descriptor updates
    extern qboolean vk_descriptor_batch_defer_update(const VkWriteDescriptorSet *write);
    for (int i = 0; i < 2; i++) {
        if (!vk_descriptor_batch_defer_update(&writes[i])) {
            qvkUpdateDescriptorSets(vk.device, 1, &writes[i], 0, NULL);
        }
    }
    
    // Flush descriptor updates
    extern void vk_descriptor_batch_flush(void);
    vk_descriptor_batch_flush();
    
    // Transition output image to GENERAL layout for compute shader write
    VkImageMemoryBarrier outputBarrier = {};
    outputBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    outputBarrier.srcAccessMask = 0;
    outputBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.image = vk.bloom_image[0];
    outputBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    outputBarrier.subresourceRange.baseMipLevel = 0;
    outputBarrier.subresourceRange.levelCount = 1;
    outputBarrier.subresourceRange.baseArrayLayer = 0;
    outputBarrier.subresourceRange.layerCount = 1;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 0, NULL, 0, NULL, 1, &outputBarrier);
    
    // Prepare push constants
    struct {
        float threshold;
        int extract_mode;
        int base_modulate;
    } pushConstants;
    
    pushConstants.threshold = config->threshold;
    pushConstants.extract_mode = config->extractMode;
    pushConstants.base_modulate = config->modulateMode;
    
    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.bloom_pipeline);
    
    // Bind descriptor set
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                             vk.bloom_layout, 0, 1, &vk.bloom_descriptor, 0, NULL);
    
    // Push constants
    qvkCmdPushConstants(vk.cmd->command_buffer, vk.bloom_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pushConstants), &pushConstants);
    
    // Dispatch compute shader (workgroup size is 8x8)
    uint32_t groupCountX = ((uint32_t)config->resolution[0] + 7) / 8;
    uint32_t groupCountY = ((uint32_t)config->resolution[1] + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);
    
    // Memory barrier to ensure compute shader completes
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Transition output image to SHADER_READ_ONLY_OPTIMAL for sampling
    outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          0, 0, NULL, 0, NULL, 1, &outputBarrier);
    
    // Note: Kawase blur and multiple passes would be handled by additional compute dispatches
    // For now, this implements the extraction phase. Blur passes can be added separately.
    
    return qtrue;
}

/*
===============
vk_dof_pass
===============
*/
qboolean vk_dof_pass(const dofConfig_t *config __attribute__((unused)))
{
    if (vk.dof_pipeline == VK_NULL_HANDLE || vk.dof_layout == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "DoF pipeline not initialized\n");
        return qfalse;
    }

    // Update descriptor set
    VkDescriptorImageInfo colorInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo depthInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo cocInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.coc_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo bokehInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.bokeh_sprite_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = vk.dof_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.dof_descriptor, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.dof_descriptor, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.dof_descriptor, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &cocInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.dof_descriptor, .dstBinding = 3, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &bokehInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.dof_descriptor, .dstBinding = 4, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.dof_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.dof_layout, 0, 1, &vk.dof_descriptor, 0, NULL);

    // Push constants
    dofConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, vk.dof_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dofConfig_t), &pushConstants);

    // Dispatch
    uint32_t groupCountX = (glConfig.vidWidth + 7) / 8;
    uint32_t groupCountY = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);

    return qtrue;
}

/*
===============
vk_motion_blur_pass
===============
Execute motion blur compute pass using velocity tiles
*/
qboolean vk_motion_blur_pass(const motionBlurConfig_t *config)
{
    if (vk.motion_blur_pipeline == VK_NULL_HANDLE || vk.motion_blur_layout == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Motion blur pipeline not initialized\n");
        return qfalse;
    }

    // First pass: compute velocity tiles
    if (config->useVelocityTiles) {
        VkDescriptorImageInfo velocityInfo = {
            .sampler = vk.bindless_sampler,
            .imageView = vk.velocity_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkDescriptorImageInfo depthInfo = {
            .sampler = vk.bindless_sampler,
            .imageView = vk.depth_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkDescriptorImageInfo tilesOutputInfo = {
            .sampler = VK_NULL_HANDLE,
            .imageView = vk.velocity_tiles_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        VkWriteDescriptorSet velocityWrites[] = {
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.velocity_tiles_descriptor, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &velocityInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.velocity_tiles_descriptor, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.velocity_tiles_descriptor, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &tilesOutputInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL }
        };

        qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(velocityWrites), velocityWrites, 0, NULL);

        qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.velocity_tiles_pipeline);
        qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.velocity_tiles_layout, 0, 1, &vk.velocity_tiles_descriptor, 0, NULL);

        // Push constants for velocity tiles
        struct {
            vec2_t tileResolution;
            vec2_t invTileResolution;
            vec2_t pixelsPerTile;
            vec2_t invPixelsPerTile;
        } velocityPushConstants;

        velocityPushConstants.tileResolution[0] = glConfig.vidWidth / config->tileSize[0];
        velocityPushConstants.tileResolution[1] = glConfig.vidHeight / config->tileSize[1];
        velocityPushConstants.invTileResolution[0] = 1.0f / velocityPushConstants.tileResolution[0];
        velocityPushConstants.invTileResolution[1] = 1.0f / velocityPushConstants.tileResolution[1];
        velocityPushConstants.pixelsPerTile[0] = config->tileSize[0];
        velocityPushConstants.pixelsPerTile[1] = config->tileSize[1];
        velocityPushConstants.invPixelsPerTile[0] = 1.0f / config->tileSize[0];
        velocityPushConstants.invPixelsPerTile[1] = 1.0f / config->tileSize[1];

        qvkCmdPushConstants(vk.cmd->command_buffer, vk.velocity_tiles_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(velocityPushConstants), &velocityPushConstants);

        uint32_t tileGroupCountX = (uint32_t)velocityPushConstants.tileResolution[0];
        uint32_t tileGroupCountY = (uint32_t)velocityPushConstants.tileResolution[1];
        qvkCmdDispatch(vk.cmd->command_buffer, tileGroupCountX, tileGroupCountY, 1);

        // Memory barrier
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk.velocity_tiles_image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        qvkCmdPipelineBarrier(vk.cmd->command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    }

    // Second pass: apply motion blur
    VkDescriptorImageInfo colorInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo velocityInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.velocity_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo depthInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo tilesInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = config->useVelocityTiles ? vk.velocity_tiles_image_view : VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = vk.motion_blur_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.motion_blur_descriptor, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.motion_blur_descriptor, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &velocityInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.motion_blur_descriptor, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.motion_blur_descriptor, .dstBinding = 3, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &tilesInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.motion_blur_descriptor, .dstBinding = 4, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.motion_blur_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.motion_blur_layout, 0, 1, &vk.motion_blur_descriptor, 0, NULL);

    // Push constants
    motionBlurConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, vk.motion_blur_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(motionBlurConfig_t), &pushConstants);

    uint32_t groupCountX = (glConfig.vidWidth + 7) / 8;
    uint32_t groupCountY = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);

    return qtrue;
}

/*
===============
vk_color_grading_pass
===============
Execute color grading compute pass with 3D LUT support
*/
qboolean vk_color_grading_pass(const colorGradingConfig_t *config)
{
    if (vk.color_grading_pipeline == VK_NULL_HANDLE || vk.color_grading_layout == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Color grading pipeline not initialized\n");
        return qfalse;
    }

    // Update descriptor set
    VkDescriptorImageInfo inputInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo lutInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.lut_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = vk.color_grading_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.color_grading_descriptor, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &inputInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.color_grading_descriptor, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &lutInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.color_grading_descriptor, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.color_grading_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.color_grading_layout, 0, 1, &vk.color_grading_descriptor, 0, NULL);

    // Push constants
    colorGradingConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, vk.color_grading_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(colorGradingConfig_t), &pushConstants);

    // Dispatch
    uint32_t groupCountX = (glConfig.vidWidth + 7) / 8;
    uint32_t groupCountY = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);

    return qtrue;
}

/*
===============
vk_heat_distortion_pass
===============
Execute heat distortion compute pass for atmospheric effects
*/
qboolean vk_heat_distortion_pass(const heatDistortionConfig_t *config)
{
    if (vk.heat_distortion_pipeline == VK_NULL_HANDLE || vk.heat_distortion_layout == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "Heat distortion pipeline not initialized\n");
        return qfalse;
    }

    // Update descriptor set
    VkDescriptorImageInfo colorInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.color_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo depthInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo heatMaskInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.heat_mask_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo noiseInfo = {
        .sampler = vk.bindless_sampler,
        .imageView = vk.noise_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = vk.heat_distortion_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &heatMaskInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 3, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &noiseInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 4, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.heat_distortion_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.heat_distortion_layout, 0, 1, &vk.heat_distortion_descriptor, 0, NULL);

    // Push constants
    heatDistortionConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, vk.heat_distortion_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(heatDistortionConfig_t), &pushConstants);

    // Dispatch
    uint32_t groupCountX = (glConfig.vidWidth + 7) / 8;
    uint32_t groupCountY = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);

    return qtrue;
}
