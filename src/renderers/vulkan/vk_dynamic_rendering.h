#ifndef VK_DYNAMIC_RENDERING_H
#define VK_DYNAMIC_RENDERING_H

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

qboolean vk_dynamic_rendering_check_support(void);
qboolean vk_dynamic_rendering_enabled(void);

// Dynamic rendering functions (Vulkan 1.4 core)
void vk_begin_dynamic_rendering(const VkRenderingInfo *rendering_info);
void vk_end_dynamic_rendering(void);
void vk_setup_rendering_info(VkRenderingInfo *info, VkImageView color_view, VkImageView depth_view,
                            uint32_t width, uint32_t height, VkClearValue *clear_values);

#endif // USE_VULKAN

#endif // VK_DYNAMIC_RENDERING_H
