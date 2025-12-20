#ifndef VK_DYNAMIC_RENDERING_H
#define VK_DYNAMIC_RENDERING_H

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

qboolean vk_dynamic_rendering_check_support(void);
qboolean vk_dynamic_rendering_enabled(void);

#endif // USE_VULKAN

#endif // VK_DYNAMIC_RENDERING_H
