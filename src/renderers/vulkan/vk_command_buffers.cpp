/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <vector>

// External Vulkan objects (declared in initialization module)
extern VkInstance vk_instance;
extern VkPhysicalDevice vk_physical_device;
// External Vulkan objects are now part of the global vk structure

// Command buffer state is local to this module

// Vulkan function pointers
extern PFN_vkGetDeviceProcAddr qvkGetDeviceProcAddr;
extern PFN_vkCreateCommandPool qvkCreateCommandPool;
extern PFN_vkDestroyCommandPool qvkDestroyCommandPool;
extern PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;
extern PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
// Timeline semaphore function pointers (optional)
extern PFN_vkWaitSemaphoresKHR qvkWaitSemaphoresKHR;
extern PFN_vkSignalSemaphoreKHR qvkSignalSemaphoreKHR;
// Forward declare temporary readback path helpers (optional)
extern PFN_vkCmdCopyImageToBuffer qvkCmdCopyImageToBuffer; // ensure symbol presence
extern PFN_vkEndCommandBuffer qvkEndCommandBuffer;
extern PFN_vkResetCommandBuffer qvkResetCommandBuffer;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkCreateFence qvkCreateFence;
extern PFN_vkDestroyFence qvkDestroyFence;
extern PFN_vkResetFences qvkResetFences;
extern PFN_vkWaitForFences qvkWaitForFences;
extern PFN_vkGetFenceStatus qvkGetFenceStatus;
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkQueueWaitIdle qvkQueueWaitIdle;
extern PFN_vkDeviceWaitIdle qvkDeviceWaitIdle;

// Vulkan Command Buffer Management Module
// Handles command pool and command buffer creation, recording, and submission

// Command buffer state (local to this module)
// Use vk.vk.command_pool from the global vk structure
static VkCommandBuffer current_command_buffer = VK_NULL_HANDLE;
static std::vector<VkCommandBuffer> command_buffers;
static VkFence command_fence = VK_NULL_HANDLE;

// Command buffer functions
// Vulkan function pointers (defined in vk.c)

// Initialize command buffer function pointers
void vk_init_command_functions(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return;
    }

    qvkCreateCommandPool = (PFN_vkCreateCommandPool)qvkGetDeviceProcAddr(vk.device, "vkCreateCommandPool");
    qvkDestroyCommandPool = (PFN_vkDestroyCommandPool)qvkGetDeviceProcAddr(vk.device, "vkDestroyCommandPool");
    qvkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)qvkGetDeviceProcAddr(vk.device, "vkAllocateCommandBuffers");
    qvkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)qvkGetDeviceProcAddr(vk.device, "vkFreeCommandBuffers");
    qvkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)qvkGetDeviceProcAddr(vk.device, "vkBeginCommandBuffer");
    qvkEndCommandBuffer = (PFN_vkEndCommandBuffer)qvkGetDeviceProcAddr(vk.device, "vkEndCommandBuffer");
    qvkResetCommandBuffer = (PFN_vkResetCommandBuffer)qvkGetDeviceProcAddr(vk.device, "vkResetCommandBuffer");
    qvkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)qvkGetDeviceProcAddr(vk.device, "vkCmdPipelineBarrier");
    qvkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)qvkGetDeviceProcAddr(vk.device, "vkCmdCopyImageToBuffer");
    qvkCreateFence = (PFN_vkCreateFence)qvkGetDeviceProcAddr(vk.device, "vkCreateFence");
    qvkDestroyFence = (PFN_vkDestroyFence)qvkGetDeviceProcAddr(vk.device, "vkDestroyFence");
    qvkResetFences = (PFN_vkResetFences)qvkGetDeviceProcAddr(vk.device, "vkResetFences");
    qvkWaitForFences = (PFN_vkWaitForFences)qvkGetDeviceProcAddr(vk.device, "vkWaitForFences");
    qvkGetFenceStatus = (PFN_vkGetFenceStatus)qvkGetDeviceProcAddr(vk.device, "vkGetFenceStatus");
    qvkQueueSubmit = (PFN_vkQueueSubmit)qvkGetDeviceProcAddr(vk.device, "vkQueueSubmit");
    qvkQueueWaitIdle = (PFN_vkQueueWaitIdle)qvkGetDeviceProcAddr(vk.device, "vkQueueWaitIdle");
    qvkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)qvkGetDeviceProcAddr(vk.device, "vkDeviceWaitIdle");
    // Optional: load timeline semaphore functions
    qvkWaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)qvkGetDeviceProcAddr(vk.device, "vkWaitSemaphoresKHR");
    qvkSignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)qvkGetDeviceProcAddr(vk.device, "vkSignalSemaphoreKHR");
}

// Create command pool
qboolean vk_create_command_pool(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        ri.Printf(PRINT_ALL, "Command: Skipping command pool creation (stub device)\n");
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Command: Creating command pool\n");

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk.queue_family_index
    };

    VkResult result = qvkCreateCommandPool(vk.device, &poolInfo, nullptr, &vk.command_pool);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to create command pool: %d\n", result);
        return qfalse;
    }

    // Create fence for command buffer synchronization
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    result = qvkCreateFence(vk.device, &fenceInfo, nullptr, &command_fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to create fence: %d\n", result);
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Command: Command pool created successfully\n");
    return qtrue;
}

// Allocate command buffers
qboolean vk_allocate_command_buffers(uint32_t count) {
    if (!vk.command_pool) {
        return qfalse;
    }

    command_buffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = vk.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count
    };

    VkResult result = qvkAllocateCommandBuffers(vk.device, &allocInfo, command_buffers.data());
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to allocate command buffers: %d\n", result);
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Command: Allocated %u command buffers\n", count);
    return qtrue;
}

// Begin command buffer recording
extern "C" VkCommandBuffer vk_begin_command_buffer(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return (VkCommandBuffer)0x30000000; // Fake command buffer handle
    }

    // Check if device is lost before proceeding
    if (vk.device_lost) {
        return VK_NULL_HANDLE;
    }

    if (command_buffers.empty()) {
        vk_allocate_command_buffers(1);
    }

    current_command_buffer = command_buffers[0];

    // Wait for all frame command buffers to complete before reusing immediate command buffer
    // This prevents conflicts between immediate commands and frame rendering
    // Use frame-based synchronization to align with the main rendering pipeline
    if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
        // Collect all active frame fences to ensure no frame is still in flight
        VkFence frame_fences[NUM_COMMAND_BUFFERS];
        uint32_t fence_count = 0;
        
        // Add all valid frame fences
        for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
            if (vk.tess[i].rendering_finished_fence != VK_NULL_HANDLE) {
                frame_fences[fence_count++] = vk.tess[i].rendering_finished_fence;
            }
        }
        
        // Wait for all frame fences if any exist
        if (fence_count > 0) {
            VkResult fence_result = qvkWaitForFences(vk.device, fence_count, frame_fences, VK_TRUE, UINT64_MAX);
            if (fence_result == VK_ERROR_DEVICE_LOST) {
                vk.device_lost = qtrue;
                vk_reset_memory_tracking_on_device_lost();
                ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer fence wait\n");
                return VK_NULL_HANDLE;
            } else if (fence_result != VK_SUCCESS) {
                ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Fence wait failed: %d\n", fence_result);
            }
        } else {
            // Fallback to static fence if no frame fences are available (e.g., during initialization)
            if (command_fence != VK_NULL_HANDLE) {
                VkResult fence_result = qvkWaitForFences(vk.device, 1, &command_fence, VK_TRUE, UINT64_MAX);
                if (fence_result == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                    vk_reset_memory_tracking_on_device_lost();
                    ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer fence wait\n");
                    return VK_NULL_HANDLE;
                } else if (fence_result != VK_SUCCESS) {
                    ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Fence wait failed: %d\n", fence_result);
                } else {
                    // Reset static fence after waiting (frame fences are managed by frame system)
                    qvkResetFences(vk.device, 1, &command_fence);
                }
            }
        }
    }

    // Reset command buffer only if device is still valid
    if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
        VkResult reset_result = qvkResetCommandBuffer(current_command_buffer, 0);
        if (reset_result != VK_SUCCESS) {
            ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Failed to reset command buffer: %d\n", reset_result);
            return VK_NULL_HANDLE;
        }
    } else {
        return VK_NULL_HANDLE;
    }

    // Begin recording
    // This is a reusable command buffer - it's reset before reuse
    // Omit ONE_TIME_SUBMIT_BIT for better performance with reusable buffers
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0, // Reusable buffer - no ONE_TIME_SUBMIT_BIT
        .pInheritanceInfo = nullptr
    };

    VkResult result = qvkBeginCommandBuffer(current_command_buffer, &beginInfo);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to begin command buffer: %d\n", result);
        return VK_NULL_HANDLE;
    }

    return current_command_buffer;
}

// End and submit command buffer
extern "C" void vk_end_command_buffer(VkCommandBuffer command_buffer, const char *location) {
    Q_UNUSED(location);

    if (!command_buffer || command_buffer == (VkCommandBuffer)0x30000000) {
        return; // Fake command buffer
    }

    // Check if device is lost before proceeding
    if (vk.device_lost) {
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Skipping command buffer submission - device is lost\n");
        return;
    }

    // End recording
    VkResult result = qvkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to end command buffer: %d\n", result);
        return;
    }

    // Submit to queue
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    result = qvkQueueSubmit(vk.queue, 1, &submitInfo, command_fence);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer submit\n");
            return;
        } else {
            ri.Printf(PRINT_ERROR, "Command: Failed to submit command buffer: %d\n", result);
            return;
        }
    }

    // Wait for completion (for simplicity, in a real engine you'd use multiple buffers)
    VkResult fence_result = qvkWaitForFences(vk.device, 1, &command_fence, VK_TRUE, UINT64_MAX);
    if (fence_result == VK_ERROR_DEVICE_LOST) {
        vk.device_lost = qtrue;
        vk_reset_memory_tracking_on_device_lost();
        ri.Printf(PRINT_ERROR, "Vulkan: Device lost during fence wait - GPU driver issue\n");
        return;
    } else if (fence_result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "vk_end_command_buffer: Fence wait failed: %d\n", fence_result);
    } else {
        // Reset fence for next use
        qvkResetFences(vk.device, 1, &command_fence);
    }
}

// vk_wait_idle is defined in vk.c

// Destroy command pool and resources
void vk_destroy_command_pool(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return;
    }

    // Wait for any pending command buffers to complete before cleanup
    // This ensures resources are safe to destroy
    if (command_fence != VK_NULL_HANDLE && !vk.device_lost && vk.device != VK_NULL_HANDLE && qvkGetFenceStatus) {
        // Non-blocking check - if fence is signaled, we can proceed
        VkResult status = qvkGetFenceStatus(vk.device, command_fence);
        if (status == VK_NOT_READY) {
            // Wait briefly for completion, but don't block indefinitely during shutdown
            VkResult wait_result = qvkWaitForFences(vk.device, 1, &command_fence, VK_TRUE, 100000000); // 100ms timeout
            if (wait_result != VK_SUCCESS && wait_result != VK_TIMEOUT) {
                if (wait_result == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                }
            }
        } else if (status == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
        }
    }

    // Free command buffers (safe even if device is lost - these are just handles)
    if (!command_buffers.empty() && vk.command_pool != VK_NULL_HANDLE) {
        if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
            qvkFreeCommandBuffers(vk.device, vk.command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
        }
        command_buffers.clear();
    }

    // Destroy fence
    if (command_fence != VK_NULL_HANDLE) {
        if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
            qvkDestroyFence(vk.device, command_fence, nullptr);
        }
        command_fence = VK_NULL_HANDLE;
    }

    // Destroy command pool (must be done after freeing all command buffers)
    if (vk.command_pool != VK_NULL_HANDLE) {
        if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
            qvkDestroyCommandPool(vk.device, vk.command_pool, nullptr);
        }
        vk.command_pool = VK_NULL_HANDLE;
    }

    current_command_buffer = VK_NULL_HANDLE;
    ri.Printf(PRINT_ALL, "Command: Command pool destroyed\n");
}