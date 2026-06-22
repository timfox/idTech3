/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan swapchain creation and query helpers.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Create swapchain, image views, and semaphores. Writes to vk.swapchain,
 * vk.swapchain_images, vk.swapchain_image_views, vk.swapchain_image_count,
 * vk.swapchain_rendering_finished, vk.swapchain_extent. */
void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface,
	VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose );

/* Destroy swapchain, image views, and semaphores. Call before recreating swapchain. */
void vk_destroy_swapchain( void );

/* Query current surface extent. Returns qfalse on failure. */
qboolean vk_query_surface_extent( VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkExtent2D *extent );

/* Log swapchain recreation (throttled to avoid spam). */
void vk_log_swapchain_recreation( VkResult res, const VkExtent2D *old_extent, const VkExtent2D *new_extent );

#ifdef __cplusplus
}
#endif
