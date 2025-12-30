/*
=============================================================================
Vulkan Framebuffer Management Header
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include <vector>

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Enhanced framebuffer creation
VkResult vk_create_framebuffers_enhanced(VkRenderPass renderPass, uint32_t width, uint32_t height,
                                       std::vector<VkImageView>& imageViews, VkFramebuffer** framebuffers);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN