/*
=============================================================================
Vulkan Swapchain Management Header
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Enhanced swapchain creation with better error handling
bool vk_create_swapchain_enhanced(VkPhysicalDevice physical_device, VkDevice device,
                                 VkSurfaceKHR surface, int width, int height,
                                 VkSwapchainKHR* swapchain, VkFormat* format,
                                 uint32_t* image_count, VkImage** images, VkImageView** image_views);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN