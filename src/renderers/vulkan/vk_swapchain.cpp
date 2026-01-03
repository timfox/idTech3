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
// These are now part of the global vk structure

// Vulkan function pointers
extern PFN_vkGetDeviceProcAddr qvkGetDeviceProcAddr;
// Use vkGetInstanceProcAddr directly from Vulkan loader
extern PFN_vkCreateSemaphore qvkCreateSemaphore;
extern PFN_vkDestroySemaphore qvkDestroySemaphore;
extern PFN_vkCreateSwapchainKHR qvkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR qvkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR qvkGetSwapchainImagesKHR;
extern PFN_vkAcquireNextImageKHR qvkAcquireNextImageKHR;
extern PFN_vkQueuePresentKHR qvkQueuePresentKHR;

// Vulkan Swapchain Management Module
// Handles swapchain creation, recreation, and presentation

// Swapchain function pointers (defined in vk.c)

// Current swapchain state
// These are now part of the global vk structure
// Present format is defined in this TU to ensure a single definition
VkSurfaceFormatKHR vk_present_format = {};

// Internal swapchain state for this module
static std::vector<VkImage> swapchain_images;
static std::vector<VkImageView> swapchain_image_views;
static std::vector<VkSemaphore> image_available_semaphores;
static std::vector<VkSemaphore> rendering_finished_semaphores;

// Initialize swapchain function pointers
void vk_init_swapchain_functions(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return;
    }

    // Load swapchain functions
    qvkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)qvkGetDeviceProcAddr(vk.device, "vkCreateSwapchainKHR");
    qvkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)qvkGetDeviceProcAddr(vk.device, "vkDestroySwapchainKHR");
    qvkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)qvkGetDeviceProcAddr(vk.device, "vkGetSwapchainImagesKHR");
    qvkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)qvkGetDeviceProcAddr(vk.device, "vkAcquireNextImageKHR");
    qvkQueuePresentKHR = (PFN_vkQueuePresentKHR)qvkGetDeviceProcAddr(vk.device, "vkQueuePresentKHR");
    qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    qvkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    qvkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
}

// Choose surface format
VkSurfaceFormatKHR vk_choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0]; // Fallback to first available
}

// Choose present mode
VkPresentModeKHR vk_choose_present_mode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode; // Triple buffering
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // V-sync guaranteed
}

// Create swapchain
qboolean vk_create_swapchain(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        ri.Printf(PRINT_ALL, "Swapchain: Skipping creation (stub device)\n");
        return qtrue;
    }

    ri.Printf(PRINT_ALL, "Swapchain: Creating swapchain\n");

    // Get surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result = qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical_device, vk.surface, &capabilities);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to get surface capabilities\n");
        return qfalse;
    }

    // Get surface formats
    uint32_t formatCount;
    result = qvkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        ri.Printf(PRINT_ERROR, "Swapchain: No surface formats available\n");
        return qfalse;
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    result = qvkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &formatCount, formats.data());
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to get surface formats\n");
        return qfalse;
    }

    // Choose surface format
    vk_present_format = vk_choose_surface_format(formats);

    // Get present modes
    uint32_t presentModeCount;
    result = qvkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical_device, vk.surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        ri.Printf(PRINT_ERROR, "Swapchain: No present modes available\n");
        return qfalse;
    }

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    result = qvkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical_device, vk.surface, &presentModeCount, presentModes.data());
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to get present modes\n");
        return qfalse;
    }

    // Choose present mode
    VkPresentModeKHR presentMode = vk_choose_present_mode(presentModes);

    // Determine image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Determine extent
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = glConfig.vidWidth;
        extent.height = glConfig.vidHeight;

        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = vk.surface,
        .minImageCount = imageCount,
        .imageFormat = vk_present_format.format,
        .imageColorSpace = vk_present_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    result = qvkCreateSwapchainKHR(vk.device, &createInfo, nullptr, &vk.swapchain);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to create swapchain: %d\n", result);
        return qfalse;
    }

    // Get swapchain images
    result = qvkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, nullptr);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to get image count\n");
        return qfalse;
    }

    swapchain_images.resize(vk.swapchain_image_count);
    result = qvkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, swapchain_images.data());
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to get images\n");
        return qfalse;
    }

    // Create image views
    swapchain_image_views.resize(vk.swapchain_image_count);
    for (size_t i = 0; i < vk.swapchain_image_count; i++) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk_present_format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        result = qvkCreateImageView(vk.device, &viewInfo, nullptr, &swapchain_image_views[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "Swapchain: Failed to create image view %zu\n", i);
            return qfalse;
        }
    }

    // Create semaphores
    image_available_semaphores.resize(vk.swapchain_image_count);
    rendering_finished_semaphores.resize(vk.swapchain_image_count);

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    for (size_t i = 0; i < vk.swapchain_image_count; i++) {
        result = qvkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &image_available_semaphores[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "Swapchain: Failed to create image available semaphore\n");
            return qfalse;
        }

        result = qvkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &rendering_finished_semaphores[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "Swapchain: Failed to create rendering finished semaphore\n");
            return qfalse;
        }
    }

    // Set initial semaphores
    vk.image_available = image_available_semaphores[0];
    vk.rendering_finished = rendering_finished_semaphores[0];

    ri.Printf(PRINT_ALL, "Swapchain: Created successfully with %u images\n", vk.swapchain_image_count);
    return qtrue;
}

