#ifndef __VK_PIPELINE_H__
#define __VK_PIPELINE_H__

#include <vulkan/vulkan.h>
#include <stdint.h>
#include "q_shared.h"

// Shader hot reload structures
typedef struct {
    char filename[MAX_QPATH];
    uint64_t last_modified;
    uint32_t shader_count;
} shader_file_watch_t;

// Pipeline binary header for disk storage
typedef struct {
    uint64_t hash;
    VkDeviceSize binary_size;
} pipeline_binary_header_t;

// Pipeline management function declarations
void vk_create_shader_modules(void);
void vk_create_pipelines(void);
void vk_create_brdflut_pipeline(void);
void vk_create_compute_post_process_pipelines(void);
void vk_create_post_process_pipeline(int program_index, uint32_t width, uint32_t height);
void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass);
void vk_update_post_process_pipelines(void);
void vk_destroy_pipelines(qboolean resetCounter);
void vk_mark_pipelines_dirty(void);

// Pipeline allocation and lookup functions are declared in vk.h

// Pipeline cache operations
void vk_pipeline_cache_load(void **data_out, size_t *size_out);
void vk_pipeline_cache_save(void);

// Pipeline binary operations (VK_KHR_pipeline_executable_properties)
__attribute__((unused)) void vk_pipeline_binary_save(VkPipeline pipeline, uint64_t pipeline_hash);
__attribute__((unused)) qboolean vk_pipeline_binary_load(uint64_t pipeline_hash, void **binary_data, VkDeviceSize *binary_size);
__attribute__((unused)) VkPipeline vk_create_pipeline_from_binary(uint64_t pipeline_hash, VkPipelineLayout layout,
    const struct Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index);

// Shader hot reload system
__attribute__((unused)) void vk_check_shader_hot_reload(void);
__attribute__((unused)) qboolean vk_reload_shader(const char *shader_name);

// Core pipeline creation (internal)
VkPipeline create_pipeline(const struct Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index);
VkPipeline vk_gen_pipeline(uint32_t index);

// Utility functions for pipeline creation
void get_viewport_rect(VkRect2D *r);

// Pipeline barrier utilities
void vk_barrier_final_image_to_shader_read(VkImage image);

#endif // __VK_PIPELINE_H__
