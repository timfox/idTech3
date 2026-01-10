#ifndef __VK_UTILS_H__
#define __VK_UTILS_H__

#include "vk.h"
#include "vk_memory.h"  // For vk_memory_tracker_t

#ifdef __cplusplus
extern "C" {
#endif

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
qboolean vk_validate_memory_state(void);
void vk_print_memory_stats(void);
void vk_detect_memory_leaks(void);

// Memory tracker (extern declaration)
extern vk_memory_tracker_t vk_memory_tracker;

#ifdef __cplusplus
}
#endif

#endif // __VK_UTILS_H__
