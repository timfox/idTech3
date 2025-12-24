#ifndef __VK_POSTPROCESS_H__
#define __VK_POSTPROCESS_H__

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

// Post-processing system initialization
qboolean vk_init_post_processing(void);
void vk_shutdown_post_processing(void);

// Bloom effect management
qboolean vk_create_bloom_resources(void);
void vk_destroy_bloom_resources(void);
void vk_apply_bloom(void);

// Pipeline creation
void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass);
void vk_update_post_process_pipelines(void);

// Effects
void vk_apply_tone_mapping(void);
void vk_apply_gamma_correction(void);

// Configuration
int vk_get_post_process_quality(void);
qboolean vk_has_post_processing(void);

#ifdef __cplusplus
}
#endif

#endif // __VK_POSTPROCESS_H__
