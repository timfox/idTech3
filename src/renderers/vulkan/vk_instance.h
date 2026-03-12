/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan instance and device creation.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#pragma once

#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Instance and surface (created by vk_init_vulkan_library, destroyed by vk_destroy_instance) */
extern VkInstance vk_instance;
extern VkSurfaceKHR vk_surface;

#ifdef USE_VK_VALIDATION
extern VkDebugReportCallbackEXT vk_debug_callback;
#endif

/* Initialize Vulkan library, create instance, enumerate devices, create logical device. */
void vk_init_vulkan_library( void );

/* Destroy instance and surface. Call before deinit. */
void vk_destroy_instance( void );

/* Reset instance-level function pointers to NULL. Call after vk_destroy_instance. */
void vk_deinit_instance_functions( void );

/* Reset device-level function pointers to NULL. Call after vkDestroyDevice. */
void vk_deinit_device_functions( void );

#ifdef __cplusplus
}
#endif
