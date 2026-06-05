/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VkSplat resource model (Table 2, arXiv:2605.00219).
===========================================================================
*/

#include "vksplat/vksplat_model.h"

static void VKSplat_TimingGSplatDefault( vksplat_timing_t *t )
{
	if ( !t ) {
		return;
	}
	t->projection_fwd_s = 94.0f;
	t->tiling_sort_s = 42.0f;
	t->raster_fwd_s = 69.0f;
	t->loss_s = 103.0f;
	t->raster_bwd_s = 246.0f;
	t->proj_bwd_opt_s = 61.0f + 398.0f;
	t->densify_s = 31.0f;
	t->unaccounted_s = 341.0f;
	t->total_s = 1384.0f;
	t->vram_gib = 4.56f;
}

static void VKSplat_TimingVkSplatDefault( vksplat_timing_t *t )
{
	if ( !t ) {
		return;
	}
	t->projection_fwd_s = 19.0f;
	t->tiling_sort_s = 25.0f;
	t->raster_fwd_s = 25.0f;
	t->loss_s = 32.0f;
	t->raster_bwd_s = 130.0f;
	t->proj_bwd_opt_s = 131.0f;
	t->densify_s = 5.0f;
	t->unaccounted_s = 43.0f;
	t->total_s = 412.0f;
	t->vram_gib = 3.01f;
}

static void VKSplat_TimingGSplatMCMC( vksplat_timing_t *t )
{
	if ( !t ) {
		return;
	}
	t->projection_fwd_s = 40.0f;
	t->tiling_sort_s = 41.0f;
	t->raster_fwd_s = 71.0f;
	t->loss_s = 110.0f;
	t->raster_bwd_s = 268.0f;
	t->proj_bwd_opt_s = 33.0f + 172.0f;
	t->densify_s = 61.0f;
	t->unaccounted_s = 200.0f;
	t->total_s = 995.0f;
	t->vram_gib = 1.37f;
}

static void VKSplat_TimingVkSplatMCMC( vksplat_timing_t *t )
{
	if ( !t ) {
		return;
	}
	t->projection_fwd_s = 8.0f;
	t->tiling_sort_s = 27.0f;
	t->raster_fwd_s = 25.0f;
	t->loss_s = 32.0f;
	t->raster_bwd_s = 120.0f;
	t->proj_bwd_opt_s = 46.0f;
	t->densify_s = 3.0f;
	t->unaccounted_s = 23.0f;
	t->total_s = 285.0f;
	t->vram_gib = 0.93f;
}

void VKSplat_ModelBenchmark( vksplat_densify_t densify, vksplat_model_result_t *out )
{
	if ( !out ) {
		return;
	}

	if ( densify == VKSPLAT_DENSIFY_MCMC ) {
		VKSplat_TimingGSplatMCMC( &out->gsplat );
		VKSplat_TimingVkSplatMCMC( &out->vksplat );
	} else {
		VKSplat_TimingGSplatDefault( &out->gsplat );
		VKSplat_TimingVkSplatDefault( &out->vksplat );
	}

	out->speedup = out->gsplat.total_s / out->vksplat.total_s;
	out->vram_ratio = out->vksplat.vram_gib / out->gsplat.vram_gib;
}

static const vksplat_quality_preset_t quality_default[] = {
	{ "bicycle", 29.2f, 0.878f, 0.124f, 3.04f },
	{ "garden", 29.25f, 0.879f, 0.125f, 3.06f },
	{ "stump", 29.20f, 0.878f, 0.124f, 3.02f }
};

static const vksplat_quality_preset_t quality_mcmc[] = {
	{ "bicycle", 29.40f, 0.881f, 0.130f, 1.00f },
	{ "garden", 29.43f, 0.881f, 0.129f, 1.00f },
	{ "stump", 29.39f, 0.881f, 0.130f, 1.00f }
};

const vksplat_quality_preset_t *VKSplat_QualityPresets( vksplat_densify_t densify, int *count )
{
	if ( count ) {
		*count = 3;
	}
	return ( densify == VKSPLAT_DENSIFY_MCMC ) ? quality_mcmc : quality_default;
}
