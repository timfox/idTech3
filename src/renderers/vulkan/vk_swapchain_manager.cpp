/*
=============================================================================
Vulkan Swapchain Management - C++23 Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <vector>
#include <algorithm>
#include <ranges>

#ifdef USE_VULKAN

// Global Vulkan structures (declared in vk.h and vk.c)
extern VkInstance vk_instance;
extern Vk_Instance vk;

namespace swapchain_mgr {

// Swapchain configuration structure
struct SwapchainConfig {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
    VkExtent2D extent;
    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    uint32_t image_count;
};

// Surface format scoring for optimal selection
static int score_surface_format(const VkSurfaceFormatKHR& format) {
    // Prefer SRGB formats for better color representation
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return 100;
    }

    // Accept other common formats
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
        return 80;
    }

    return 0;
}

// Present mode priority (higher = better)
static int get_present_mode_priority(VkPresentModeKHR mode) {
    switch (mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return 100; // Lowest latency
        case VK_PRESENT_MODE_MAILBOX_KHR: return 90;    // VSync with less tearing
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return 80; // Relaxed FIFO
        case VK_PRESENT_MODE_FIFO_KHR: return 70;       // Standard VSync
        default: return 0;
    }
}

// Query surface capabilities and supported formats/modes
static bool query_surface_support(VkPhysicalDevice physical_device, VkSurfaceKHR surface, SwapchainConfig& config) {
    VK_CHECK(qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &config.capabilities));

    uint32_t format_count;
    VK_CHECK(qvkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));
    if (format_count == 0) {
        ri.Printf(PRINT_ERROR, "Vulkan: No surface formats supported\n");
        return false;
    }

    config.formats.resize(format_count);
    VK_CHECK(qvkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, config.formats.data()));

    uint32_t present_mode_count;
    VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));
    if (present_mode_count == 0) {
        ri.Printf(PRINT_ERROR, "Vulkan: No present modes supported\n");
        return false;
    }

    config.present_modes.resize(present_mode_count);
    VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, config.present_modes.data()));

    return true;
}

// Choose optimal surface format
static VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    // If only one format is available and it's undefined, use our preferred format
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // Find the highest scoring format
    auto best_format = std::ranges::max_element(formats, {},
        [](const VkSurfaceFormatKHR& format) { return score_surface_format(format); });

    return *best_format;
}

// Choose optimal present mode based on VSync settings
static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes, int vsync_setting) {
    if (vsync_setting != 0) {
        // VSync enabled - prefer FIFO_RELAXED, fallback to FIFO
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
                return mode;
            }
        }
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_FIFO_KHR) {
                return mode;
            }
        }
    } else {
        // VSync disabled - prefer IMMEDIATE, fallback to MAILBOX
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return mode;
            }
        }
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }
    }

    // Fallback to FIFO (always available)
    return VK_PRESENT_MODE_FIFO_KHR;
}

// Calculate optimal image extent
static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height)
    };

    extent.width = std::clamp(extent.width,
                             capabilities.minImageExtent.width,
                             capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                              capabilities.minImageExtent.height,
                              capabilities.maxImageExtent.height);

    return extent;
}

// Determine optimal image count
static uint32_t choose_image_count(const VkSurfaceCapabilitiesKHR& capabilities, VkPresentModeKHR present_mode) {
    uint32_t count = capabilities.minImageCount;

    // For mailbox mode, prefer one extra image to reduce latency
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR && capabilities.maxImageCount > count) {
        count++;
    }

    // For immediate mode, prefer double buffering minimum
    if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        count = std::max(count, 2u);
    }

    // Clamp to maximum
    if (capabilities.maxImageCount > 0) {
        count = std::min(count, capabilities.maxImageCount);
    }

    return count;
}

// Create swapchain with comprehensive error handling
static VkResult create_swapchain_internal(VkPhysicalDevice physical_device, VkDevice device,
                                         VkSurfaceKHR surface, const SwapchainConfig& config,
                                         VkSwapchainKHR* swapchain) {
    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = config.image_count;
    create_info.imageFormat = config.format.format;
    create_info.imageColorSpace = config.format.colorSpace;
    create_info.imageExtent = config.extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Add transfer usage if supported (for screenshots/clears)
    if (config.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (config.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.preTransform = config.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = config.present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = qvkCreateSwapchainKHR(device, &create_info, nullptr, swapchain);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create swapchain: %s\n", vk_result_string(result));
        return result;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Swapchain created successfully\n");
    return VK_SUCCESS;
}

// Get swapchain images and create image views
static bool setup_swapchain_images(VkDevice device, VkSwapchainKHR swapchain,
                                  VkFormat format, uint32_t* image_count,
                                  VkImage** images, VkImageView** image_views) {
    VK_CHECK(qvkGetSwapchainImagesKHR(device, swapchain, image_count, nullptr));

    // Limit to maximum supported
    *image_count = std::min(*image_count, static_cast<uint32_t>(MAX_SWAPCHAIN_IMAGES));

    // Allocate arrays
    *images = (VkImage*)ri.Malloc(*image_count * sizeof(VkImage));
    *image_views = (VkImageView*)ri.Malloc(*image_count * sizeof(VkImageView));

    VK_CHECK(qvkGetSwapchainImagesKHR(device, swapchain, image_count, *images));

    ri.Printf(PRINT_ALL, "Vulkan: Retrieved %u swapchain images\n", *image_count);

    // Create image views
    for (uint32_t i = 0; i < *image_count; i++) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = (*images)[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkResult result = qvkCreateImageView(device, &view_info, nullptr, &(*image_views)[i]);
        if (result != VK_SUCCESS) {
            ri.Printf(PRINT_ERROR, "Vulkan: Failed to create image view %u: %s\n", i, vk_result_string(result));
            return false;
        }
    }

    ri.Printf(PRINT_ALL, "Vulkan: Created %u swapchain image views\n", *image_count);
    return true;
}

} // namespace swapchain_mgr

// Public interface functions
bool vk_create_swapchain_enhanced(VkPhysicalDevice physical_device, VkDevice device,
                                 VkSurfaceKHR surface, int width, int height,
                                 VkSwapchainKHR* swapchain, VkFormat* format,
                                 uint32_t* image_count, VkImage** images, VkImageView** image_views) {
    swapchain_mgr::SwapchainConfig config;

    // Query surface support
    if (!swapchain_mgr::query_surface_support(physical_device, surface, config)) {
        return false;
    }

    // Choose optimal parameters
    config.format = swapchain_mgr::choose_surface_format(config.formats);
    config.present_mode = swapchain_mgr::choose_present_mode(config.present_modes,
        ri.Cvar_VariableIntegerValue("r_swapInterval"));
    config.extent = swapchain_mgr::choose_extent(config.capabilities, width, height);
    config.image_count = swapchain_mgr::choose_image_count(config.capabilities, config.present_mode);

    // Print configuration
    ri.Printf(PRINT_ALL, "Vulkan: Swapchain config - Format: %u, Present mode: %u, Extent: %ux%u, Images: %u\n",
             config.format.format, config.present_mode, config.extent.width, config.extent.height, config.image_count);

    // Create swapchain
    VkResult result = swapchain_mgr::create_swapchain_internal(physical_device, device, surface, config, swapchain);
    if (result != VK_SUCCESS) {
        return false;
    }

    // Setup images and views
    if (!swapchain_mgr::setup_swapchain_images(device, *swapchain, config.format.format,
                                              image_count, images, image_views)) {
        return false;
    }

    *format = config.format.format;
    return true;
}

#endif // USE_VULKAN