// Destroy swapchain
void vk_destroy_swapchain(void) {
    if (!vk.device || vk.device == (VkDevice)0x20000000) {
        return;
    }

    // Destroy semaphores
    for (auto semaphore : image_available_semaphores) {
        if (semaphore) qvkDestroySemaphore(vk.device, semaphore, nullptr);
    }
    for (auto semaphore : rendering_finished_semaphores) {
        if (semaphore) qvkDestroySemaphore(vk.device, semaphore, nullptr);
    }

    image_available_semaphores.clear();
    rendering_finished_semaphores.clear();

    // Destroy image views
    for (auto view : swapchain_image_views) {
        if (view) qvkDestroyImageView(vk.device, view, nullptr);
    }
    swapchain_image_views.clear();

    // Destroy swapchain
    if (vk.swapchain) {
        qvkDestroySwapchainKHR(vk.device, vk.swapchain, nullptr);
        vk.swapchain = VK_NULL_HANDLE;
    }

    ri.Printf(PRINT_ALL, "Swapchain: Destroyed\n");
}

// Acquire next swapchain image
qboolean vk_acquire_next_image(void) {
    if (!vk.swapchain) {
        return qfalse;
    }

#ifdef USE_VULKAN
    if (VK_IsHeadless()) {
        // In headless mode, do not acquire swapchain images
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Headless skip acquireNextImageKHR in vk_swapchain.cpp\n");
        return VK_NOT_READY;
    }
#endif
    VkResult result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                                             vk.image_available, VK_NULL_HANDLE,
                                             &vk.current_swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs recreation
        vk_destroy_swapchain();
        vk_create_swapchain();
        return vk_acquire_next_image(); // Try again
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to acquire image: %d\n", result);
        return qfalse;
    }

    return qtrue;
}

// Present swapchain image
qboolean vk_present_image(void) {
    if (!vk.swapchain) {
        return qfalse;
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.rendering_finished,
        .swapchainCount = 1,
        .pSwapchains = &vk.swapchain,
        .pImageIndices = &vk.current_swapchain_image_index,
        .pResults = nullptr
    };

    VkResult result = qvkQueuePresentKHR(vk.queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Swapchain needs recreation
        vk_destroy_swapchain();
        vk_create_swapchain();
        return qfalse;
    } else if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Swapchain: Failed to present: %d\n", result);
        return qfalse;
    }

    return qtrue;
}