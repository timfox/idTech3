#pragma once

#include "qcommon/q_shared.h"

/* VkSplat analytical model — Chen, Ibrahim & Liu, Eurographics 2026 / arXiv:2605.00219 */

typedef enum {
	VKSPLAT_DENSIFY_DEFAULT = 0,
	VKSPLAT_DENSIFY_MCMC
} vksplat_densify_t;

typedef struct {
	float projection_fwd_s;
	float tiling_sort_s;
	float raster_fwd_s;
	float loss_s;
	float raster_bwd_s;
	float proj_bwd_opt_s;
	float densify_s;
	float unaccounted_s;
	float total_s;
	float vram_gib;
} vksplat_timing_t;

typedef struct {
	float speedup;
	float vram_ratio; /* vksplat / gsplat */
	vksplat_timing_t gsplat;
	vksplat_timing_t vksplat;
} vksplat_model_result_t;

void VKSplat_ModelBenchmark( vksplat_densify_t densify, vksplat_model_result_t *out );

typedef struct {
	const char *label;
	float psnr;
	float ssim;
	float lpips;
	float num_gs_m;
} vksplat_quality_preset_t;

const vksplat_quality_preset_t *VKSplat_QualityPresets( vksplat_densify_t densify, int *count );
