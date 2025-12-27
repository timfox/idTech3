/*
=============================================================================
Vulkan Enhanced Post-Processing System

Contains definitions for advanced post-processing effects including:
- Dual SSAO (LISSAO + SAO)
- SSR (ray-marched)
- Bloom (Kawase blur + flares)
- DoF (sprite-scattered bokeh)
- Motion blur (velocity tiles)
- 3D LUT color grading
- Heat distortion/volumetrics
=============================================================================
*/

#ifndef VK_POST_PROCESS_H
#define VK_POST_PROCESS_H

#ifdef USE_VULKAN
#include "vk.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Post-processing effect flags
typedef enum {
    PP_EFFECT_SSAO         = (1 << 0),
    PP_EFFECT_SSR          = (1 << 1),
    PP_EFFECT_BLOOM        = (1 << 2),
    PP_EFFECT_DOF          = (1 << 3),
    PP_EFFECT_MOTION_BLUR  = (1 << 4),
    PP_EFFECT_COLOR_GRADING = (1 << 5),
    PP_EFFECT_HEAT_DISTORTION = (1 << 6),
    PP_EFFECT_LENS_FLARE   = (1 << 7),
} postProcessEffect_t;

// SSAO configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    float radius;
    float bias;
    float intensity;
    int numSamples;
    qboolean enableLISSAO;
    float indirectIntensity;
    float indirectRadius;
} ssaoConfig_t;

// SSR configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    vec3_t cameraPos;
    float maxDistance;
    float thickness;
    int numSteps;
    int numBinarySteps;
    float roughnessThreshold;
} ssrConfig_t;

// Bloom configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    float threshold;
    int extractMode; // 0=max, 1=avg, 2=luma
    int modulateMode; // 0=none, 1=square, 2=luma
    qboolean useKawase;
    int numPasses;
} bloomConfig_t;

// DoF configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    float focalDistance;
    float focalRange;
    float aperture;
    int numSamples;
    int bokehShape; // 0=circular, 1=hexagonal, 2=sprite
    float bokehRotation;
} dofConfig_t;

// Motion blur configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    int numSamples;
    float exposureTime;
    qboolean useVelocityTiles;
    vec2_t tileSize;
} motionBlurConfig_t;

// Color grading configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    float exposure;
    float contrast;
    float saturation;
    float brightness;
    vec3_t colorFilter;
    float gamma;
    vec3_t shadows;
    vec3_t midtones;
    vec3_t highlights;
    float shadowsStart;
    float shadowsEnd;
    float highlightsStart;
    float highlightsEnd;
    qboolean useLUT;
} colorGradingConfig_t;

// Heat distortion configuration
typedef struct {
    vec2_t resolution;
    vec2_t invResolution;
    float time;
    float distortionStrength;
    float heatWaveFrequency;
    float heatWaveSpeed;
    int numSamples;
    float atmosphericDistortion;
    vec3_t atmosphericTint;
} heatDistortionConfig_t;

// Master post-processing configuration
typedef struct {
    uint32_t enabledEffects; // Bitmask of postProcessEffect_t
    ssaoConfig_t ssao;
    ssrConfig_t ssr;
    bloomConfig_t bloom;
    dofConfig_t dof;
    motionBlurConfig_t motionBlur;
    colorGradingConfig_t colorGrading;
    heatDistortionConfig_t heatDistortion;
} postProcessConfig_t;

// Function declarations
void vk_init_enhanced_post_processing(void);
void vk_shutdown_enhanced_post_processing(void);
qboolean vk_create_enhanced_post_process_pipelines(void);
void vk_execute_post_processing(const postProcessConfig_t *config);
void vk_update_post_process_config(const postProcessConfig_t *config);

// Individual effect functions
qboolean vk_ssao_pass(const ssaoConfig_t *config);
qboolean vk_ssr_pass(const ssrConfig_t *config);
qboolean vk_bloom_pass(const bloomConfig_t *config);
qboolean vk_dof_pass(const dofConfig_t *config);
qboolean vk_motion_blur_pass(const motionBlurConfig_t *config);
qboolean vk_color_grading_pass(const colorGradingConfig_t *config);
qboolean vk_heat_distortion_pass(const heatDistortionConfig_t *config);

// Pipeline creation helpers
VkPipeline vk_create_compute_pipeline(VkShaderModule computeShader, VkPipelineLayout layout, const char *name);
qboolean vk_create_ssao_pipeline(void);
qboolean vk_create_ssr_pipeline(void);
qboolean vk_create_bloom_pipeline(void);
qboolean vk_create_dof_pipeline(void);
qboolean vk_create_motion_blur_pipeline(void);
qboolean vk_create_velocity_tiles_pipeline(void);
qboolean vk_create_color_grading_pipeline(void);
qboolean vk_create_heat_distortion_pipeline(void);

// CVAR declarations for runtime configuration
extern cvar_t *r_pp_ssao;
extern cvar_t *r_pp_ssao_lissao;
extern cvar_t *r_pp_ssr;
extern cvar_t *r_pp_bloom;
extern cvar_t *r_pp_bloom_kawase;
extern cvar_t *r_pp_dof;
extern cvar_t *r_pp_dof_bokeh_shape;
extern cvar_t *r_pp_motion_blur;
extern cvar_t *r_pp_motion_blur_tiles;
extern cvar_t *r_pp_color_grading;
extern cvar_t *r_pp_heat_distortion;

#ifdef __cplusplus
}
#endif

#endif // VK_POST_PROCESS_H
