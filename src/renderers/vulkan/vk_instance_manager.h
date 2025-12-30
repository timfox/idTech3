/*
=============================================================================
Vulkan Instance and Device Management Header
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Instance and device management functions
VkResult vk_create_instance(void);
bool vk_enumerate_devices(void);
VkResult vk_create_device(void);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN