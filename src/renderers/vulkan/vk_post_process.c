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
    VkComputePipelineCreateInfo createInfo = {0};
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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.ssao_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSAO descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.ssao_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssaoConfig_t)
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.ssr_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSR descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.ssr_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssrConfig_t)
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.bloom_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create bloom descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.bloom_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bloomConfig_t)
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.dof_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create DoF descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.dof_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dofConfig_t)
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.velocity_tiles_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create velocity tiles descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.velocity_tiles_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vec4_t) * 2 // tileResolution, invTileResolution, pixelsPerTile, invPixelsPerTile
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.motion_blur_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create motion blur descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.motion_blur_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(motionBlurConfig_t)
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.color_grading_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create color grading descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.color_grading_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(colorGradingConfig_t)
    };

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_LEN(bindings);
    layoutInfo.pBindings = bindings;

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.heat_distortion_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create heat distortion descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &vk.heat_distortion_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(heatDistortionConfig_t)
    };

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
*/
void vk_update_post_process_config(const postProcessConfig_t *config __attribute__((unused)))
{
    // Update configuration based on CVARs
    // This would be called from the main rendering loop
}

/*
===============
vk_ssao_pass
===============
*/
qboolean vk_ssao_pass(const ssaoConfig_t *config __attribute__((unused)))
{
    // TODO: Implement full SSAO pass when Vulkan resources are available
    // For now, this is a placeholder that will be implemented in the next phase
    if (vk.ssao_pipeline == VK_NULL_HANDLE) {
        return qfalse; // Pipeline not created yet
    }
    return qtrue; // Success placeholder
}

/*
===============
vk_ssr_pass
===============
*/
qboolean vk_ssr_pass(const ssrConfig_t *config __attribute__((unused)))
{
    // TODO: Implement full SSR pass when Vulkan resources are available
    if (vk.ssr_pipeline == VK_NULL_HANDLE) {
        return qfalse; // Pipeline not created yet
    }
    return qtrue; // Success placeholder
}

/*
===============
vk_bloom_pass
===============
*/
qboolean vk_bloom_pass(const bloomConfig_t *config __attribute__((unused)))
{
    // TODO: Implement enhanced bloom pass when Vulkan resources are available
    if (vk.bloom_pipeline == VK_NULL_HANDLE) {
        return qfalse; // Pipeline not created yet
    }
    return qtrue; // Success placeholder
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.dof_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.dof_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.dof_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &cocInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.dof_descriptor, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &bokehInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.dof_descriptor, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
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
*/
qboolean vk_motion_blur_pass(const motionBlurConfig_t *config __attribute__((unused)))
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
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.velocity_tiles_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &velocityInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.velocity_tiles_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.velocity_tiles_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &tilesOutputInfo }
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.motion_blur_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.motion_blur_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &velocityInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.motion_blur_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.motion_blur_descriptor, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &tilesInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.motion_blur_descriptor, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
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
*/
qboolean vk_color_grading_pass(const colorGradingConfig_t *config __attribute__((unused)))
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.color_grading_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &inputInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.color_grading_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &lutInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.color_grading_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
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
*/
qboolean vk_heat_distortion_pass(const heatDistortionConfig_t *config __attribute__((unused)))
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &heatMaskInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &noiseInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk.heat_distortion_descriptor, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
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
