#ifndef __VK_CONFIG_H__
#define __VK_CONFIG_H__

#include "../renderercommon/tr_public.h"

// Vulkan CVAR declarations (extern in implementation)
extern cvar_t *r_vrs;
extern cvar_t *r_vrs_mode;
extern cvar_t *r_vrs_center_radius;
extern cvar_t *r_vrs_falloff_start;
extern cvar_t *r_vrs_min_rate;
extern cvar_t *r_vrs_max_rate;
extern cvar_t *r_vk_profiling;
extern cvar_t *r_vk_debug_overlay;
extern cvar_t *r_vk_disableScreenMap;
extern cvar_t *r_procDressing;
extern cvar_t *r_materialSystem;
extern cvar_t *r_frameTelemetry;
extern cvar_t *r_bloom;
extern cvar_t *r_dlss;
extern cvar_t *r_dlss_quality;
extern cvar_t *r_dlss_sharpening;
extern cvar_t *r_styleTransfer;
extern cvar_t *r_styleStrength;
extern cvar_t *r_styleLevels;
extern cvar_t *r_styleEdge;
extern cvar_t *r_postprocess_workgroup;
extern cvar_t *r_postprocess_compute;
extern cvar_t *r_postQuality;
extern cvar_t *r_hdr;
extern cvar_t *r_tonemapMode;
extern cvar_t *r_tonemapExposure;
extern cvar_t *r_gamma;
extern cvar_t *r_greyscale;
extern cvar_t *r_dither;
extern cvar_t *r_vk_hotReload;

// Configuration management functions
void vk_config_init(void);
void vk_config_shutdown(void);
qboolean vk_config_validate(void);
qboolean vk_config_has_advanced_features(void);

#endif // __VK_CONFIG_H__
