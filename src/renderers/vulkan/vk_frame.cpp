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

// Global guard for headless-present state (initialized early for cross-function visibility)
static bool g_vk_headless_present_state = false;

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
// region agent log
static void log_instrumentation(const char* hypothesisId, const char* location, const char* message, const char* data) {
  FILE* f = fopen("/home/tim/Desktop/idtech3/.cursor/debug.log", "a");
  if (!f) return;
  long long ts = (long long)time(NULL);
  fprintf(f, "{\"timestamp\":%lld,\"location\":\"%s\",\"message\":\"%s\",\"hypothesisId\":\"%s\",\"data\":%s}\n",
          ts, location, message, hypothesisId, data);
  fclose(f);
}
// #endregion
// region agent log
static inline void agent_log(const char* hypothesisId, const char* location, const char* message, const char* data) {
  FILE* f = fopen("/home/tim/Desktop/idtech3/.cursor/debug.log", "a");
  if (!f) return;
  long long ts = ri.Milliseconds();
  fprintf(f, "{\"timestamp\":%lld,\"location\":\"%s\",\"message\":\"%s\",\"hypothesisId\":\"%s\",\"data\":%s}\n", ts, location, message, hypothesisId, data);
  fclose(f);
}
// Simple fallback trace log for environments where NDJSON log isn't captured
static inline void agent_trace_log(const char* json) {
// Route instrumentation to centralized trace helper instead of direct file IO
vt_trace(json);
}
static inline void vt_trace(const char* json) {
  FILE* f = fopen("/home/tim/Desktop/idtech3/.cursor/trace.log", "a");
  if (!f) return;
  fprintf(f, "%s\n", json);
  fclose(f);
}
// end region
// (Note: headless-present guard variable now provided by existing static at vk_frame.cpp: present guard)
extern "C" void vk_begin_frame(void) {
  // region agent log
  agent_log("H1","vk_frame.cpp:vk_begin_frame","begin_frame","{}");
    VkResult result;
    uint32_t image_index;

    // Instrument: begin_frame entry
    agent_log("H1","vk_frame.cpp:vk_begin_frame","begin_frame","{}");
    // region instrumentation: begin frame entry
    agent_log("H1","vk_frame.cpp:vk_begin_frame","begin_frame","{}");
    // end region
    if (!vk_validate_handle(vk.swapchain, "swapchain")) {
        return;
    }

    // Set command buffer pointer early so we can use vk.cmd
    vk.cmd = &vk.tess[vk.cmd_index];

    // Check if we're running headless (no display available)
    static qboolean headless_detected = qfalse;
    if (!headless_detected) {
        // Try to acquire image with very short timeout first to detect headless mode
        ri.Printf(PRINT_ALL, "DEBUG: Attempting initial swapchain image acquisition (headless detection)\n");
        ri.Printf(PRINT_ALL, "DEBUG: swapchain=%p, semaphore=%p, cmd_index=%d\n",
            vk.swapchain, vk.tess[vk.cmd_index].image_acquired, vk.cmd_index);

        // Validate parameters before acquisition
        if (vk.swapchain == VK_NULL_HANDLE) {
            ri.Printf(PRINT_ERROR, "DEBUG: Swapchain handle is NULL!\n");
            // Final boundary instrumentation
            {
                char _log[64];
                snprintf(_log, sizeof(_log), "{\"final_timeout\":1,\"retry\":%d}", retry_count);
                agent_log("H1","vk_frame.cpp:vk_begin_frame","final_timeout", _log);
                vt_trace(_log);
            }
            {
                char _log2[32];
                snprintf(_log2, sizeof(_log2), "{\"headless\":1}");
                agent_log("H1","vk_frame.cpp:vk_begin_frame","headless_state", _log2);
                vt_trace(_log2);
            }
            return;
        }
        if (vk.tess[vk.cmd_index].image_acquired == VK_NULL_HANDLE) {
            ri.Printf(PRINT_ERROR, "DEBUG: Image acquired semaphore is NULL!\n");
            return;
        }
        if (vk.cmd_index >= NUM_COMMAND_BUFFERS) {
            ri.Printf(PRINT_ERROR, "DEBUG: Command index %d is out of bounds (>= %d)!\n", vk.cmd_index, NUM_COMMAND_BUFFERS);
            return;
        }

        result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, 1000000ULL, // 1ms timeout
            vk.tess[vk.cmd_index].image_acquired, VK_NULL_HANDLE, &image_index);
        {
            char data[128];
            snprintf(data, sizeof(data), "{\"result\":%d,\"image_index\":%u}", (int)result, image_index);
            agent_log("H1","vk_frame.cpp:vk_begin_frame","acquire_result", data);
        }
        ri.Printf(PRINT_ALL, "DEBUG: Initial acquisition result: %d (%s), image_index=%u\n", result, vk_result_string(result), image_index);

        if (result == VK_TIMEOUT || result == VK_NOT_READY) {
            // Likely running headless, skip rendering for this frame
            ri.Printf(PRINT_DEVELOPER, "Vulkan: Headless mode detected, skipping frame rendering\n");
        {
            char data[128];
            snprintf(data, sizeof(data), "{\"result\":%d}", (int)result);
            agent_log("H1","vk_frame.cpp:vk_begin_frame","headless_detected", data);
        }
            ri.Printf(PRINT_ALL, "DEBUG: Headless mode set due to result=%d\n", result);
            headless_detected = qtrue;
            g_vk_headless_present_state = true;
            return;
        } else if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ALL, "DEBUG: Unexpected result in headless detection: %d\n", result);
        }
    } else {
        // Already detected headless mode, continue skipping
        ri.Printf(PRINT_ALL, "DEBUG: Skipping frame due to previous headless detection\n");
        return;
    }

    // Acquire next swapchain image with retry logic
    const uint64_t timeout_ns = 1000000000ULL; // 1 second timeout
    int retry_count = 0;
    const int max_retries = 3;

    ri.Printf(PRINT_ALL, "DEBUG: Starting swapchain image acquisition with timeout=%llu ns, max_retries=%d\n", timeout_ns, max_retries);

 do {
  // region instrumentation: retry start
  {
    char _log[128];
    snprintf(_log, sizeof(_log), "{\"retry\":%d}", retry_count);
    agent_log("H1","vk_frame.cpp:vk_begin_frame","acquire_retry_start", _log);
  }
        ri.Printf(PRINT_ALL, "DEBUG: Acquisition attempt %d/%d\n", retry_count + 1, max_retries + 1);
        // instrumentation: log an acquire attempt before calling acquire
        {
            char _attempt[64];
            snprintf(_attempt, sizeof(_attempt), "{\"attempt\":%d}", retry_count);
            agent_log("H1","vk_frame.cpp:vk_begin_frame","acquire_attempt", _attempt);
            agent_trace_log(_attempt);
        }
        result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, timeout_ns,
            vk.tess[vk.cmd_index].image_acquired, VK_NULL_HANDLE, &image_index);
        {
            char data[128];
            snprintf(data, sizeof(data), "{\"retry\":%d,\"result\":%d,\"image_index\":%u}",
                     retry_count, (int)result, image_index);
            agent_log("H1","vk_frame.cpp:vk_begin_frame","acquire_retry", data);
            agent_trace_log(data);
        }
        ri.Printf(PRINT_ALL, "DEBUG: Acquisition result: %d (%s), image_index=%u\n", result, vk_result_string(result), image_index);

        if (result == VK_SUCCESS) {
            ri.Printf(PRINT_ALL, "DEBUG: Successfully acquired swapchain image %u\n", image_index);
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
            ri.Printf(PRINT_ALL, "DEBUG: Recreating swapchain due to result=%d\n", result);
            vk_recreate_swapchain();
            return;
        } else if (result == VK_TIMEOUT) {
            // Timeout - window may be minimized or display unavailable
            ri.Printf(PRINT_WARNING, "Vulkan: Timeout acquiring swapchain image, window may be minimized or display unavailable\n");
            // Buffered log
            {
                char _buf[64];
                snprintf(_buf, sizeof(_buf), "{\"retry\":%d,\"image_index\":%u}", retry_count, image_index);
                agent_log("H1","vk_frame.cpp:vk_begin_frame","timeout_detected", _buf);
            }
            if (retry_count < max_retries) {
                retry_count++;
                ri.Milliseconds();
                continue;
            }
            headless_detected = qtrue;
            {
                char _log[40];
                snprintf(_log, sizeof(_log), "{\"final_timeout\":%d}", (int)result);
                agent_log("H1","vk_frame.cpp:vk_begin_frame","final_timeout", _log);
            }
            {
                char _buf[32];
                snprintf(_buf, sizeof(_buf), "{\"result\":%d}", (int)result);
                agent_log("H1","vk_frame.cpp:vk_begin_frame","headless_detected", _buf);
            }
            {
                char _log[24];
                snprintf(_log, sizeof(_log), "{\"headless\":1}");
                agent_log("H1","vk_frame.cpp:vk_begin_frame","headless_state", _log);
            }
            {
                char _log[32];
                snprintf(_log, sizeof(_log), "{\"headless_state\":1}");
                agent_log("H1","vk_frame.cpp:vk_begin_frame","headless_state", _log);
            }
            // Additional instrumentation: indicate headless entered boundary
            {
                char _flog[40];
                snprintf(_flog, sizeof(_flog), "{\"headless_entered\":1}");
                agent_log("H1","vk_frame.cpp:vk_begin_frame","headless_entered", _flog);
            }
            return;
        } else {
            // Other error
            ri.Printf(PRINT_ERROR, "vk_begin_frame: Failed to acquire swapchain image: %s\n", vk_result_string(result));
            ri.Printf(PRINT_ALL, "DEBUG: Fatal error in swapchain acquisition: %d\n", result);
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

    ri.Printf(PRINT_ALL, "DEBUG: Successfully acquired swapchain image %u, proceeding with frame setup\n", image_index);

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
    ri.Printf(PRINT_ALL, "DEBUG: Calling vk_begin_main_render_pass\n");
    vk_begin_main_render_pass();
    ri.Printf(PRINT_ALL, "DEBUG: vk_begin_main_render_pass completed, frame %d begun\n", vk.frame_count);
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
    // region agent log: present frame entry
    agent_log("H1","vk_frame.cpp:vk_present_frame","present_frame","{}");
    // If headless, skip presenting
  if (g_vk_headless_present_state) {
        ri.Printf(PRINT_ALL, "DEBUG: headless present state active, skipping vk_present_frame\n");
        return;
  }
    ri.Printf(PRINT_ALL, "DEBUG: vk_present_frame called\n");
    {
        char _log[64];
        snprintf(_log, sizeof(_log), "{\"image_index\":%u,\"swapchain\":%p}", vk.cmd->swapchain_image_index, (void*)vk.swapchain);
        agent_log("H1","vk_frame.cpp:vk_present_frame","present_start", _log);
    }

    if (!vk_validate_handle(vk.swapchain, "swapchain")) {
        ri.Printf(PRINT_ALL, "DEBUG: Swapchain handle invalid, skipping present\n");
        return;
    }
    if (vk.device == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ALL, "DEBUG: Vulkan device invalid during present, skipping\n");
        return;
    }

    // Check if we have an acquired image to present
    if (!vk.cmd->swapchain_image_acquired) {
        ri.Printf(PRINT_ALL, "DEBUG: No swapchain image acquired, skipping present\n");
        return;
    }

  {
    char _present[64];
    snprintf(_present, sizeof(_present), "{\"present_start_image_index\":%u}", vk.cmd->swapchain_image_index);
    agent_log("H1","vk_frame.cpp:vk_present_frame","present_start", _present);
    vt_trace(_present);
  }
  ri.Printf(PRINT_ALL, "DEBUG: Presenting frame, image_index=%u, semaphore=%p\n",
        vk.cmd->swapchain_image_index, vk.cmd->rendering_finished2);

    // Wait for rendering to complete
    if (vk.cmd->waitForFence) {
        ri.Printf(PRINT_ALL, "DEBUG: Waiting for fence\n");
        VkResult result = qvkWaitForFences(vk.device, 1, &vk.cmd->rendering_finished_fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "vk_present_frame: Failed to wait for fence: %s\n", vk_result_string(result));
        } else {
            ri.Printf(PRINT_ALL, "DEBUG: Fence wait completed\n");
        }
        qvkResetFences(vk.device, 1, &vk.cmd->rendering_finished_fence);
        vk.cmd->waitForFence = qfalse;
    }

        // Present the frame
        {
            // present_start instrumentation (before actual present)
            char _log[64];
            snprintf(_log, sizeof(_log), "{\"present_start_image_index\":%u}", vk.cmd->swapchain_image_index);
            agent_log("H1","vk_frame.cpp:vk_present_frame","present_start", _log);
        }
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

    ri.Printf(PRINT_ALL, "DEBUG: Calling qvkQueuePresentKHR\n");
    VkResult result = qvkQueuePresentKHR(vk.queue, &present_info);
    ri.Printf(PRINT_ALL, "DEBUG: qvkQueuePresentKHR returned: %d (%s)\n", result, vk_result_string(result));
    {
        char pres[64];
        snprintf(pres, sizeof(pres), "{\"present_result\":%d}", (int)result);
        agent_log("H1","vk_frame.cpp:vk_present_frame","present_result", pres);
        if (result == VK_SUCCESS) {
            // clear headless flag on a successful present
            // Note: this is a local persistence; surface to main function if needed
            // no-op for safety
        }
        // mark headless_present false unless run detects headless elsewhere
        // keep a local static to gate subsequent presents
    }
    {
        char present_data[64];
        snprintf(present_data, sizeof(present_data), "{\"present_result\":%d}", (int)result);
        agent_log("H1","vk_frame.cpp:vk_present_frame","present_queue", present_data);
    }

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
