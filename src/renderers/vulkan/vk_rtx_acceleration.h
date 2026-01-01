/*
=============================================================================
Vulkan RTX Acceleration Structures Header
=============================================================================
Enhanced RTX Integration with Acceleration Structures and Advanced Lighting
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Ensure matrix3x4_t is defined for this header if not pulled in by consumers.
#ifndef MATRIX3X4_T_DEFINED
typedef struct { float m[12]; } matrix3x4_t;
#define MATRIX3X4_T_DEFINED
#endif

// RTX acceleration structure management
qboolean vk_rtx_acceleration_init(void);
void vk_rtx_acceleration_shutdown(void);

// Acceleration structure creation
uint64_t vk_rtx_create_blas_for_geometry(VkBuffer vertex_buffer, VkBuffer index_buffer,
                                       uint32_t vertex_count, uint32_t index_count,
                                       uint32_t vertex_stride, const char* debug_name);
void vk_rtx_update_instance_data(uint64_t accel_id, const matrix3x4_t* transform, uint32_t instance_id);
void vk_rtx_build_tlas(VkCommandBuffer cmd_buffer);

// Advanced lighting system
uint32_t vk_rtx_add_light(const vec3_t position, const vec3_t color, float intensity,
                         float radius, int light_type, qboolean casts_shadows);
void vk_rtx_update_light(uint32_t light_id, const vec3_t position, const vec3_t color, float intensity);
void vk_rtx_set_light_advanced_properties(uint32_t light_id, float temperature, float cone_angle,
                                        const vec3_t direction, int ies_profile);
void vk_rtx_create_shadow_acceleration(uint32_t light_id, VkCommandBuffer cmd_buffer);
void vk_rtx_update_lighting_data(VkBuffer light_buffer, VkDeviceMemory light_buffer_memory);

// Quality and performance settings
void vk_rtx_set_quality_settings(float quality, qboolean shadows, qboolean reflections,
                               qboolean refractions, qboolean global_illumination);

// Statistics and monitoring
void vk_rtx_get_statistics(uint32_t* triangles, uint32_t* instances, uint32_t* lights,
                         VkDeviceSize* memory_usage, float* quality);
void vk_rtx_performance_monitor(uint64_t frame_time_ns, uint32_t ray_count);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN