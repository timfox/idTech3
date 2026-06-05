/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VkSplat console — Eurographics 2026 / arXiv:2605.00219.
===========================================================================
*/

#include "vksplat/vksplat_console.h"
#include "vksplat/vksplat_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_vksplat_model;
static qboolean vksplat_console_registered = qfalse;

static void Vksplat_Cmd_ModelStatus_f( void )
{
	Com_Printf( "[VkSplat] cl_vksplat_model=%d\n", cl_vksplat_model ? cl_vksplat_model->integer : 0 );
	Com_Printf( "[VkSplat] Commands: vksplat_api, vksplat_model, vksplat_quality\n" );
	Com_Printf( "[VkSplat] Runtime (renderer): vksplat_status, vksplat_train_step, vksplat_reset\n" );
	Com_Printf( "[VkSplat] See docs/VKSPLAT.md — upstream https://github.com/harry7557558/vksplat\n" );
}

static void Vksplat_Cmd_Api_f( void )
{
	Com_Printf( "[VkSplat] Pipeline (Vulkan compute, cross-vendor):\n" );
	Com_Printf( "  1. Projection forward + scan-line tile culling (no false positives)\n" );
	Com_Printf( "  2. 32-bit tile-depth radix sort\n" );
	Com_Printf( "  3. Raster forward + adaptive raster backward (Thompson scheduler)\n" );
	Com_Printf( "  4. Fused L1+SSIM loss gradient + fused projection-backward + Adam\n" );
	Com_Printf( "[VkSplat] Engine: r_vksplat 1 + vid_restart; vksplat_train_step N\n" );
}

static void Vksplat_Cmd_Model_f( void )
{
	vksplat_model_result_t def;
	vksplat_model_result_t mcmc;
	const char *mode;

	mode = ( Cmd_Argc() >= 2 && !Q_stricmp( Cmd_Argv( 1 ), "mcmc" ) ) ? "MCMC" : "default";

	VKSplat_ModelBenchmark( VKSPLAT_DENSIFY_DEFAULT, &def );
	VKSplat_ModelBenchmark( VKSPLAT_DENSIFY_MCMC, &mcmc );

	if ( !Q_stricmp( mode, "MCMC" ) ) {
		Com_Printf( "[VkSplat] MCMC densification (Table 2, RTX 3090 class)\n" );
		Com_Printf( "[VkSplat] GSplat: %.0fs VRAM %.2f GiB\n", mcmc.gsplat.total_s, mcmc.gsplat.vram_gib );
		Com_Printf( "[VkSplat] VkSplat: %.0fs VRAM %.2f GiB speedup %.2fx VRAM %.0f%%\n",
			mcmc.vksplat.total_s, mcmc.vksplat.vram_gib, mcmc.speedup, mcmc.vram_ratio * 100.0f );
	} else {
		Com_Printf( "[VkSplat] Default densification (Table 2)\n" );
		Com_Printf( "[VkSplat] GSplat: %.0fs VRAM %.2f GiB\n", def.gsplat.total_s, def.gsplat.vram_gib );
		Com_Printf( "[VkSplat] VkSplat: %.0fs VRAM %.2f GiB speedup %.2fx VRAM %.0f%%\n",
			def.vksplat.total_s, def.vksplat.vram_gib, def.speedup, def.vram_ratio * 100.0f );
		Com_Printf( "[VkSplat] Paper: ~3.3x faster, ~33%% less VRAM vs GSplat baseline\n" );
	}
}

static void Vksplat_Cmd_Quality_f( void )
{
	const vksplat_quality_preset_t *presets;
	int count;
	int i;
	vksplat_densify_t d;

	d = VKSPLAT_DENSIFY_DEFAULT;
	if ( Cmd_Argc() >= 2 && !Q_stricmp( Cmd_Argv( 1 ), "mcmc" ) ) {
		d = VKSPLAT_DENSIFY_MCMC;
	}

	presets = VKSplat_QualityPresets( d, &count );
	Com_Printf( "[VkSplat] Quality presets (%s, Table 1 style)\n",
		d == VKSPLAT_DENSIFY_MCMC ? "MCMC" : "default" );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "[VkSplat] %-8s PSNR=%.2f SSIM=%.3f LPIPS=%.3f NumGS=%.2fM\n",
			presets[i].label, presets[i].psnr, presets[i].ssim, presets[i].lpips, presets[i].num_gs_m );
	}
}

void Vksplat_ConsoleInit( void )
{
	cl_vksplat_model = Cvar_Get( "cl_vksplat_model", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_vksplat_model,
		"Enable VkSplat 3DGS training model commands (Chen et al., Eurographics 2026)." );

	if ( !cl_vksplat_model->integer ) {
		return;
	}

	if ( !vksplat_console_registered ) {
		Cmd_AddCommand( "vksplat_model_status", Vksplat_Cmd_ModelStatus_f );
		Cmd_AddCommand( "vksplat_api", Vksplat_Cmd_Api_f );
		Cmd_AddCommand( "vksplat_model", Vksplat_Cmd_Model_f );
		Cmd_AddCommand( "vksplat_quality", Vksplat_Cmd_Quality_f );
		vksplat_console_registered = qtrue;
	}

	Com_Printf( "[VkSplat] Model commands enabled (cl_vksplat_model 1)\n" );
}
