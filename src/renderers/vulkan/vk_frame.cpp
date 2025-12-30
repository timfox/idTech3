#include "tr_local.h"
#include "vk_frame.h"
#include "vk_renderpass.h"
#include "vk_utils.h"
#include "vk_postprocess.h"
#include "vk_volumetric_fog.h"
#include "vk_decals.h"
#include "vk_god_rays.h"
#include "vk_pbo.h"
#include "vk_terrain.h"
#include "vk_surface_sprites.h"
#include "vk_world_effects.h"
// Ray marching moved to RTX renderer only
#include "vk.h"

#include "vk_fsr.h"
#include "vk_atmosphere.h"

// Renderer interface
extern refimport_t ri;

// Vulkan function pointer for depth stencil clearing
extern PFN_vkCmdClearDepthStencilImage qvkCmdClearDepthStencilImage;

// Vulkan function pointer extern declarations
extern PFN_vkAcquireNextImageKHR qvkAcquireNextImageKHR;
extern PFN_vkQueuePresentKHR qvkQueuePresentKHR;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkResetFences qvkResetFences;
extern PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer qvkEndCommandBuffer;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCmdBlitImage qvkCmdBlitImage;

// External references
extern glconfig_t glConfig;
extern shaderCommands_t tess;

// Performance tracking
extern "C" void vk_update_performance_stats(void) {
    static int last_frame_time = 0;
    int current_time = ri.Milliseconds();
    int frame_time = current_time - last_frame_time;
    last_frame_time = current_time;

    if (frame_time > 0) {
        vk.performance.fps = 1000.0f / (float)frame_time;
        vk.performance.frame_time_ms = (float)frame_time;
    }

    // Update GPU timing if available
    // TODO: Implement GPU timestamp query processing
}

// Begin frame
extern "C" void vk_begin_frame(void) {
    VkResult result;
    uint32_t image_index;

    if (!vk_validate_handle(vk.swapchain, "swapchain")) {
        return;
    }

    // Set command buffer pointer early so we can use vk.cmd
    vk.cmd = &vk.tess[vk.cmd_index];

    // Check if we're running headless (no display available)
    static qboolean headless_detected = qfalse;
    if (!headless_detected) {
        // Try to acquire image with very short timeout first to detect headless mode
        result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, 1000000ULL, // 1ms timeout
            vk.tess[vk.cmd_index].image_acquired, VK_NULL_HANDLE, &image_index);

        if (result == VK_TIMEOUT || result == VK_NOT_READY) {
            // Likely running headless, skip rendering for this frame
            ri.Printf(PRINT_DEVELOPER, "Vulkan: Headless mode detected, skipping frame rendering\n");
            headless_detected = qtrue;
            return;
        }
    } else {
        // Already detected headless mode, continue skipping
        return;
    }

    // Acquire next swapchain image with retry logic
    const uint64_t timeout_ns = 1000000000ULL; // 1 second timeout
    int retry_count = 0;
    const int max_retries = 3;

    do {
        result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, timeout_ns,
            vk.tess[vk.cmd_index].image_acquired, VK_NULL_HANDLE, &image_index);

        if (result == VK_SUCCESS) {
            break; // Success!
        } else if (result == VK_NOT_READY && retry_count < max_retries) {
            // Swapchain not ready, wait a bit and retry
            ri.Printf(PRINT_DEVELOPER, "Vulkan: Swapchain not ready, retrying... (%d/%d)\n", retry_count + 1, max_retries);
            retry_count++;
            ri.Milliseconds(); // Small delay
            continue;
        } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // Swapchain needs recreation
            ri.Printf(PRINT_WARNING, "Vulkan: Swapchain needs recreation (result=%d)\n", result);
            vk_recreate_swapchain();
            return;
        } else if (result == VK_TIMEOUT) {
            // Timeout - window may be minimized or display unavailable
            ri.Printf(PRINT_WARNING, "Vulkan: Timeout acquiring swapchain image, window may be minimized or display unavailable\n");
            headless_detected = qtrue; // Mark as headless for future frames
            return;
        } else {
            // Other error
            ri.Printf(PRINT_ERROR, "vk_begin_frame: Failed to acquire swapchain image: %s\n", vk_result_string(result));
            return;
        }
    } while (retry_count < max_retries);

    // If we get here and result is not success, all retries failed
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_begin_frame: Failed to acquire swapchain image after %d retries: %s\n", max_retries, vk_result_string(result));
        return;
    }

    // Handle dynamic resolution
    if (r_dynamicResolution && r_dynamicResolution->integer) {
        float scale = 1.0f;
        if (vk.performance.fps < 60.0f) {
            scale = 0.75f;
        } else if (vk.performance.fps > 70.0f) {
            scale = 1.0f;
        }

        uint32_t targetWidth = (uint32_t)(glConfig.vidWidth * scale);
        uint32_t targetHeight = (uint32_t)(glConfig.vidHeight * scale);

        if (targetWidth != vk.renderWidth || targetHeight != vk.renderHeight) {
            vk.renderWidth = targetWidth;
            vk.renderHeight = targetHeight;
            ri.Printf(PRINT_ALL, "Vulkan: Dynamic resolution scale set to %.2f (%dx%d)\n",
                scale, vk.renderWidth, vk.renderHeight);
        }
    }

    vk.cmd->swapchain_image_index = image_index;
    vk.tess[vk.cmd_index].swapchain_image_acquired = qtrue;

    // Update performance statistics
    vk_update_performance_stats();

    // Begin command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };

    result = qvkBeginCommandBuffer(vk.tess[vk.cmd_index].command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_begin_frame: Failed to begin command buffer: %s\n", vk_result_string(result));
        return;
    }

    // Set up render area
    // vk.cmd already set above

    // Transition offscreen color image to color attachment if needed
    // (Render pass will handle this if initialLayout is UNDEFINED)

    // Begin main render pass
    vk_begin_main_render_pass();

    ri.Printf(PRINT_ALL, "Vulkan: Frame %d begun\n", vk.frame_count);
}

