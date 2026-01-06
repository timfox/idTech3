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
// Real TLAS builder
qboolean vk_rtx_build_tlas_real(VkCommandBuffer cmd_buffer);
qboolean vk_rtx_build_tlas_real_full(VkCommandBuffer cmd_buffer);
qboolean vk_rtx_build_blas_for_geometry_real(VkCommandBuffer cmd_buffer);
// Query per-surface material index
uint32_t vk_rtx_get_surface_material_index(uint32_t surfaceIndex, uint32_t* outIndex);
// Build BLAS for world geometry during map load
qboolean vk_rtx_build_blas_from_world(void);
void vk_rtx_update_surface_material_indices_buffer(void);
VkBuffer vk_rtx_get_surface_indices_buffer(void);
VkDeviceSize vk_rtx_get_surface_indices_size(void);
void vk_rtx_bind_surface_indices_buffer(VkDescriptorSet descriptorSet);
void vk_rtx_create_sbt_buffer_full(void);
VkResult vk_rtx_create_pipeline(void);
void VK_try_init_calibrated_timestamps(void);

// Plan A RTX groundwork (pipeline creation and per-frame hooks)
VkResult vk_rtx_create_pipeline(void);
void vk_rtx_bind_and_trace_raysKHR_from_main(VkCommandBuffer cmd_buffer, uint32_t width, uint32_t height);
void vk_rtx_trace_raysKHR(VkCommandBuffer cmd_buffer);
void vk_rtx_setup_sbt(VkCommandBuffer cmd_buffer);
void vk_rtx_build_sbt_for_frame_full(VkCommandBuffer cmd_buffer);
// Stub for hardware ray tracing kernel launch (will be fleshed out in future)
void vk_rtx_trace_raysKHR(VkCommandBuffer cmd_buffer);
// Hardware trace path (width/height provided by caller)
void vk_rtx_bind_and_trace_raysKHR_from_main(VkCommandBuffer cmd_buffer, uint32_t width, uint32_t height);
void vk_rtx_setup_sbt(VkCommandBuffer cmd_buffer);
void vk_rtx_create_sbt_buffer(void);
void vk_rtx_build_sbt_for_frame(VkCommandBuffer cmd_buffer);
// Temporary per-frame RT scratch and TLAS/BLAS scaffolding
extern VkBuffer g_rt_scratch_buffer;
extern VkDeviceMemory g_rt_scratch_memory;
extern VkDeviceSize g_rt_scratch_size;

// Advanced lighting system
uint32_t vk_rtx_add_light(const vec3_t position, const vec3_t color, float intensity,
                         float radius, int light_type, qboolean casts_shadows);
void vk_rtx_update_light(uint32_t light_id, const vec3_t position, const vec3_t color, float intensity);
void vk_rtx_set_light_advanced_properties(uint32_t light_id, float temperature, float cone_angle,
                                        const vec3_t direction, int ies_profile);
void vk_rtx_create_shadow_acceleration(uint32_t light_id, VkCommandBuffer cmd_buffer);
qboolean vk_rtx_trace_shadow_ray(const vec3_t origin, const vec3_t direction, float distance);
void vk_rtx_update_lighting_data(VkBuffer light_buffer, VkDeviceMemory light_buffer_memory);

// Quality and performance settings
typedef enum {
    RTX_QUALITY_LOW = 0,
    RTX_QUALITY_MEDIUM,
    RTX_QUALITY_HIGH,
    RTX_QUALITY_ULTRA
} rtx_quality_preset_t;

void vk_rtx_set_quality_settings(float quality, qboolean shadows, qboolean reflections,
                               qboolean refractions, qboolean global_illumination);
void vk_rtx_set_quality_preset(rtx_quality_preset_t preset);
rtx_quality_preset_t vk_rtx_get_current_quality_preset(void);

// BVH optimization functions
qboolean vk_rtx_build_blas_incremental(VkCommandBuffer cmd, uint32_t changed_surface_start, uint32_t changed_surface_count);
void vk_rtx_optimize_bvh_quality(rtx_quality_preset_t preset);
void vk_rtx_compact_geometry(void);
qboolean vk_rtx_update_instance_transforms(VkCommandBuffer cmd_buffer);

