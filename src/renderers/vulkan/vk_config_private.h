#ifndef __VK_CONFIG_PRIVATE_H__
#define __VK_CONFIG_PRIVATE_H__

#include "../renderercommon/tr_public.h"

// CVAR extern declarations for Vulkan renderer (private header without Vulkan dependencies)
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

#endif // __VK_CONFIG_PRIVATE_H__