// End frame
extern "C" void vk_end_frame(void) {
    // Update PBO system
    vk_pbo_update();

    // Update world effects (wind/weather orchestration)
    vk_world_effects_update();

    // Update terrain system
    vk_terrain_update();

    // Update volumetric fog parameters
    vk_volumetric_fog_update();

    // Apply volumetric fog before post-processing
    vk_volumetric_fog_render(vk.cmd->command_buffer);

    // Render terrain
    vk_terrain_render();

    // Render world effects (placeholder/no-op for now)
    vk_world_effects_render();

    // Update and render surface sprites
    vk_surface_sprites_update();
    vk_surface_sprites_render();

    // Update and render decals
    vk_decals_update();
    vk_decals_render();

    // Update and render god rays
    vk_god_rays_update();
    vk_god_rays_render(vk.cmd->command_buffer);

    // Apply post-processing effects
    if (vk_has_post_processing()) {
        vk_volumetric_fog_render(vk.cmd->command_buffer);
        vk_apply_bloom();
        vk_apply_tone_mapping();
        vk_apply_gamma_correction();
    }

    // Ray marching moved to RTX renderer only

    // Apply FSR (FidelityFX Super Resolution) after post-processing but before UI
    if (vk_fsr_is_enabled()) {
        vk_fsr_update_constants(vk.renderWidth, vk.renderHeight, glConfig.vidWidth, glConfig.vidHeight);

        // 1. EASU: Upscale color_image -> upscale.image[0]
        // color_image is already in SHADER_READ_ONLY_OPTIMAL from main render pass
        vk_fsr_apply_easu(vk.cmd->command_buffer, vk.color_image, vk.color_image_view, vk.upscale.image[0], vk.upscale.view[0]);

        // 2. Barrier for EASU output -> RCAS input
        VkImageMemoryBarrier easu_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk.upscale.image[0],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        qvkCmdPipelineBarrier(vk.cmd->command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &easu_barrier);

        // 3. RCAS: Sharpen upscale.image[0] -> upscale.image[1]
        vk_fsr_apply_rcas(vk.cmd->command_buffer, vk.upscale.image[0], vk.upscale.view[0], vk.upscale.image[1], vk.upscale.view[1]);

        // 4. Barrier for RCAS output -> Transfer source
        VkImageMemoryBarrier rcas_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk.upscale.image[1],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        qvkCmdPipelineBarrier(vk.cmd->command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &rcas_barrier);

        // 5. Blit result to swapchain
        VkImageBlit blit = {};
        blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { (int32_t)glConfig.vidWidth, (int32_t)glConfig.vidHeight, 1 };
        blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { (int32_t)glConfig.vidWidth, (int32_t)glConfig.vidHeight, 1 };

        // Transition swapchain image to TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier swapchain_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk.swapchain_images[vk.cmd->swapchain_image_index],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        qvkCmdPipelineBarrier(vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &swapchain_barrier);

        qvkCmdBlitImage(vk.cmd->command_buffer, vk.upscale.image[1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        vk.swapchain_images[vk.cmd->swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit, VK_FILTER_LINEAR);

        // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL for UI
        swapchain_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapchain_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        qvkCmdPipelineBarrier(vk.cmd->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &swapchain_barrier);
    }

    // Transition swapchain image to present layout
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk.swapchain_images[vk.cmd->swapchain_image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    // End command buffer
    VkResult result = qvkEndCommandBuffer(vk.cmd->command_buffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_end_frame: Failed to end command buffer: %s\n", vk_result_string(result));
        return;
    }

    // Submit command buffer
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.cmd->image_acquired,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk.cmd->command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vk.cmd->rendering_finished2
    };

    result = qvkQueueSubmit(vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_end_frame: Failed to submit command buffer: %s\n", vk_result_string(result));
        return;
    }

    vk.cmd->waitForFence = qtrue;
    vk.frame_count++;

    // Check for memory defragmentation opportunity
    vk_check_defragmentation();

    // Update hierarchical memory pool system
    vk_update_memory_pool_system();

    // Reset frame arena for next frame
    vk_reset_frame_arena();

    // Update memory advisor
    vk_update_memory_advisor();

    // Update GPU-Async compute manager (cleanup completed jobs)
    vk_update_compute_manager();

    // End frame profiling
    vk_profile_frame_end();

    // Sample memory bandwidth and analyze access patterns
    vk_sample_memory_bandwidth();
    vk_analyze_memory_access_patterns();

    // Sample thread utilization for parallel processing analysis
    vk_sample_thread_utilization();

    ri.Printf(PRINT_ALL, "Vulkan: Frame %d ended\n", vk.frame_count);
}

// Present frame
extern "C" void vk_present_frame(void) {
    if (!vk_validate_handle(vk.swapchain, "swapchain")) {
        return;
    }

    // Wait for rendering to complete
    if (vk.cmd->waitForFence) {
        VkResult result = qvkWaitForFences(vk.device, 1, &vk.cmd->rendering_finished_fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_present_frame: Failed to wait for fence: %s\n", vk_result_string(result));
        }
        qvkResetFences(vk.device, 1, &vk.cmd->rendering_finished_fence);
        vk.cmd->waitForFence = qfalse;
    }

    // Present the frame
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.cmd->rendering_finished2,
        .swapchainCount = 1,
        .pSwapchains = &vk.swapchain,
        .pImageIndices = &vk.cmd->swapchain_image_index,
        .pResults = nullptr
    };

    VkResult result = qvkQueuePresentKHR(vk.queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Swapchain needs recreation
        ri.Printf(PRINT_WARNING, "Vulkan: Swapchain needs recreation after present (result=%d)\n", result);
        // TODO: Handle swapchain recreation
    } else if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_present_frame: Failed to present frame: %s\n", vk_result_string(result));
    }

    // Switch to next command buffer
    vk.cmd_index = (vk.cmd_index + 1) % NUM_COMMAND_BUFFERS;
    vk.cmd = &vk.tess[vk.cmd_index];

    ri.Printf(PRINT_ALL, "Vulkan: Frame %d presented\n", vk.frame_count);
}

// Resize framebuffers and resources
void vk_resize_frame_resources(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        ri.Printf(PRINT_ERROR, "vk_resize_frame_resources: Invalid dimensions %ux%u\n", width, height);
        return;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Resizing frame resources to %ux%u\n", width, height);

    // Update render dimensions
    vk.renderWidth = width;
    vk.renderHeight = height;

    // Recreate post-processing resources if needed
    if (vk_has_post_processing()) {
        vk_destroy_bloom_resources();
        if (!vk_create_bloom_resources()) {
            ri.Printf(PRINT_ERROR, "Vulkan: Failed to recreate bloom resources after resize\n");
        }
    }

    // VRS resources
    if (vk.vrs.supported) {
        vk_vrs_destroy_resources();
        vk_vrs_create_resources(width, height);
    }

    ri.Printf(PRINT_ALL, "Vulkan: Frame resources resized successfully\n");
}

// Clear color buffer
extern "C" void vk_clear_color(const vec4_t clear_color) {
    if (!vk_validate_handle(vk.cmd->command_buffer, "command buffer")) {
        return;
    }

    VkClearColorValue clear_value;
    clear_value.float32[0] = vk_sanitize_float(clear_color[0], 0.0f);
    clear_value.float32[1] = vk_sanitize_float(clear_color[1], 0.0f);
    clear_value.float32[2] = vk_sanitize_float(clear_color[2], 0.0f);
    clear_value.float32[3] = vk_sanitize_float(clear_color[3], 1.0f);

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    qvkCmdClearColorImage(vk.cmd->command_buffer, vk.swapchain_images[vk.cmd->swapchain_image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}

// Clear depth buffer
extern "C" void vk_clear_depth(qboolean clear_stencil) {
    if (!vk_validate_handle(vk.cmd->command_buffer, "command buffer")) {
        return;
    }

    VkClearDepthStencilValue clear_value = {
        .depth = 1.0f,
        .stencil = 0
    };

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    if (clear_stencil) {
        range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    qvkCmdClearDepthStencilImage(vk.cmd->command_buffer, vk.depth_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}

// Read pixels from framebuffer
void vk_read_pixels(byte *buffer, uint32_t width, uint32_t height) {
    if (!buffer || width == 0 || height == 0) {
        ri.Printf(PRINT_ERROR, "vk_read_pixels: Invalid parameters\n");
        return;
    }

    // This would implement reading pixels from the current framebuffer
    // Implementation depends on specific requirements and may involve
    // copying from GPU to CPU memory

    ri.Printf(PRINT_ALL, "Vulkan: Read pixels %ux%u (stub implementation)\n", width, height);
    // TODO: Implement actual pixel reading
    Com_Memset(buffer, 0, width * height * 4); // Placeholder
}
