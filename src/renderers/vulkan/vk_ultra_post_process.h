/*
=============================================================================
Vulkan Ultra-Advanced Post-Processing Header
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#ifdef __cplusplus
extern "C" {
#endif

// Ultra-advanced post-processing system
qboolean vk_ultra_post_process_init(void);
void vk_ultra_post_process_shutdown(void);
void vk_apply_ultra_post_processing(VkCommandBuffer cmd_buffer, VkImage input_image, VkImageView input_view,
                                   VkImage output_image, VkImageView output_view, uint32_t width, uint32_t height);
void vk_update_ultra_pp_parameters(float delta_time);
void vk_get_ultra_pp_stats(uint32_t *effects_count, uint64_t *total_time, float *avg_time_per_frame);

// Individual effect functions (for custom pipelines)
void vk_apply_film_grain(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                        VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_chromatic_aberration(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                                  VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_heat_distortion(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                             VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_motion_blur(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                         VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_vignette(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                      VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_lens_flare(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                        VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_sharpen(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                     VkImage output, VkImageView output_view, uint32_t width, uint32_t height);
void vk_apply_dithering(VkCommandBuffer cmd_buffer, VkImage input, VkImageView input_view,
                       VkImage output, VkImageView output_view, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN