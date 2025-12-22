#include "vk_compute.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include "vk_commands.h"
#include <string.h>

extern refimport_t ri;

// Vulkan function pointer extern declarations
extern PFN_vkResetFences qvkResetFences;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkWaitForFences qvkWaitForFences;

// Async compute job queue (internal to module)
static vk_async_compute_job_t vk_async_compute_jobs[16];
static uint32_t vk_async_compute_job_count = 0;

// Submit async compute work
qboolean vk_submit_async_compute(VkCommandBuffer cmd_buffer, qboolean wait_for_graphics) {
    if (!vk.compute_queue.supported || cmd_buffer == VK_NULL_HANDLE) {
        return qfalse;
    }

    if (vk_async_compute_job_count >= 16) {
        ri.Printf(PRINT_WARNING, "Vulkan: Async compute job queue full\n");
        return qfalse;
    }

    vk_async_compute_job_t *job = &vk_async_compute_jobs[vk_async_compute_job_count++];
    job->active = qtrue;
    job->command_buffer = cmd_buffer;
    job->fence = vk.compute_queue.fences[vk.compute_queue.current_buffer_index];
    job->wait_for_graphics = wait_for_graphics;

    // Reset fence
    qvkResetFences(vk.device, 1, &job->fence);

    // Submit compute work
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL
    };

    // Use timeline semaphore for cross-queue synchronization if needed
    if (wait_for_graphics && vk.timeline_semaphore != VK_NULL_HANDLE) {
        // Wait for graphics queue to finish
        VkSemaphore wait_semaphores[1] = {vk.timeline_semaphore};
        VkPipelineStageFlags wait_stages[1] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        uint64_t wait_value = vk.timeline_value;

        // Framework: wait_info reserved for future timeline semaphore implementation
        (void)wait_value; // Suppress unused warning

        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
    }

    VkResult result = qvkQueueSubmit(vk.compute_queue.queue, 1, &submit_info, job->fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to submit async compute work: %s\n", vk_result_string(result));
        vk_async_compute_job_count--;
        return qfalse;
    }

    // Advance to next buffer
    vk.compute_queue.current_buffer_index = (vk.compute_queue.current_buffer_index + 1) % NUM_COMMAND_BUFFERS;

    return qtrue;
}

// Wait for async compute to complete
void vk_wait_async_compute(void) {
    if (!vk.compute_queue.supported) {
        return;
    }

    // Wait for all active compute jobs
    for (uint32_t i = 0; i < vk_async_compute_job_count; i++) {
        if (vk_async_compute_jobs[i].active) {
            VkResult result = qvkWaitForFences(vk.device, 1, &vk_async_compute_jobs[i].fence, VK_TRUE, UINT64_MAX);
            if (result == VK_SUCCESS) {
                vk_async_compute_jobs[i].active = qfalse;
            }
        }
    }

    // Clean up completed jobs
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < vk_async_compute_job_count; i++) {
        if (vk_async_compute_jobs[i].active) {
            if (write_idx != i) {
                vk_async_compute_jobs[write_idx] = vk_async_compute_jobs[i];
            }
            write_idx++;
        }
    }
    vk_async_compute_job_count = write_idx;
}

// Shutdown async compute system
void vk_shutdown_async_compute(void) {
    if (!vk.compute_queue.supported) {
        return;
    }

    // Wait for all compute work to complete
    vk_wait_async_compute();

    // Destroy fences
    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        if (vk.compute_queue.fences[i] != VK_NULL_HANDLE) {
            qvkDestroyFence(vk.device, vk.compute_queue.fences[i], NULL);
            vk.compute_queue.fences[i] = VK_NULL_HANDLE;
        }
    }

    // Free command buffers
    if (vk.compute_queue.command_pool != VK_NULL_HANDLE) {
        VK_FreeCommandBuffers(vk.device, vk.compute_queue.command_pool, NUM_COMMAND_BUFFERS, vk.compute_queue.command_buffers);
        VK_DestroyCommandPool(vk.device, vk.compute_queue.command_pool);
        vk.compute_queue.command_pool = VK_NULL_HANDLE;
    }

    Com_Memset(&vk.compute_queue, 0, sizeof(vk.compute_queue));
    ri.Printf(PRINT_ALL, "Vulkan: Async compute system shut down\n");
}
