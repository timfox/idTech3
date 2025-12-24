#include "vk_postprocess.h"
#include "vk_renderpass.h"
#include "vk_utils.h"
#include "vk_images.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"

// Renderer interface
extern refimport_t ri;

// CVAR externs
extern cvar_t *r_bloom;
extern cvar_t *r_postQuality;

// Utility functions
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

// Vulkan function pointer extern declarations
extern PFN_vkCreatePipeline qvkCreatePipeline;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdDispatch qvkCmdDispatch;

// Forward declarations
static void vk_create_bloom_extract_pipeline(void);
static void vk_create_blur_pipelines(void);
static void vk_update_bloom_descriptors(void);

// Initialize post-processing system
qboolean vk_init_post_processing(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing post-processing system\n");

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
void vk_shutdown_post_processing(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Shutting down post-processing system\n");

    // Destroy bloom resources
    vk_destroy_bloom_resources();

    ri.Printf(PRINT_ALL, "Vulkan: Post-processing system shut down\n");
}

// Create bloom resources
qboolean vk_create_bloom_resources(void) {
    uint32_t width = vk.renderWidth;
    uint32_t height = vk.renderHeight;

    // Create bloom extract image
    if (!vk_create_image(&vk.bloomExtractImage, width, height, 1)) {
        ri.Printf(PRINT_ERROR, "vk_create_bloom_resources: Failed to create bloom extract image\n");
        return qfalse;
    }

    vk_create_image_view(&vk.bloomExtractImage, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create blur images (ping-pong)
    for (int i = 0; i < ARRAY_LEN(vk.blurImages); i++) {
        if (!vk_create_image(&vk.blurImages[i], width, height, 1)) {
            ri.Printf(PRINT_ERROR, "vk_create_bloom_resources: Failed to create blur image %d\n", i);
            return qfalse;
        }

        vk_create_image_view(&vk.blurImages[i], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    ri.Printf(PRINT_ALL, "Vulkan: Bloom resources created successfully\n");
    return qtrue;
}

// Destroy bloom resources
void vk_destroy_bloom_resources(void) {
    // Destroy blur images
    for (int i = 0; i < ARRAY_LEN(vk.blurImages); i++) {
        vk_destroy_image(&vk.blurImages[i]);
    }

    // Destroy bloom extract image
    vk_destroy_image(&vk.bloomExtractImage);

    // Destroy pipelines
    if (vk.bloomExtractPipeline != VK_NULL_HANDLE) {
        qvkDestroyPipeline(vk.device, vk.bloomExtractPipeline, NULL);
        vk.bloomExtractPipeline = VK_NULL_HANDLE;
    }

    for (int i = 0; i < ARRAY_LEN(vk.blurPipelines); i++) {
        if (vk.blurPipelines[i] != VK_NULL_HANDLE) {
            qvkDestroyPipeline(vk.device, vk.blurPipelines[i], NULL);
            vk.blurPipelines[i] = VK_NULL_HANDLE;
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
    for (int i = 0; i < ARRAY_LEN(vk.blurPipelines); i++) {
        qboolean horizontal = (i % 2 == 0);
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
void vk_apply_bloom(void) {
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
void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass) {
    if (!vk_bounds_check(index, ARRAY_LEN(vk.blurPipelines), "blur pipeline")) {
        return;
    }

    // Create compute pipeline for blur effect
    // This is a placeholder - actual implementation would create the pipeline
    ri.Printf(PRINT_ALL, "Vulkan: Creating blur pipeline %u (%s, %ux%u)\n",
        index, horizontal_pass ? "horizontal" : "vertical", width, height);
}

// Update post-processing pipelines
void vk_update_post_process_pipelines(void) {
    // Update pipelines based on current settings
    if (r_bloom && r_bloom->integer) {
        vk_update_bloom_descriptors();
    }

    ri.Printf(PRINT_ALL, "Vulkan: Post-processing pipelines updated\n");
}

// Apply tone mapping
void vk_apply_tone_mapping(void) {
    // Apply HDR tone mapping if enabled
    // This would bind appropriate pipeline and dispatch compute shader
    ri.Printf(PRINT_ALL, "Vulkan: Tone mapping applied (stub)\n");
}

// Apply gamma correction
void vk_apply_gamma_correction(void) {
    // Apply gamma correction to final image
    ri.Printf(PRINT_ALL, "Vulkan: Gamma correction applied (stub)\n");
}

// Post-processing quality settings
int vk_get_post_process_quality(void) {
    return r_postQuality ? CLAMP(0, 4, r_postQuality->integer) : 2;
}

// Check if post-processing is enabled
qboolean vk_has_post_processing(void) {
    return (r_bloom && r_bloom->integer) || vk_get_post_process_quality() > 0;
}
