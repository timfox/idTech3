/*
=============================================================================
Vulkan Render Pass Management Header
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Enhanced render pass creation
VkResult vk_create_main_render_pass_enhanced(VkFormat colorFormat, VkFormat depthFormat, VkRenderPass* renderPass);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN