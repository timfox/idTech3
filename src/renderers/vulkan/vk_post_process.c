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
extern const char *vk_result_to_string(VkResult result);
#define SET_OBJECT_NAME(obj, name, type) vk_set_object_name((uint64_t)(obj), (name), (type))
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

// Pipeline objects for enhanced effects (extern declarations)
extern VkPipeline ssao_pipeline;
extern VkPipeline ssr_pipeline;
extern VkPipeline bloom_pipeline;
extern VkPipeline dof_pipeline;
extern VkPipeline motion_blur_pipeline;
extern VkPipeline velocity_tiles_pipeline;
extern VkPipeline color_grading_pipeline;
extern VkPipeline heat_distortion_pipeline;

// Pipeline layouts (extern declarations)
extern VkPipelineLayout ssao_layout;
extern VkPipelineLayout ssr_layout;
extern VkPipelineLayout bloom_layout;
extern VkPipelineLayout dof_layout;
extern VkPipelineLayout motion_blur_layout;
extern VkPipelineLayout velocity_tiles_layout;
extern VkPipelineLayout color_grading_layout;
extern VkPipelineLayout heat_distortion_layout;

// Descriptor set layouts (extern declarations)
extern VkDescriptorSetLayout ssao_descriptor_layout;
extern VkDescriptorSetLayout ssr_descriptor_layout;
extern VkDescriptorSetLayout bloom_descriptor_layout;
extern VkDescriptorSetLayout dof_descriptor_layout;
extern VkDescriptorSetLayout motion_blur_descriptor_layout;
extern VkDescriptorSetLayout velocity_tiles_descriptor_layout;
extern VkDescriptorSetLayout color_grading_descriptor_layout;
extern VkDescriptorSetLayout heat_distortion_descriptor_layout;

// Descriptor sets for temporary resources (extern declarations)
extern VkDescriptorSet ssao_descriptor;
extern VkDescriptorSet ssr_descriptor;
extern VkDescriptorSet bloom_descriptor;
extern VkDescriptorSet dof_descriptor;
extern VkDescriptorSet motion_blur_descriptor;
extern VkDescriptorSet velocity_tiles_descriptor;
extern VkDescriptorSet color_grading_descriptor;
extern VkDescriptorSet heat_distortion_descriptor;

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
    // Destroy pipelines
    if (ssao_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, ssao_pipeline, NULL);
        ssao_pipeline = VK_NULL_HANDLE;
    }
    if (ssr_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, ssr_pipeline, NULL);
        ssr_pipeline = VK_NULL_HANDLE;
    }
    if (bloom_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, bloom_pipeline, NULL);
        bloom_pipeline = VK_NULL_HANDLE;
    }
    if (dof_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, dof_pipeline, NULL);
        dof_pipeline = VK_NULL_HANDLE;
    }
    if (motion_blur_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, motion_blur_pipeline, NULL);
        motion_blur_pipeline = VK_NULL_HANDLE;
    }
    if (velocity_tiles_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, velocity_tiles_pipeline, NULL);
        velocity_tiles_pipeline = VK_NULL_HANDLE;
    }
    if (color_grading_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, color_grading_pipeline, NULL);
        color_grading_pipeline = VK_NULL_HANDLE;
    }
    if (heat_distortion_pipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, heat_distortion_pipeline, NULL);
        heat_distortion_pipeline = VK_NULL_HANDLE;
    }

    // Destroy pipeline layouts
    if (ssao_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, ssao_layout, NULL);
        ssao_layout = VK_NULL_HANDLE;
    }
    if (ssr_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, ssr_layout, NULL);
        ssr_layout = VK_NULL_HANDLE;
    }
    if (bloom_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, bloom_layout, NULL);
        bloom_layout = VK_NULL_HANDLE;
    }
    if (dof_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, dof_layout, NULL);
        dof_layout = VK_NULL_HANDLE;
    }
    if (motion_blur_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, motion_blur_layout, NULL);
        motion_blur_layout = VK_NULL_HANDLE;
    }
    if (velocity_tiles_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, velocity_tiles_layout, NULL);
        velocity_tiles_layout = VK_NULL_HANDLE;
    }
    if (color_grading_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, color_grading_layout, NULL);
        color_grading_layout = VK_NULL_HANDLE;
    }
    if (heat_distortion_layout != VK_NULL_HANDLE) {
        qvkDestroyPipelineLayout(vk.device, heat_distortion_layout, NULL);
        heat_distortion_layout = VK_NULL_HANDLE;
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
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create %s pipeline: %s\n", name, vk_result_to_string(result));
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &ssao_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSAO descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &ssao_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssaoConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &ssao_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSAO pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    ssao_pipeline = vk_create_compute_pipeline(vk.modules.ssao_comp, ssao_layout, "SSAO");
    return ssao_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &ssr_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSR descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &ssr_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssrConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &ssr_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create SSR pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    ssr_pipeline = vk_create_compute_pipeline(vk.modules.ssr_comp, ssr_layout, "SSR");
    return ssr_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &bloom_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create bloom descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &bloom_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bloomConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &bloom_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create bloom pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    bloom_pipeline = vk_create_compute_pipeline(vk.modules.bloom_comp, bloom_layout, "Bloom");
    return bloom_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &dof_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create DoF descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &dof_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dofConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &dof_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create DoF pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    dof_pipeline = vk_create_compute_pipeline(vk.modules.depth_of_field_comp, dof_layout, "Depth of Field");
    return dof_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &velocity_tiles_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create velocity tiles descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &velocity_tiles_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vec4_t) * 2 // tileResolution, invTileResolution, pixelsPerTile, invPixelsPerTile
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &velocity_tiles_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create velocity tiles pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    velocity_tiles_pipeline = vk_create_compute_pipeline(vk.modules.velocity_tiles_comp, velocity_tiles_layout, "Velocity Tiles");
    return velocity_tiles_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &motion_blur_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create motion blur descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &motion_blur_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(motionBlurConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &motion_blur_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create motion blur pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    motion_blur_pipeline = vk_create_compute_pipeline(vk.modules.motion_blur_comp, motion_blur_layout, "Motion Blur");
    return motion_blur_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &color_grading_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create color grading descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &color_grading_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(colorGradingConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &color_grading_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create color grading pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    color_grading_pipeline = vk_create_compute_pipeline(vk.modules.color_grading_comp, color_grading_layout, "Color Grading");
    return color_grading_pipeline != VK_NULL_HANDLE;
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

    if (qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &heat_distortion_descriptor_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create heat distortion descriptor set layout\n");
        return qfalse;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &heat_distortion_descriptor_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &(VkPushConstantRange){
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(heatDistortionConfig_t)
    };

    if (qvkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, NULL, &heat_distortion_layout) != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Failed to create heat distortion pipeline layout\n");
        return qfalse;
    }

    // Create pipeline
    heat_distortion_pipeline = vk_create_compute_pipeline(vk.modules.heat_distortion_comp, heat_distortion_layout, "Heat Distortion");
    return heat_distortion_pipeline != VK_NULL_HANDLE;
}

/*
===============
vk_create_enhanced_post_process_pipelines
===============
*/
qboolean vk_create_enhanced_post_process_pipelines(void)
{
    qboolean success = qtrue;

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
    if (ssao_pipeline == VK_NULL_HANDLE) {
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
    if (ssr_pipeline == VK_NULL_HANDLE) {
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
    if (bloom_pipeline == VK_NULL_HANDLE) {
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
    if (dof_pipeline == VK_NULL_HANDLE || dof_layout == VK_NULL_HANDLE) {
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dof_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dof_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dof_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &cocInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dof_descriptor, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &bokehInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dof_descriptor, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, dof_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, dof_layout, 0, 1, &dof_descriptor, 0, NULL);

    // Push constants
    dofConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, dof_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dofConfig_t), &pushConstants);

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
    if (motion_blur_pipeline == VK_NULL_HANDLE || motion_blur_layout == VK_NULL_HANDLE) {
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
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = velocity_tiles_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &velocityInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = velocity_tiles_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = velocity_tiles_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &tilesOutputInfo }
        };

        qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(velocityWrites), velocityWrites, 0, NULL);

        qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, velocity_tiles_pipeline);
        qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, velocity_tiles_layout, 0, 1, &velocity_tiles_descriptor, 0, NULL);

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

        qvkCmdPushConstants(vk.cmd->command_buffer, velocity_tiles_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(velocityPushConstants), &velocityPushConstants);

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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = motion_blur_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = motion_blur_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &velocityInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = motion_blur_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = motion_blur_descriptor, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &tilesInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = motion_blur_descriptor, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, motion_blur_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, motion_blur_layout, 0, 1, &motion_blur_descriptor, 0, NULL);

    // Push constants
    motionBlurConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, motion_blur_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(motionBlurConfig_t), &pushConstants);

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
    if (color_grading_pipeline == VK_NULL_HANDLE || color_grading_layout == VK_NULL_HANDLE) {
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = color_grading_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &inputInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = color_grading_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &lutInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = color_grading_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, color_grading_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, color_grading_layout, 0, 1, &color_grading_descriptor, 0, NULL);

    // Push constants
    colorGradingConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, color_grading_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(colorGradingConfig_t), &pushConstants);

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
    if (heat_distortion_pipeline == VK_NULL_HANDLE || heat_distortion_layout == VK_NULL_HANDLE) {
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
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = heat_distortion_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &colorInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = heat_distortion_descriptor, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &depthInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = heat_distortion_descriptor, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &heatMaskInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = heat_distortion_descriptor, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &noiseInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = heat_distortion_descriptor, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo }
    };

    qvkUpdateDescriptorSets(vk.device, ARRAY_LEN(writes), writes, 0, NULL);

    // Bind pipeline
    qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, heat_distortion_pipeline);
    qvkCmdBindDescriptorSets(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, heat_distortion_layout, 0, 1, &heat_distortion_descriptor, 0, NULL);

    // Push constants
    heatDistortionConfig_t pushConstants = *config;
    pushConstants.resolution[0] = glConfig.vidWidth;
    pushConstants.resolution[1] = glConfig.vidHeight;
    pushConstants.invResolution[0] = 1.0f / glConfig.vidWidth;
    pushConstants.invResolution[1] = 1.0f / glConfig.vidHeight;

    qvkCmdPushConstants(vk.cmd->command_buffer, heat_distortion_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(heatDistortionConfig_t), &pushConstants);

    // Dispatch
    uint32_t groupCountX = (glConfig.vidWidth + 7) / 8;
    uint32_t groupCountY = (glConfig.vidHeight + 7) / 8;
    qvkCmdDispatch(vk.cmd->command_buffer, groupCountX, groupCountY, 1);

    return qtrue;
}
