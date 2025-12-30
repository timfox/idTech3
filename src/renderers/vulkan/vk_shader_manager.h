/*
=============================================================================
Vulkan Shader Management Header
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Shader management functions
qboolean vk_shader_manager_init(void);
void vk_shader_manager_shutdown(void);
VkShaderModule vk_load_shader(const char* shader_name, VkShaderStageFlagBits stage);
qboolean vk_create_basic_pipeline(const char* vertex_shader, const char* fragment_shader,
                                 VkPipelineLayout pipeline_layout, VkRenderPass render_pass,
                                 VkPipeline* pipeline);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN