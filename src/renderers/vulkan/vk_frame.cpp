#include "vk_frame.h"
#include "vk_renderpass.h"
#include "vk_utils.h"
#include "vk_postprocess.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"

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

// External references
extern shaderCommands_t tess;

// Performance tracking
extern void vk_update_performance_stats(void);

// Begin frame
extern "C" void vk_begin_frame(void) {
    VkResult result;
    uint32_t image_index;

    if (!vk_validate_handle(vk.swapchain, "swapchain")) {
        return;
    }

    // Acquire next swapchain image
    result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
        vk.tess[vk.cmd_index].image_acquired, VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Swapchain needs recreation
        ri.Printf(PRINT_WARNING, "Vulkan: Swapchain needs recreation (result=%d)\n", result);
        // TODO: Handle swapchain recreation
        return;
    } else if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "vk_begin_frame: Failed to acquire swapchain image: %s\n", vk_result_string(result));
        return;
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
    vk.cmd = &vk.tess[vk.cmd_index];

    // Transition swapchain image to color attachment
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk.swapchain_images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    qvkCmdPipelineBarrier(vk.cmd->command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

    ri.Printf(PRINT_ALL, "Vulkan: Frame %d begun\n", vk.frame_count);
}

// End frame
extern "C" void vk_end_frame(void) {
    // Apply post-processing effects
    if (vk_has_post_processing()) {
        vk_apply_bloom();
        vk_apply_tone_mapping();
        vk_apply_gamma_correction();
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
