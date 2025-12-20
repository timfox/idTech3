/*
=============================================================================
Vulkan Asynchronous Shader Compilation Header
=============================================================================
*/

#ifndef VK_ASYNC_COMPILE_H
#define VK_ASYNC_COMPILE_H

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

qboolean vk_async_compile_init(void);
void vk_async_compile_shutdown(void);

#endif // USE_VULKAN

#endif // VK_ASYNC_COMPILE_H
