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
#include "vk_images.h"
#include "vk.h"
// Prototypes for host-accessible command buffer helpers (use real functions)
extern VkCommandBuffer vk_begin_command_buffer(void);
extern void vk_end_command_buffer(VkCommandBuffer, const char* location);

// Texture Management Functions for Vulkan Renderer
// Simplified implementation for modularization

qhandle_t vk_register_shader(const char *name) {
    // Shader registration - simplified stub
    if (!name || !*name) {
        return 0;
    }

    ri.Printf(PRINT_DEVELOPER, "vk_register_shader: Registering shader %s\n", name);

    // Return a dummy handle for now
    static qhandle_t next_handle = 1000;
    return next_handle++;
}

qhandle_t vk_register_image(const char *name, int flags) {
    // Image registration - simplified stub
    if (!name || !*name) {
        return 0;
    }

    ri.Printf(PRINT_DEVELOPER, "vk_register_image: Registering image %s (flags=%d)\n", name, flags);

    // Return a dummy handle for now
    static qhandle_t next_handle = 2000;
    return next_handle++;
}

void vk_update_image_data(image_t* image, int x, int y, int width, int height, int layers, const void* data, int data_size) {
    // Update image data in Vulkan texture with staging buffers
    if (!image || !data) {
        return;
    }

    ri.Printf(PRINT_DEVELOPER, "vk_update_image_data: Updating texture at (%d,%d) size (%dx%d), layers=%d, data_size=%d\n",
              x, y, width, height, layers, data_size);

    // Validate parameters
    if (width <= 0 || height <= 0 || layers <= 0 || data_size <= 0) {
        ri.Printf(PRINT_WARNING, "vk_update_image_data: Invalid parameters\n");
        return;
    }

    // Skip for stub device
    if (vk.device == (VkDevice)0x20000000) {
        ri.Printf(PRINT_DEVELOPER, "vk_update_image_data: Skipping for stub device\n");
        return;
    }

    // Implement actual Vulkan texture update with staging buffers
    // This involves:
    // 1. Creating or reusing a staging buffer
    // 2. Mapping the staging buffer memory
    // 3. Copying data to the staging buffer
    // 4. Unmapping memory
    // 5. Recording command buffer operations:
    //    a. Transition image layout to TRANSFER_DST_OPTIMAL
    //    b. Copy from staging buffer to image
    //    c. Transition image layout back to SHADER_READ_ONLY_OPTIMAL
    // 6. Submitting and waiting for completion

    // Calculate expected data size for validation
    size_t expected_size = (size_t)width * (size_t)height * 4; // Assume RGBA8 format
    if (layers > 1) {
        expected_size *= layers;
    }

    if ((size_t)data_size != expected_size) {
        ri.Printf(PRINT_WARNING, "vk_update_image_data: Data size mismatch (expected %zu, got %d)\n",
                  expected_size, data_size);
    }

    // Upload using existing staging-based path in the image management subsystem
    // This uses the same staging buffer and copy mechanism as the rest of the engine.
    // We forward to the centralized upload path to ensure consistency.
    if (image && data && data_size > 0) {
        vk_upload_image_data(image, x, y, width, height, layers, data, data_size, qtrue);
        return;
    }

    ri.Printf(PRINT_DEVELOPER, "vk_update_image_data: Texture update framework ready - implementation pending Vulkan device access\n");

    Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(layers);
}

// Read back an image to CPU memory using a staging buffer
// Currently supports a single layer (layers == 1). Falls back gracefully otherwise.
void vk_readback_image_to_cpu(image_t *image, void *dstBuffer, int width, int height, int layers) {
    if (!image || !dstBuffer || width <= 0 || height <= 0 || layers != 1) {
        ri.Printf(PRINT_WARNING, "vk_readback_image_to_cpu: unsupported parameters\n");
        return;
    }
    // Calculate bytes (RGBA8 assumed)
    size_t bytes = (size_t)width * (size_t)height * 4;
    // Ensure staging buffer is large enough
    if (vk.staging_buffer.handle == VK_NULL_HANDLE || vk.staging_buffer.size < bytes) {
        vk_alloc_staging_buffer((VkDeviceSize)bytes);
    }
    // Allocate a short-lived command buffer for copy
    VkCommandBuffer cmd = vk_begin_command_buffer();
    if (cmd == VK_NULL_HANDLE) {
        ri.Printf(PRINT_WARNING, "vk_readback_image_to_cpu: failed to acquire command buffer\n");
        // Fallback: zero memory
        memset(dstBuffer, 0, bytes);
        return;
    }

    // Transition to transfer source
    VkImageMemoryBarrier barrier1 = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    qvkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier1);

    // Copy to staging buffer
    VkBufferImageCopy region = {};
    region.bufferOffset = vk.staging_buffer.offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = (uint32_t)width;
    region.imageExtent.height = (uint32_t)height;
    region.imageExtent.depth = 1;
    qvkCmdCopyImageToBuffer(cmd, image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk.staging_buffer.handle, 1, &region);

    // Transition back to shader read
    VkImageMemoryBarrier barrier2 = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image->handle,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    qvkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);

    // End and submit
    vk_end_command_buffer(cmd, "vk_readback_image_to_cpu");
    VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    qvkQueueSubmit(vk.queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk.queue);

    // Copy staging buffer to CPU memory
    if (vk.staging_buffer.ptr) {
        memcpy(dstBuffer, vk.staging_buffer.ptr, bytes);
    } else {
        memset(dstBuffer, 0, bytes);
    }
}
