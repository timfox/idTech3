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

// NUM_COMMAND_BUFFERS is defined in vk.h
#ifndef NUM_COMMAND_BUFFERS
#define NUM_COMMAND_BUFFERS 2
#endif

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
// Allocate NUM_COMMAND_BUFFERS to match frame pipelining
static std::vector<VkCommandBuffer> command_buffers;
static VkFence command_fences[NUM_COMMAND_BUFFERS]; // Per-frame fences for immediate commands

// Batch reset command buffers for better performance
// Resets multiple command buffers efficiently
static qboolean batch_reset_command_buffers(VkCommandBuffer *buffers, uint32_t count) {
    if (!buffers || count == 0 || vk.device_lost || vk.device == VK_NULL_HANDLE) {
        return qfalse;
    }
    
    // Reset all buffers - individual calls but organized for better performance
    // Vulkan doesn't have a batch reset API, but we can optimize by:
    // 1. Collecting all buffers first
    // 2. Resetting them in a tight loop for better cache behavior
    qboolean all_succeeded = qtrue;
    for (uint32_t i = 0; i < count; i++) {
        if (buffers[i] != VK_NULL_HANDLE) {
            VkResult result = qvkResetCommandBuffer(buffers[i], 0);
            if (result != VK_SUCCESS) {
                if (result == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                    extern void vk_reset_memory_tracking_on_device_lost(void);
                    vk_reset_memory_tracking_on_device_lost();
                    ri.Printf(PRINT_ERROR, "Vulkan: Device lost during batch command buffer reset\n");
                    return qfalse;
                } else {
                    ri.Printf(PRINT_WARNING, "batch_reset_command_buffers: Failed to reset buffer %u: %d\n", i, result);
                    all_succeeded = qfalse;
                }
            }
        }
    }
    
    return all_succeeded;
}

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

    // Create per-frame fences for immediate command buffer synchronization
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        result = qvkCreateFence(vk.device, &fenceInfo, nullptr, &command_fences[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "Command: Failed to create fence %u: %d\n", i, result);
            // Clean up previously created fences
            for (uint32_t j = 0; j < i; j++) {
                qvkDestroyFence(vk.device, command_fences[j], nullptr);
                command_fences[j] = VK_NULL_HANDLE;
            }
            return qfalse;
        }
    }

    ri.Printf(PRINT_ALL, "Command: Command pool created successfully\n");
    return qtrue;
}

// Allocate command buffers
// Allocates NUM_COMMAND_BUFFERS to match frame pipelining
extern "C" qboolean vk_allocate_command_buffers(uint32_t count) {
    if (!vk.command_pool) {
        return qfalse;
    }

    // Ensure we allocate at least NUM_COMMAND_BUFFERS for frame pipelining
    uint32_t alloc_count = (count < NUM_COMMAND_BUFFERS) ? NUM_COMMAND_BUFFERS : count;
    command_buffers.resize(alloc_count);

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = vk.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = alloc_count
    };

    VkResult result = qvkAllocateCommandBuffers(vk.device, &allocInfo, command_buffers.data());
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer allocation\n");
        } else {
            ri.Printf(PRINT_ERROR, "Command: Failed to allocate command buffers: %d\n", result);
        }
        return qfalse;
    }

    // Bind allocated command buffers to per-frame slots
    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS && i < command_buffers.size(); i++) {
        vk.tess[i].command_buffer = command_buffers[i];
    }

    ri.Printf(PRINT_ALL, "Command: Allocated %u command buffers (for %u frames)\n", alloc_count, NUM_COMMAND_BUFFERS);
    return qtrue;
}

// Begin command buffer recording
// Uses frame-based indexing to leverage frame pipelining
extern "C" VkCommandBuffer vk_begin_command_buffer(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return (VkCommandBuffer)0x30000000; // Fake command buffer handle
    }

    // Check if device is lost before proceeding
    if (vk.device_lost) {
        return VK_NULL_HANDLE;
    }

    // Allocate NUM_COMMAND_BUFFERS if not already allocated
    if (command_buffers.empty()) {
        vk_allocate_command_buffers(NUM_COMMAND_BUFFERS);
    }

    // Use frame index to select the appropriate command buffer
    // This allows immediate commands to be pipelined with frame rendering
    uint32_t frame_index = vk.cmd_index;
    if (frame_index >= command_buffers.size()) {
        frame_index = 0; // Fallback to first buffer if index is out of bounds
    }

    VkCommandBuffer current_command_buffer = command_buffers[frame_index];

    // Wait on the current frame's rendering fence to ensure immediate commands
    // don't conflict with frame rendering. This ensures proper synchronization
    // between immediate commands and frame rendering command buffers.
    if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
        // Wait on both the immediate command fence and the frame rendering fence
        // to ensure we don't conflict with either
        VkFence immediate_fence = command_fences[frame_index];
        VkFence frame_fence = vk.tess[frame_index].rendering_finished_fence;
        
        // Wait on frame rendering fence first to ensure frame rendering is complete
        // This prevents immediate commands from conflicting with frame rendering
        if (frame_fence != VK_NULL_HANDLE) {
            extern PFN_vkGetFenceStatus qvkGetFenceStatus;
            if (qvkGetFenceStatus) {
                VkResult frame_status = qvkGetFenceStatus(vk.device, frame_fence);
                if (frame_status == VK_SUCCESS) {
                    // Frame fence is signaled, frame rendering is complete
                    // Reset it for next use
                    qvkResetFences(vk.device, 1, &frame_fence);
                } else if (frame_status == VK_NOT_READY) {
                    // Frame is still rendering, wait for it to complete
                    VkResult fence_result = qvkWaitForFences(vk.device, 1, &frame_fence, VK_TRUE, UINT64_MAX);
                    if (fence_result == VK_ERROR_DEVICE_LOST) {
                        vk.device_lost = qtrue;
                        extern void vk_reset_memory_tracking_on_device_lost(void);
                        vk_reset_memory_tracking_on_device_lost();
                        ri.Printf(PRINT_ERROR, "Vulkan: Device lost during frame fence wait\n");
                        return VK_NULL_HANDLE;
                    } else if (fence_result != VK_SUCCESS) {
                        ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Frame fence wait failed: %d\n", fence_result);
                    } else {
                        // Reset fence after waiting
                        qvkResetFences(vk.device, 1, &frame_fence);
                    }
                } else if (frame_status == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                    extern void vk_reset_memory_tracking_on_device_lost(void);
                    vk_reset_memory_tracking_on_device_lost();
                    ri.Printf(PRINT_ERROR, "Vulkan: Device lost during frame fence status check\n");
                    return VK_NULL_HANDLE;
                }
            } else {
                // Fallback: wait on fence if status check is unavailable
                VkResult fence_result = qvkWaitForFences(vk.device, 1, &frame_fence, VK_TRUE, UINT64_MAX);
                if (fence_result == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                    extern void vk_reset_memory_tracking_on_device_lost(void);
                    vk_reset_memory_tracking_on_device_lost();
                    ri.Printf(PRINT_ERROR, "Vulkan: Device lost during frame fence wait\n");
                    return VK_NULL_HANDLE;
                } else if (fence_result != VK_SUCCESS) {
                    ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Frame fence wait failed: %d\n", fence_result);
                } else {
                    // Reset fence after waiting
                    qvkResetFences(vk.device, 1, &frame_fence);
                }
            }
        }
        
        // Also wait on immediate command fence for this frame
        if (immediate_fence != VK_NULL_HANDLE) {
            VkResult fence_result = qvkWaitForFences(vk.device, 1, &immediate_fence, VK_TRUE, UINT64_MAX);
            if (fence_result == VK_ERROR_DEVICE_LOST) {
                vk.device_lost = qtrue;
                extern void vk_reset_memory_tracking_on_device_lost(void);
                vk_reset_memory_tracking_on_device_lost();
                ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer fence wait\n");
                return VK_NULL_HANDLE;
            } else if (fence_result != VK_SUCCESS) {
                ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Fence wait failed: %d\n", fence_result);
            } else {
                // Reset fence after waiting
                qvkResetFences(vk.device, 1, &immediate_fence);
            }
        }
    }

    // Reset command buffer only if device is still valid
    // Note: We reset individually here since we only need one buffer
    // For batch resets, use batch_reset_command_buffers() when resetting multiple buffers
    if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
        VkResult reset_result = qvkResetCommandBuffer(current_command_buffer, 0);
        if (reset_result != VK_SUCCESS) {
            if (reset_result == VK_ERROR_DEVICE_LOST) {
                vk.device_lost = qtrue;
                extern void vk_reset_memory_tracking_on_device_lost(void);
                vk_reset_memory_tracking_on_device_lost();
                ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer reset\n");
            } else {
                ri.Printf(PRINT_WARNING, "vk_begin_command_buffer: Failed to reset command buffer: %d\n", reset_result);
            }
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
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer begin\n");
        } else {
            ri.Printf(PRINT_ERROR, "Command: Failed to begin command buffer: %d\n", result);
        }
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

    // Find which frame index this command buffer belongs to
    uint32_t frame_index = vk.cmd_index;
    if (frame_index >= command_buffers.size()) {
        frame_index = 0; // Fallback
    }
    
    // Verify this is the correct buffer for this frame
    if (command_buffer != command_buffers[frame_index]) {
        // Search for the buffer index
        frame_index = NUM_COMMAND_BUFFERS; // Invalid
        for (uint32_t i = 0; i < command_buffers.size(); i++) {
            if (command_buffers[i] == command_buffer) {
                frame_index = i;
                break;
            }
        }
        if (frame_index >= NUM_COMMAND_BUFFERS) {
            ri.Printf(PRINT_WARNING, "vk_end_command_buffer: Command buffer not found in allocated buffers\n");
            frame_index = 0; // Fallback
        }
    }

    // End recording
    VkResult result = qvkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer end\n");
        } else {
            ri.Printf(PRINT_ERROR, "Command: Failed to end command buffer: %d\n", result);
        }
        return;
    }

    // Submit to queue using frame-specific fence
    VkFence fence_to_use = command_fences[frame_index];
    if (fence_to_use == VK_NULL_HANDLE) {
        ri.Printf(PRINT_ERROR, "vk_end_command_buffer: No fence available for frame %u\n", frame_index);
        return;
    }

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

    result = qvkQueueSubmit(vk.queue, 1, &submitInfo, fence_to_use);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST) {
            vk.device_lost = qtrue;
            extern void vk_reset_memory_tracking_on_device_lost(void);
            vk_reset_memory_tracking_on_device_lost();
            ri.Printf(PRINT_ERROR, "Vulkan: Device lost during command buffer submit\n");
            return;
        } else {
            ri.Printf(PRINT_ERROR, "Command: Failed to submit command buffer: %d\n", result);
            return;
        }
    }

    // Wait for completion using frame-specific fence
    VkResult fence_result = qvkWaitForFences(vk.device, 1, &fence_to_use, VK_TRUE, UINT64_MAX);
    if (fence_result == VK_ERROR_DEVICE_LOST) {
        vk.device_lost = qtrue;
        extern void vk_reset_memory_tracking_on_device_lost(void);
        vk_reset_memory_tracking_on_device_lost();
        ri.Printf(PRINT_ERROR, "Vulkan: Device lost during fence wait - GPU driver issue\n");
        return;
    } else if (fence_result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "vk_end_command_buffer: Fence wait failed: %d\n", fence_result);
    } else {
        // Reset fence for next use
        qvkResetFences(vk.device, 1, &fence_to_use);
    }

    // Note: Command buffers are reused, not freed after each use
    // They're allocated once and reset before reuse
}

// vk_wait_idle is defined in vk.c

// Batch reset all immediate command buffers
// Can be called when multiple buffers are ready to be reset
extern "C" void vk_batch_reset_command_buffers(void) {
    if (command_buffers.empty() || vk.device_lost || vk.device == VK_NULL_HANDLE) {
        return;
    }
    
    // Batch reset all allocated command buffers
    // This is more efficient than resetting them individually throughout the code
    batch_reset_command_buffers(command_buffers.data(), static_cast<uint32_t>(command_buffers.size()));
}

// Destroy command pool and resources
void vk_destroy_command_pool(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return;
    }

    // Wait for any pending command buffers to complete before cleanup
    // Check all per-frame fences
    if (!vk.device_lost && vk.device != VK_NULL_HANDLE && qvkGetFenceStatus) {
        for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
            if (command_fences[i] != VK_NULL_HANDLE) {
                VkResult status = qvkGetFenceStatus(vk.device, command_fences[i]);
                if (status == VK_NOT_READY) {
                    // Wait briefly for completion, but don't block indefinitely during shutdown
                    VkResult wait_result = qvkWaitForFences(vk.device, 1, &command_fences[i], VK_TRUE, 100000000); // 100ms timeout
                    if (wait_result != VK_SUCCESS && wait_result != VK_TIMEOUT) {
                        if (wait_result == VK_ERROR_DEVICE_LOST) {
                            vk.device_lost = qtrue;
                        }
                    }
                } else if (status == VK_ERROR_DEVICE_LOST) {
                    vk.device_lost = qtrue;
                }
            }
        }
    }

    // Free command buffers (safe even if device is lost - these are just handles)
    if (!command_buffers.empty() && vk.command_pool != VK_NULL_HANDLE) {
        if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
            qvkFreeCommandBuffers(vk.device, vk.command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
        }
        command_buffers.clear();
    }

    // Destroy per-frame fences
    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS; i++) {
        if (command_fences[i] != VK_NULL_HANDLE) {
            if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
                qvkDestroyFence(vk.device, command_fences[i], nullptr);
            }
            command_fences[i] = VK_NULL_HANDLE;
        }
    }

    // Destroy command pool (must be done after freeing all command buffers)
    if (vk.command_pool != VK_NULL_HANDLE) {
        if (!vk.device_lost && vk.device != VK_NULL_HANDLE) {
            qvkDestroyCommandPool(vk.device, vk.command_pool, nullptr);
        }
        vk.command_pool = VK_NULL_HANDLE;
    }

    ri.Printf(PRINT_ALL, "Command: Command pool destroyed\n");
}