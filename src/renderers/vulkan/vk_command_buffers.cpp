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
#include <vector>

// External Vulkan objects (declared in initialization module)
extern VkInstance vk_instance;
extern VkPhysicalDevice vk_physical_device;
extern VkDevice vk_device;
extern VkQueue vk_queue;
extern uint32_t vk_queue_family_index;

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
extern PFN_vkQueueSubmit qvkQueueSubmit;
extern PFN_vkQueueWaitIdle qvkQueueWaitIdle;
extern PFN_vkDeviceWaitIdle qvkDeviceWaitIdle;

// Vulkan Command Buffer Management Module
// Handles command pool and command buffer creation, recording, and submission

// Command buffer state (local to this module)
static VkCommandPool command_pool = VK_NULL_HANDLE;
static VkCommandBuffer current_command_buffer = VK_NULL_HANDLE;
static std::vector<VkCommandBuffer> command_buffers;
static VkFence command_fence = VK_NULL_HANDLE;

// Command buffer functions
PFN_vkCreateCommandPool qvkCreateCommandPool = nullptr;
PFN_vkDestroyCommandPool qvkDestroyCommandPool = nullptr;
PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers = nullptr;
PFN_vkFreeCommandBuffers qvkFreeCommandBuffers = nullptr;
PFN_vkBeginCommandBuffer qvkBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer qvkEndCommandBuffer = nullptr;
PFN_vkResetCommandBuffer qvkResetCommandBuffer = nullptr;
PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier = nullptr;
PFN_vkCreateFence qvkCreateFence = nullptr;
PFN_vkDestroyFence qvkDestroyFence = nullptr;
PFN_vkResetFences qvkResetFences = nullptr;
PFN_vkWaitForFences qvkWaitForFences = nullptr;
PFN_vkQueueSubmit qvkQueueSubmit = nullptr;
PFN_vkQueueWaitIdle qvkQueueWaitIdle = nullptr;
PFN_vkDeviceWaitIdle qvkDeviceWaitIdle = nullptr;

// Initialize command buffer function pointers
void vk_init_command_functions(void) {
    if (!vk_device || vk_device == (VkDevice)0x20000000) {
        return;
    }

    qvkCreateCommandPool = (PFN_vkCreateCommandPool)qvkGetDeviceProcAddr(vk_device, "vkCreateCommandPool");
    qvkDestroyCommandPool = (PFN_vkDestroyCommandPool)qvkGetDeviceProcAddr(vk_device, "vkDestroyCommandPool");
    qvkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)qvkGetDeviceProcAddr(vk_device, "vkAllocateCommandBuffers");
    qvkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)qvkGetDeviceProcAddr(vk_device, "vkFreeCommandBuffers");
    qvkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)qvkGetDeviceProcAddr(vk_device, "vkBeginCommandBuffer");
    qvkEndCommandBuffer = (PFN_vkEndCommandBuffer)qvkGetDeviceProcAddr(vk_device, "vkEndCommandBuffer");
    qvkResetCommandBuffer = (PFN_vkResetCommandBuffer)qvkGetDeviceProcAddr(vk_device, "vkResetCommandBuffer");
    qvkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)qvkGetDeviceProcAddr(vk_device, "vkCmdPipelineBarrier");
    qvkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)qvkGetDeviceProcAddr(vk_device, "vkCmdCopyImageToBuffer");
    qvkCreateFence = (PFN_vkCreateFence)qvkGetDeviceProcAddr(vk_device, "vkCreateFence");
    qvkDestroyFence = (PFN_vkDestroyFence)qvkGetDeviceProcAddr(vk_device, "vkDestroyFence");
    qvkResetFences = (PFN_vkResetFences)qvkGetDeviceProcAddr(vk_device, "vkResetFences");
    qvkWaitForFences = (PFN_vkWaitForFences)qvkGetDeviceProcAddr(vk_device, "vkWaitForFences");
    qvkQueueSubmit = (PFN_vkQueueSubmit)qvkGetDeviceProcAddr(vk_device, "vkQueueSubmit");
    qvkQueueWaitIdle = (PFN_vkQueueWaitIdle)qvkGetDeviceProcAddr(vk_device, "vkQueueWaitIdle");
    qvkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)qvkGetDeviceProcAddr(vk_device, "vkDeviceWaitIdle");
    // Optional: load timeline semaphore functions
    qvkWaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)qvkGetDeviceProcAddr(vk_device, "vkWaitSemaphoresKHR");
    qvkSignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)qvkGetDeviceProcAddr(vk_device, "vkSignalSemaphoreKHR");
}

// Create command pool
qboolean vk_create_command_pool(void) {
    if (!vk_device || vk_device == (VkDevice)0x20000000) {
        ri.Printf(PRINT_ALL, "Command: Skipping command pool creation (stub device)\n");
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Command: Creating command pool\n");

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk_queue_family_index
    };

    VkResult result = qvkCreateCommandPool(vk_device, &poolInfo, nullptr, &command_pool);
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

    result = qvkCreateFence(vk_device, &fenceInfo, nullptr, &command_fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to create fence: %d\n", result);
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Command: Command pool created successfully\n");
    return qtrue;
}

// Allocate command buffers
qboolean vk_allocate_command_buffers(uint32_t count) {
    if (!command_pool) {
        return qfalse;
    }

    command_buffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count
    };

    VkResult result = qvkAllocateCommandBuffers(vk_device, &allocInfo, command_buffers.data());
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to allocate command buffers: %d\n", result);
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Command: Allocated %u command buffers\n", count);
    return qtrue;
}

// Begin command buffer recording
VkCommandBuffer vk_begin_command_buffer(void) {
    if (!vk_device || vk_device == (VkDevice)0x20000000) {
        return (VkCommandBuffer)0x30000000; // Fake command buffer handle
    }

    if (command_buffers.empty()) {
        vk_allocate_command_buffers(1);
    }

    current_command_buffer = command_buffers[0];

    // Wait for previous frame to complete
    qvkWaitForFences(vk_device, 1, &command_fence, VK_TRUE, UINT64_MAX);
    qvkResetFences(vk_device, 1, &command_fence);

    // Reset command buffer
    qvkResetCommandBuffer(current_command_buffer, 0);

    // Begin recording
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
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
void vk_end_command_buffer(VkCommandBuffer command_buffer, const char *location) {
    Q_UNUSED(location);

    if (!command_buffer || command_buffer == (VkCommandBuffer)0x30000000) {
        return; // Fake command buffer
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

    result = qvkQueueSubmit(vk_queue, 1, &submitInfo, command_fence);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Command: Failed to submit command buffer: %d\n", result);
        return;
    }

    // Wait for completion (for simplicity, in a real engine you'd use multiple buffers)
    qvkWaitForFences(vk_device, 1, &command_fence, VK_TRUE, UINT64_MAX);
}

// Wait for device to become idle
void vk_wait_idle(void) {
    if (vk_device && vk_device != (VkDevice)0x20000000) {
        qvkDeviceWaitIdle(vk_device);
    }
}

// Destroy command pool and resources
void vk_destroy_command_pool(void) {
    if (!vk_device || vk_device == (VkDevice)0x20000000) {
        return;
    }

    // Free command buffers
    if (!command_buffers.empty() && command_pool) {
        qvkFreeCommandBuffers(vk_device, command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
        command_buffers.clear();
    }

    // Destroy fence
    if (command_fence) {
        qvkDestroyFence(vk_device, command_fence, nullptr);
        command_fence = VK_NULL_HANDLE;
    }

    // Destroy command pool
    if (command_pool) {
        qvkDestroyCommandPool(vk_device, command_pool, nullptr);
        command_pool = VK_NULL_HANDLE;
    }

    ri.Printf(PRINT_ALL, "Command: Command pool destroyed\n");
}