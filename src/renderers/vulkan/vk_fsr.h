#ifndef VK_FSR_H_
#define VK_FSR_H_

#include "vulkan/vulkan.h"
#include "../fsr/ffx_a.h"
#include "../fsr/ffx_fsr1.h"

// FSR pipeline types
typedef enum {
    FSR_EASU_TO_RCAS,
    FSR_EASU_TO_DISPLAY,
    FSR_RCAS_AFTER_EASU,
    FSR_RCAS_AFTER_TAAU,
    FSR_NUM_PIPELINES
} fsr_pipeline_type_t;

// FSR constants structure
typedef struct {
    AU4 easu_const0;
    AU4 easu_const1;
    AU4 easu_const2;
    AU4 easu_const3;
} vk_fsr_easu_constants_t;

typedef struct {
    AU4 rcas_const0;
} vk_fsr_rcas_constants_t;

// FSR configuration
typedef struct {
    qboolean enabled;
    qboolean easu_enabled;
    qboolean rcas_enabled;
    float sharpness; // 0.0 to 2.0
} vk_fsr_config_t;

// FSR pipeline state
typedef struct {
    VkPipeline pipelines[FSR_NUM_PIPELINES];
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkBuffer constants_buffer;
    VkDeviceMemory constants_memory;
    void* constants_mapped;
    qboolean initialized;
} vk_fsr_state_t;

// Function declarations
qboolean vk_fsr_init(void);
void vk_fsr_shutdown(void);
qboolean vk_fsr_create_pipelines(void);
void vk_fsr_destroy_pipelines(void);
qboolean vk_fsr_is_enabled(void);
void vk_fsr_update_constants(uint32_t render_width, uint32_t render_height,
                           uint32_t display_width, uint32_t display_height);
void vk_fsr_apply_easu(VkCommandBuffer cmd_buf, VkImage input_image,
                      VkImageView input_view, VkImage output_image, VkImageView output_view);
void vk_fsr_apply_rcas(VkCommandBuffer cmd_buf, VkImage input_image,
                      VkImageView input_view, VkImage output_image, VkImageView output_view);

// CVAR externs
extern cvar_t *r_fsr_enable;
extern cvar_t *r_fsr_easu;
extern cvar_t *r_fsr_rcas;
extern cvar_t *r_fsr_sharpness;

#endif // VK_FSR_H_