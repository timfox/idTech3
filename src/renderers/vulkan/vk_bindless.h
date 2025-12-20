/*
=============================================================================
Vulkan Bindless Texture System Header
=============================================================================
*/

#ifndef VK_BINDLESS_H
#define VK_BINDLESS_H

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

qboolean vk_bindless_init(void);
void vk_bindless_shutdown(void);
uint32_t vk_bindless_register_texture(VkImageView imageView);

#endif // USE_VULKAN

#endif // VK_BINDLESS_H
