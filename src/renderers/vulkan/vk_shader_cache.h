/*
=============================================================================
Vulkan Shader Cache System Header
=============================================================================
*/

#ifndef VK_SHADER_CACHE_H
#define VK_SHADER_CACHE_H

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

qboolean vk_shader_cache_init(void);
void vk_shader_cache_shutdown(void);
qboolean vk_shader_cache_get(const char *shader_name, void **spirv_data, size_t *spirv_size);
qboolean vk_shader_cache_put(const char *shader_name, const void *spirv_data, size_t spirv_size);
qboolean create_shader_cache_directory(void);

#endif // USE_VULKAN

#endif // VK_SHADER_CACHE_H