// Parallel BLAS building
typedef struct {
    VkAccelerationStructureKHR accel;
    VkDeviceMemory memory;
    VkBuffer scratch_buffer;
    VkDeviceMemory scratch_memory;
    VkDeviceSize scratch_size;
    uint32_t surface_index;
    qboolean valid;
} blas_build_job_t;

qboolean vk_rtx_build_blas_parallel(VkCommandBuffer cmd_buffer, uint32_t max_parallel_jobs);
void vk_rtx_submit_blas_jobs(VkCommandBuffer cmd_buffer, blas_build_job_t *jobs, uint32_t job_count);

// Memory management and pooling
typedef struct RTXMemoryPool_s {
    VkBuffer scratch_buffer;
    VkDeviceMemory scratch_memory;
    VkDeviceSize allocated_size;
    VkDeviceSize max_size;
    qboolean initialized;
} RTXMemoryPool_t;

qboolean vk_rtx_memory_pool_init(RTXMemoryPool_t *pool, VkDeviceSize initial_size, VkDeviceSize max_size);
void vk_rtx_memory_pool_shutdown(RTXMemoryPool_t *pool);
VkBuffer vk_rtx_memory_pool_allocate_scratch(RTXMemoryPool_t *pool, VkDeviceSize size, VkDeviceMemory *memory);
void vk_rtx_memory_pool_free_scratch(RTXMemoryPool_t *pool, VkBuffer buffer, VkDeviceMemory memory);
qboolean vk_rtx_memory_pool_defragment(RTXMemoryPool_t *pool);

// Memory budgeting and lazy allocation
typedef struct RTXMemoryBudget_s {
    VkDeviceSize current_usage;
    VkDeviceSize peak_usage;
    VkDeviceSize budget_limit;
    VkDeviceSize warning_threshold;
    uint32_t allocation_count;
} RTXMemoryBudget_t;

void vk_rtx_memory_budget_init(RTXMemoryBudget_t *budget, VkDeviceSize limit);
qboolean vk_rtx_memory_budget_check(RTXMemoryBudget_t *budget, VkDeviceSize requested_size);
void vk_rtx_memory_budget_allocate(RTXMemoryBudget_t *budget, VkDeviceSize size);
void vk_rtx_memory_budget_free(RTXMemoryBudget_t *budget, VkDeviceSize size);
void vk_rtx_memory_budget_report(RTXMemoryBudget_t *budget);

// Lazy allocation system
typedef struct RTXLazyAllocator_s {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize used;
    qboolean allocated;
    qboolean dirty;
} RTXLazyAllocator_t;

qboolean vk_rtx_lazy_allocate(RTXLazyAllocator_t *allocator, VkDeviceSize minimum_size);
void vk_rtx_lazy_free(RTXLazyAllocator_t *allocator);
VkDeviceSize vk_rtx_lazy_get_free_space(RTXLazyAllocator_t *allocator);

// SBT optimization functions
typedef struct RTXSBTCache_s {
    VkBuffer sbt_buffer;
    VkDeviceMemory sbt_memory;
    VkDeviceSize sbt_size;
    uint32_t shader_group_count;
    qboolean valid;
} RTXSBTCache_t;

qboolean vk_rtx_sbt_cache_init(RTXSBTCache_t *cache, uint32_t max_shader_groups);
void vk_rtx_sbt_cache_shutdown(RTXSBTCache_t *cache);
qboolean vk_rtx_sbt_build_optimized(VkCommandBuffer cmd_buffer, RTXSBTCache_t *cache);
qboolean vk_rtx_sbt_update_incremental(VkCommandBuffer cmd_buffer, RTXSBTCache_t *cache,
                                     uint32_t changed_groups_start, uint32_t changed_groups_count);
qboolean vk_rtx_sbt_compress(RTXSBTCache_t *cache);
void vk_rtx_sbt_reorder_groups(RTXSBTCache_t *cache);
qboolean vk_rtx_sbt_prefetch_groups(VkCommandBuffer cmd_buffer, RTXSBTCache_t *cache,
                                  const uint32_t *group_indices, uint32_t group_count);

// Statistics and monitoring
void vk_rtx_get_statistics(uint32_t* triangles, uint32_t* instances, uint32_t* lights,
                         VkDeviceSize* memory_usage, float* quality);
void vk_rtx_performance_monitor(uint64_t frame_time_ns, uint32_t ray_count);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN