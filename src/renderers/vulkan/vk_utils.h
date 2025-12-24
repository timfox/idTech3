#ifndef __VK_UTILS_H__
#define __VK_UTILS_H__

#include "vk.h"

// Safety and validation functions
void vk_safety_checks(void);
qboolean vk_bounds_check(size_t index, size_t max, const char *array_name);
qboolean vk_validate_handle(void *handle, const char *handle_name);

// Performance monitoring
void vk_performance_marker_begin(VkCommandBuffer cmd, const char *name);
void vk_performance_marker_end(VkCommandBuffer cmd);

// Float validation and sanitization
qboolean vk_validate_float(float value);
qboolean vk_validate_vec3(vec3_t v);
qboolean vk_validate_vec4(vec4_t v);
float vk_sanitize_float(float value, float default_value);
void vk_sanitize_vec3(vec3_t v, float default_value);
void vk_sanitize_vec4(vec4_t v, float default_value);

// Shader input validation
qboolean vk_validate_shader_inputs(const Vk_Pipeline_Def *def);

// Memory statistics
void vk_print_memory_stats(void);

#endif // __VK_UTILS_H__
