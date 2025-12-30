/*
=============================================================================
Vulkan Ultra-Advanced Post-Processing Effects
=============================================================================
*/

#include "tr_local.h"
#include <cmath>
#include <random>

#ifdef USE_VULKAN

// External Vulkan objects
extern VkDevice vk_device;
extern VkPhysicalDevice vk_physical_device;
extern VkCommandPool vk_command_pool;

// Vulkan function pointers
extern PFN_vkCreateComputePipelines qvkCreateComputePipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
extern PFN_vkCreateShaderModule qvkCreateShaderModule;
extern PFN_vkDestroyShaderModule qvkDestroyShaderModule;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
extern PFN_vkCmdDispatch qvkCmdDispatch;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants qvkCmdPushConstants;

// Ultra-advanced post-processing effects structure
typedef struct {
    // Film grain effect
    qboolean film_grain_enabled;
    float film_grain_strength;
    float film_grain_size;
    uint32_t film_grain_seed;

    // Chromatic aberration
    qboolean chromatic_aberration_enabled;
    float chromatic_aberration_strength;
    float chromatic_aberration_spectral_offset;

    // Heat distortion
    qboolean heat_distortion_enabled;
    float heat_distortion_strength;
    float heat_distortion_speed;
    float heat_distortion_frequency;

    // Motion blur
    qboolean motion_blur_enabled;
    float motion_blur_strength;
    int motion_blur_samples;
    float motion_blur_max_velocity;

    // Vignette
    qboolean vignette_enabled;
    float vignette_strength;
    float vignette_softness;
    vec3_t vignette_color;

    // Color fringing
    qboolean color_fringing_enabled;
    float color_fringing_strength;
    float color_fringing_angle;

    // Lens distortion
    qboolean lens_distortion_enabled;
    float lens_distortion_strength;
    float lens_distortion_zoom;

    // Lens flare
    qboolean lens_flare_enabled;
    float lens_flare_strength;
    int lens_flare_ghosts;
    vec3_t lens_flare_color;

    // Auto exposure
    qboolean auto_exposure_enabled;
    float auto_exposure_speed;
    float auto_exposure_min;
    float auto_exposure_max;
    float current_exposure;

    // Temporal anti-aliasing
    qboolean temporal_aa_enabled;
    float temporal_aa_strength;
    int temporal_aa_samples;

    // Sharpening
    qboolean sharpen_enabled;
    float sharpen_strength;
    float sharpen_clamp;

    // Dithering
    qboolean dithering_enabled;
    float dithering_strength;
    int dithering_pattern; // 0=bayer, 1=blue_noise, 2=white_noise

    // Pipeline objects
    VkPipeline film_grain_pipeline;
    VkPipeline chromatic_aberration_pipeline;
    VkPipeline heat_distortion_pipeline;
    VkPipeline motion_blur_pipeline;
    VkPipeline vignette_pipeline;
    VkPipeline color_fringing_pipeline;
    VkPipeline lens_distortion_pipeline;
    VkPipeline lens_flare_pipeline;
    VkPipeline auto_exposure_pipeline;
    VkPipeline temporal_aa_pipeline;
    VkPipeline sharpen_pipeline;
    VkPipeline dithering_pipeline;

    VkPipelineLayout film_grain_layout;
    VkPipelineLayout chromatic_aberration_layout;
    VkPipelineLayout heat_distortion_layout;
    VkPipelineLayout motion_blur_layout;
    VkPipelineLayout vignette_layout;
    VkPipelineLayout color_fringing_layout;
    VkPipelineLayout lens_distortion_layout;
    VkPipelineLayout lens_flare_layout;
    VkPipelineLayout auto_exposure_layout;
    VkPipelineLayout temporal_aa_layout;
    VkPipelineLayout sharpen_layout;
    VkPipelineLayout dithering_layout;

    VkDescriptorSetLayout film_grain_desc_layout;
    VkDescriptorSetLayout chromatic_aberration_desc_layout;
    VkDescriptorSetLayout heat_distortion_desc_layout;
    VkDescriptorSetLayout motion_blur_desc_layout;
    VkDescriptorSetLayout vignette_desc_layout;
    VkDescriptorSetLayout color_fringing_desc_layout;
    VkDescriptorSetLayout lens_distortion_desc_layout;
    VkDescriptorSetLayout lens_flare_desc_layout;
    VkDescriptorSetLayout auto_exposure_desc_layout;
    VkDescriptorSetLayout temporal_aa_desc_layout;
    VkDescriptorSetLayout sharpen_desc_layout;
    VkDescriptorSetLayout dithering_desc_layout;

    // Noise textures and buffers
    VkImage film_grain_noise_image;
    VkDeviceMemory film_grain_noise_memory;
    VkImageView film_grain_noise_view;

    VkBuffer motion_blur_velocity_buffer;
    VkDeviceMemory motion_blur_velocity_memory;

    VkBuffer auto_exposure_luminance_buffer;
    VkDeviceMemory auto_exposure_luminance_memory;

    VkImage temporal_aa_history_image;
    VkDeviceMemory temporal_aa_history_memory;
    VkImageView temporal_aa_history_view;

    // Random number generator for effects
    std::mt19937_64 rng;
    std::uniform_real_distribution<float> uniform_dist;

    // Performance tracking
    uint64_t total_effects_time;
    uint32_t effects_applied_count;

} vk_ultra_post_process_t;

static vk_ultra_post_process_t ultra_pp = {0};

// CVAR definitions
cvar_t *r_pp_ultra_film_grain;
cvar_t *r_pp_ultra_film_grain_strength;
cvar_t *r_pp_ultra_chromatic_aberration;
cvar_t *r_pp_ultra_chromatic_aberration_strength;
cvar_t *r_pp_ultra_heat_distortion;
cvar_t *r_pp_ultra_heat_distortion_strength;
cvar_t *r_pp_ultra_motion_blur;
cvar_t *r_pp_ultra_motion_blur_strength;
cvar_t *r_pp_ultra_vignette;
cvar_t *r_pp_ultra_vignette_strength;
cvar_t *r_pp_ultra_color_fringing;
cvar_t *r_pp_ultra_lens_distortion;
cvar_t *r_pp_ultra_lens_flare;
cvar_t *r_pp_ultra_auto_exposure;
cvar_t *r_pp_ultra_temporal_aa;
cvar_t *r_pp_ultra_sharpen;
cvar_t *r_pp_ultra_dithering;

// Initialize ultra-advanced post-processing
qboolean vk_ultra_post_process_init(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing ultra-advanced post-processing effects\n");

    // Register CVARs
    r_pp_ultra_film_grain = ri.Cvar_Get("r_pp_ultra_film_grain", "1", CVAR_ARCHIVE);
    r_pp_ultra_film_grain_strength = ri.Cvar_Get("r_pp_ultra_film_grain_strength", "0.1", CVAR_ARCHIVE);
    r_pp_ultra_chromatic_aberration = ri.Cvar_Get("r_pp_ultra_chromatic_aberration", "1", CVAR_ARCHIVE);
    r_pp_ultra_chromatic_aberration_strength = ri.Cvar_Get("r_pp_ultra_chromatic_aberration_strength", "0.002", CVAR_ARCHIVE);
    r_pp_ultra_heat_distortion = ri.Cvar_Get("r_pp_ultra_heat_distortion", "0", CVAR_ARCHIVE);
    r_pp_ultra_heat_distortion_strength = ri.Cvar_Get("r_pp_ultra_heat_distortion_strength", "0.05", CVAR_ARCHIVE);
    r_pp_ultra_motion_blur = ri.Cvar_Get("r_pp_ultra_motion_blur", "1", CVAR_ARCHIVE);
    r_pp_ultra_motion_blur_strength = ri.Cvar_Get("r_pp_ultra_motion_blur_strength", "0.8", CVAR_ARCHIVE);
    r_pp_ultra_vignette = ri.Cvar_Get("r_pp_ultra_vignette", "1", CVAR_ARCHIVE);
    r_pp_ultra_vignette_strength = ri.Cvar_Get("r_pp_ultra_vignette_strength", "0.3", CVAR_ARCHIVE);
    r_pp_ultra_color_fringing = ri.Cvar_Get("r_pp_ultra_color_fringing", "0", CVAR_ARCHIVE);
    r_pp_ultra_lens_distortion = ri.Cvar_Get("r_pp_ultra_lens_distortion", "0", CVAR_ARCHIVE);
    r_pp_ultra_lens_flare = ri.Cvar_Get("r_pp_ultra_lens_flare", "1", CVAR_ARCHIVE);
    r_pp_ultra_auto_exposure = ri.Cvar_Get("r_pp_ultra_auto_exposure", "1", CVAR_ARCHIVE);
    r_pp_ultra_temporal_aa = ri.Cvar_Get("r_pp_ultra_temporal_aa", "1", CVAR_ARCHIVE);
    r_pp_ultra_sharpen = ri.Cvar_Get("r_pp_ultra_sharpen", "1", CVAR_ARCHIVE);
    r_pp_ultra_dithering = ri.Cvar_Get("r_pp_ultra_dithering", "1", CVAR_ARCHIVE);

    // Initialize effect parameters
    ultra_pp.film_grain_enabled = r_pp_ultra_film_grain->integer != 0;
    ultra_pp.film_grain_strength = r_pp_ultra_film_grain_strength->value;
    ultra_pp.film_grain_size = 1.0f;
    ultra_pp.film_grain_seed = 0;

    ultra_pp.chromatic_aberration_enabled = r_pp_ultra_chromatic_aberration->integer != 0;
    ultra_pp.chromatic_aberration_strength = r_pp_ultra_chromatic_aberration_strength->value;
    ultra_pp.chromatic_aberration_spectral_offset = 0.001f;

    ultra_pp.heat_distortion_enabled = r_pp_ultra_heat_distortion->integer != 0;
    ultra_pp.heat_distortion_strength = r_pp_ultra_heat_distortion_strength->value;
    ultra_pp.heat_distortion_speed = 1.0f;
    ultra_pp.heat_distortion_frequency = 10.0f;

    ultra_pp.motion_blur_enabled = r_pp_ultra_motion_blur->integer != 0;
    ultra_pp.motion_blur_strength = r_pp_ultra_motion_blur_strength->value;
    ultra_pp.motion_blur_samples = 16;
    ultra_pp.motion_blur_max_velocity = 50.0f;

    ultra_pp.vignette_enabled = r_pp_ultra_vignette->integer != 0;
    ultra_pp.vignette_strength = r_pp_ultra_vignette_strength->value;
    ultra_pp.vignette_softness = 0.5f;
    VectorSet(ultra_pp.vignette_color, 0.0f, 0.0f, 0.0f);

    ultra_pp.color_fringing_enabled = r_pp_ultra_color_fringing->integer != 0;
    ultra_pp.color_fringing_strength = 0.005f;
    ultra_pp.color_fringing_angle = 0.0f;

    ultra_pp.lens_distortion_enabled = r_pp_ultra_lens_distortion->integer != 0;
    ultra_pp.lens_distortion_strength = 0.1f;
    ultra_pp.lens_distortion_zoom = 1.0f;

    ultra_pp.lens_flare_enabled = r_pp_ultra_lens_flare->integer != 0;
    ultra_pp.lens_flare_strength = 0.3f;
    ultra_pp.lens_flare_ghosts = 5;
    VectorSet(ultra_pp.lens_flare_color, 1.0f, 0.9f, 0.7f);

    ultra_pp.auto_exposure_enabled = r_pp_ultra_auto_exposure->integer != 0;
    ultra_pp.auto_exposure_speed = 1.0f;
    ultra_pp.auto_exposure_min = 0.1f;
    ultra_pp.auto_exposure_max = 2.0f;
    ultra_pp.current_exposure = 1.0f;

    ultra_pp.temporal_aa_enabled = r_pp_ultra_temporal_aa->integer != 0;
    ultra_pp.temporal_aa_strength = 0.5f;
    ultra_pp.temporal_aa_samples = 8;

    ultra_pp.sharpen_enabled = r_pp_ultra_sharpen->integer != 0;
    ultra_pp.sharpen_strength = 0.3f;
    ultra_pp.sharpen_clamp = 0.1f;

    ultra_pp.dithering_enabled = r_pp_ultra_dithering->integer != 0;
    ultra_pp.dithering_strength = 0.5f;
    ultra_pp.dithering_pattern = 0; // Bayer matrix

    // Initialize random number generator
    ultra_pp.rng.seed(std::random_device{}());
    ultra_pp.uniform_dist = std::uniform_real_distribution<float>(0.0f, 1.0f);

    // Create compute pipelines for effects
    if (!vk_create_ultra_pp_pipelines()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create ultra post-processing pipelines\n");
        return qfalse;
    }

    // Create resources
    if (!vk_create_ultra_pp_resources()) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create ultra post-processing resources\n");
        return qfalse;
    }

    ultra_pp.total_effects_time = 0;
    ultra_pp.effects_applied_count = 0;

    ri.Printf(PRINT_ALL, "Vulkan: Ultra-advanced post-processing initialized\n");
    return qtrue;
}

// Shutdown ultra-advanced post-processing
void vk_ultra_post_process_shutdown(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down ultra-advanced post-processing\n");

    // Destroy pipelines
    if (ultra_pp.film_grain_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.film_grain_pipeline, nullptr);
    if (ultra_pp.chromatic_aberration_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.chromatic_aberration_pipeline, nullptr);
    if (ultra_pp.heat_distortion_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.heat_distortion_pipeline, nullptr);
    if (ultra_pp.motion_blur_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.motion_blur_pipeline, nullptr);
    if (ultra_pp.vignette_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.vignette_pipeline, nullptr);
    if (ultra_pp.lens_flare_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.lens_flare_pipeline, nullptr);
    if (ultra_pp.sharpen_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.sharpen_pipeline, nullptr);
    if (ultra_pp.dithering_pipeline) qvkDestroyPipeline(vk_device, ultra_pp.dithering_pipeline, nullptr);

    // Destroy pipeline layouts
    if (ultra_pp.film_grain_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.film_grain_layout, nullptr);
    if (ultra_pp.chromatic_aberration_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.chromatic_aberration_layout, nullptr);
    if (ultra_pp.heat_distortion_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.heat_distortion_layout, nullptr);
    if (ultra_pp.motion_blur_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.motion_blur_layout, nullptr);
    if (ultra_pp.vignette_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.vignette_layout, nullptr);
    if (ultra_pp.lens_flare_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.lens_flare_layout, nullptr);
    if (ultra_pp.sharpen_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.sharpen_layout, nullptr);
    if (ultra_pp.dithering_layout) qvkDestroyPipelineLayout(vk_device, ultra_pp.dithering_layout, nullptr);

    // Destroy descriptor set layouts
    if (ultra_pp.film_grain_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.film_grain_desc_layout, nullptr);
    if (ultra_pp.chromatic_aberration_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.chromatic_aberration_desc_layout, nullptr);
    if (ultra_pp.heat_distortion_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.heat_distortion_desc_layout, nullptr);
    if (ultra_pp.motion_blur_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.motion_blur_desc_layout, nullptr);
    if (ultra_pp.vignette_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.vignette_desc_layout, nullptr);
    if (ultra_pp.lens_flare_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.lens_flare_desc_layout, nullptr);
    if (ultra_pp.sharpen_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.sharpen_desc_layout, nullptr);
    if (ultra_pp.dithering_desc_layout) qvkDestroyDescriptorSetLayout(vk_device, ultra_pp.dithering_desc_layout, nullptr);

    // Destroy resources
    vk_destroy_ultra_pp_resources();

    ri.Printf(PRINT_ALL, "Vulkan: Ultra-advanced post-processing shutdown complete\n");
}

// Apply all enabled ultra post-processing effects
void vk_apply_ultra_post_processing(VkCommandBuffer cmd_buffer, VkImage input_image, VkImageView input_view,
                                   VkImage output_image, VkImageView output_view, uint32_t width, uint32_t height) {
    uint64_t start_time = ri.Milliseconds();

    // Film grain effect
    if (ultra_pp.film_grain_enabled && ultra_pp.film_grain_strength > 0.001f) {
        vk_apply_film_grain(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Chromatic aberration
    if (ultra_pp.chromatic_aberration_enabled && ultra_pp.chromatic_aberration_strength > 0.0001f) {
        vk_apply_chromatic_aberration(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Heat distortion
    if (ultra_pp.heat_distortion_enabled && ultra_pp.heat_distortion_strength > 0.001f) {
        vk_apply_heat_distortion(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Motion blur
    if (ultra_pp.motion_blur_enabled && ultra_pp.motion_blur_strength > 0.001f) {
        vk_apply_motion_blur(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Vignette
    if (ultra_pp.vignette_enabled && ultra_pp.vignette_strength > 0.001f) {
        vk_apply_vignette(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Color fringing
    if (ultra_pp.color_fringing_enabled && ultra_pp.color_fringing_strength > 0.0001f) {
        vk_apply_color_fringing(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Lens flare
    if (ultra_pp.lens_flare_enabled && ultra_pp.lens_flare_strength > 0.001f) {
        vk_apply_lens_flare(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Sharpening
    if (ultra_pp.sharpen_enabled && ultra_pp.sharpen_strength > 0.001f) {
        vk_apply_sharpen(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    // Dithering (applied last)
    if (ultra_pp.dithering_enabled && ultra_pp.dithering_strength > 0.001f) {
        vk_apply_dithering(cmd_buffer, input_image, input_view, output_image, output_view, width, height);
    }

    uint64_t end_time = ri.Milliseconds();
    ultra_pp.total_effects_time += (end_time - start_time);
    ultra_pp.effects_applied_count++;
}

// Update effect parameters (called every frame)
void vk_update_ultra_pp_parameters(float delta_time) {
    // Update time-based effects
    static float time_accumulator = 0.0f;
    time_accumulator += delta_time;

    // Update film grain seed periodically
    if (fmodf(time_accumulator, 0.1f) < delta_time) {
        ultra_pp.film_grain_seed = static_cast<uint32_t>(time_accumulator * 1000.0f);
    }

    // Update heat distortion animation
    ultra_pp.heat_distortion_frequency = 8.0f + sinf(time_accumulator * 0.5f) * 2.0f;

    // Update auto exposure
    if (ultra_pp.auto_exposure_enabled) {
        // This would sample luminance from the scene and adjust exposure
        float target_exposure = 1.0f; // Placeholder - would be computed from scene luminance
        float exposure_change = (target_exposure - ultra_pp.current_exposure) * ultra_pp.auto_exposure_speed * delta_time;
        ultra_pp.current_exposure += exposure_change;
        ultra_pp.current_exposure = Com_Clamp(ultra_pp.auto_exposure_min, ultra_pp.auto_exposure_max, ultra_pp.current_exposure);
    }
}

// Get performance statistics
void vk_get_ultra_pp_stats(uint32_t *effects_count, uint64_t *total_time, float *avg_time_per_frame) {
    if (effects_count) *effects_count = ultra_pp.effects_applied_count;
    if (total_time) *total_time = ultra_pp.total_effects_time;
    if (avg_time_per_frame && ultra_pp.effects_applied_count > 0) {
        *avg_time_per_frame = static_cast<float>(ultra_pp.total_effects_time) / ultra_pp.effects_applied_count;
    }
}

// ============================================================================
// Implementation Functions
// ============================================================================

// Create compute pipelines for ultra post-processing effects
static qboolean vk_create_ultra_pp_pipelines(void) {
    // Create descriptor set layouts
    VkDescriptorSetLayoutBinding bindings[4] = {};

    // Common bindings for most effects: input texture, output image, sampler, uniform buffer
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 4;
    layout_info.pBindings = bindings;

    // Create descriptor set layouts for each effect
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.film_grain_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.chromatic_aberration_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.heat_distortion_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.motion_blur_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.vignette_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.lens_flare_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.sharpen_desc_layout) != VK_SUCCESS) return qfalse;
    if (qvkCreateDescriptorSetLayout(vk_device, &layout_info, nullptr, &ultra_pp.dithering_desc_layout) != VK_SUCCESS) return qfalse;

    // Create pipeline layouts
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &ultra_pp.film_grain_desc_layout;

    VkPushConstantRange push_constants = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float) * 8};
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constants;

    // Create pipeline layouts for each effect
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.film_grain_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.chromatic_aberration_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.chromatic_aberration_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.heat_distortion_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.heat_distortion_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.motion_blur_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.motion_blur_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.vignette_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.vignette_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.lens_flare_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.lens_flare_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.sharpen_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.sharpen_layout) != VK_SUCCESS) return qfalse;
    pipeline_layout_info.pSetLayouts = &ultra_pp.dithering_desc_layout;
    if (qvkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &ultra_pp.dithering_layout) != VK_SUCCESS) return qfalse;

    // TODO: Load compute shaders and create pipelines
    // For now, pipelines are set to VK_NULL_HANDLE and effects will be skipped

    return qtrue;
}

// Create resources needed for ultra post-processing effects
static qboolean vk_create_ultra_pp_resources(void) {
    // Create film grain noise texture
    if (ultra_pp.film_grain_enabled) {
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R8_UNORM;
        image_info.extent = {512, 512, 1};
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (qvkCreateImage(vk_device, &image_info, nullptr, &ultra_pp.film_grain_noise_image) != VK_SUCCESS) return qfalse;

        // Allocate and bind memory (simplified)
        VkMemoryRequirements mem_reqs;
        qvkGetImageMemoryRequirements(vk_device, ultra_pp.film_grain_noise_image, &mem_reqs);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = 0; // TODO: Find proper memory type

        if (qvkAllocateMemory(vk_device, &alloc_info, nullptr, &ultra_pp.film_grain_noise_memory) != VK_SUCCESS) return qfalse;
        if (qvkBindImageMemory(vk_device, ultra_pp.film_grain_noise_image, ultra_pp.film_grain_noise_memory, 0) != VK_SUCCESS) return qfalse;

        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = ultra_pp.film_grain_noise_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8_UNORM;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        if (qvkCreateImageView(vk_device, &view_info, nullptr, &ultra_pp.film_grain_noise_view) != VK_SUCCESS) return qfalse;
    }

    return qtrue;
}

// Destroy resources for ultra post-processing effects
static void vk_destroy_ultra_pp_resources(void) {
    if (ultra_pp.film_grain_noise_view) qvkDestroyImageView(vk_device, ultra_pp.film_grain_noise_view, nullptr);
    if (ultra_pp.film_grain_noise_image) qvkDestroyImage(vk_device, ultra_pp.film_grain_noise_image, nullptr);
    if (ultra_pp.film_grain_noise_memory) qvkFreeMemory(vk_device, ultra_pp.film_grain_noise_memory, nullptr);

    if (ultra_pp.motion_blur_velocity_buffer) qvkDestroyBuffer(vk_device, ultra_pp.motion_blur_velocity_buffer, nullptr);
    if (ultra_pp.motion_blur_velocity_memory) qvkFreeMemory(vk_device, ultra_pp.motion_blur_velocity_memory, nullptr);

    if (ultra_pp.auto_exposure_luminance_buffer) qvkDestroyBuffer(vk_device, ultra_pp.auto_exposure_luminance_buffer, nullptr);
    if (ultra_pp.auto_exposure_luminance_memory) qvkFreeMemory(vk_device, ultra_pp.auto_exposure_luminance_memory, nullptr);

    if (ultra_pp.temporal_aa_history_view) qvkDestroyImageView(vk_device, ultra_pp.temporal_aa_history_view, nullptr);
    if (ultra_pp.temporal_aa_history_image) qvkDestroyImage(vk_device, ultra_pp.temporal_aa_history_image, nullptr);
    if (ultra_pp.temporal_aa_history_memory) qvkFreeMemory(vk_device, ultra_pp.temporal_aa_history_memory, nullptr);
}

// ============================================================================
// Individual Effect Implementations
// ============================================================================

static void vk_apply_film_grain(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                                VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.film_grain_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.film_grain_pipeline);

    // Bind descriptors and push constants
    float push_constants[8] = {
        ultra_pp.film_grain_strength,
        ultra_pp.film_grain_size,
        static_cast<float>(ultra_pp.film_grain_seed),
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.film_grain_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    // TODO: Bind descriptor sets

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);

    // Add barrier
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image = output;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    qvkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static void vk_apply_chromatic_aberration(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                                         VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.chromatic_aberration_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.chromatic_aberration_pipeline);

    float push_constants[8] = {
        ultra_pp.chromatic_aberration_strength,
        ultra_pp.chromatic_aberration_spectral_offset,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 0.0f, 0.0f, 0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.chromatic_aberration_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

static void vk_apply_heat_distortion(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                                    VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.heat_distortion_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.heat_distortion_pipeline);

    float time = static_cast<float>(ri.Milliseconds()) / 1000.0f;
    float push_constants[8] = {
        ultra_pp.heat_distortion_strength,
        ultra_pp.heat_distortion_speed,
        ultra_pp.heat_distortion_frequency,
        time,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.heat_distortion_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

static void vk_apply_motion_blur(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                                VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.motion_blur_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.motion_blur_pipeline);

    float push_constants[8] = {
        ultra_pp.motion_blur_strength,
        static_cast<float>(ultra_pp.motion_blur_samples),
        ultra_pp.motion_blur_max_velocity,
        0.0f, // Would be filled with velocity vectors
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.motion_blur_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

static void vk_apply_vignette(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                             VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.vignette_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.vignette_pipeline);

    float push_constants[8] = {
        ultra_pp.vignette_strength,
        ultra_pp.vignette_softness,
        ultra_pp.vignette_color[0],
        ultra_pp.vignette_color[1],
        ultra_pp.vignette_color[2],
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.vignette_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

static void vk_apply_color_fringing(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                                   VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    // Color fringing is a simpler effect that can be combined with chromatic aberration
    vk_apply_chromatic_aberration(cmd_buffer, input, input_view, output, output_view, width, height);
}

static void vk_apply_lens_flare(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                               VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.lens_flare_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.lens_flare_pipeline);

    float push_constants[8] = {
        ultra_pp.lens_flare_strength,
        static_cast<float>(ultra_pp.lens_flare_ghosts),
        ultra_pp.lens_flare_color[0],
        ultra_pp.lens_flare_color[1],
        ultra_pp.lens_flare_color[2],
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.lens_flare_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

static void vk_apply_sharpen(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                            VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.sharpen_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.sharpen_pipeline);

    float push_constants[8] = {
        ultra_pp.sharpen_strength,
        ultra_pp.sharpen_clamp,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 0.0f, 0.0f, 0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.sharpen_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

static void vk_apply_dithering(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                              VkImage output, VkImageView output_view, uint32_t width, uint32_t height) {
    if (!ultra_pp.dithering_pipeline) return;

    qvkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ultra_pp.dithering_pipeline);

    float push_constants[8] = {
        ultra_pp.dithering_strength,
        static_cast<float>(ultra_pp.dithering_pattern),
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 0.0f, 0.0f, 0.0f
    };
    qvkCmdPushConstants(cmd_buffer, ultra_pp.dithering_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);

    uint32_t group_count_x = (width + 15) / 16;
    uint32_t group_count_y = (height + 15) / 16;
    qvkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
}

#endif // USE_VULKAN