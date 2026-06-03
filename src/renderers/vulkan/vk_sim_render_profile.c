#include "tr_local.h"
#include "vk_sim_render_profile.h"

/*
===============
VK_ApplyVolumetricAccurateSettings
===============
One-shot preset for physically grounded froxel fog composite and integration.
Does not enable/disable the fog pass itself (see r_volumetricFog).
===============
*/
void VK_ApplyVolumetricAccurateSettings( void )
{
	int steps = r_volumetricFogSteps ? r_volumetricFogSteps->integer : 48;

	if ( steps < 48 ) {
		steps = 48;
	}

	ri.Cvar_Set( "r_volumetricFogCompositeMode", "0" );
	ri.Cvar_Set( "r_volumetricFogDepthMode", "1" );
	ri.Cvar_Set( "r_volumetricFogSteps", va( "%d", steps ) );
	ri.Cvar_Set( "r_volumetricFogTransmittanceCutoff", "0.002" );
	ri.Cvar_Set( "r_volumetricFogJitter", "1.0" );
	ri.Cvar_Set( "r_volumetricFogTemporalWeight", "0.88" );
	ri.Cvar_Set( "r_fog_shadows", "1" );
	ri.Cvar_Set( "r_volumetricFogAccurate", "1" );
	ri.Printf( PRINT_ALL,
		"...volumetric accurate preset: physical composite, depth-aligned march, %d steps, shadowed froxels\n",
		steps );
}

/*
===============
VK_ApplySimRenderProfile
===============
Apply render settings aligned with AMBF-Vulkan (Allison et al., arXiv:2410.05095)
and simulation-grade volumetrics. Requires vid_restart for latched renderer state.
*/
void VK_ApplySimRenderProfile( int profile )
{
	switch ( profile ) {
		case 1:
			ri.Cvar_Set( "r_fbo", "1" );
			ri.Cvar_Set( "r_hdr", "1" );
			ri.Cvar_Set( "r_pbr", "1" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ext_fxaa", "1" );
			ri.Cvar_Set( "r_tonemap", "1" );
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_bloom", "0" );
			ri.Cvar_Set( "r_ssao", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_volumetricFog", "0" );
			ri.Cvar_Set( "r_volumetricFogAccurate", "0" );
			ri.Cvar_Set( "r_sharpen", "0" );
			ri.Cvar_Set( "r_exposure_auto", "0" );
			ri.Cvar_Set( "r_oit", "0" );
#ifdef USE_VULKAN_RTX
			ri.Cvar_Set( "r_rtx", "1" );
#else
			ri.Cvar_Set( "r_rtx", "0" );
#endif
			ri.Printf( PRINT_ALL,
				"...sim render profile 1 (AMBF-Vulkan): MSAA+FXAA, Reinhard tonemap, PBR, lightweight post stack"
#ifdef USE_VULKAN_RTX
				", r_rtx 1 when RTX build"
#endif
				"\n" );
			break;
		case 2:
			ri.Cvar_Set( "r_fbo", "1" );
			ri.Cvar_Set( "r_hdr", "1" );
			ri.Cvar_Set( "r_pbr", "1" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ext_fxaa", "1" );
			ri.Cvar_Set( "r_tonemap", "1" );
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_bloom", "0" );
			ri.Cvar_Set( "r_ssao", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_sharpen", "0" );
			ri.Cvar_Set( "r_exposure_auto", "0" );
			ri.Cvar_Set( "r_oit", "0" );
			VK_ApplyVolumetricAccurateSettings();
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_volumetricFogQuality", "3" );
			ri.Cvar_Set( "r_volumetricFogResolutionScale", "1.0" );
			ri.Cvar_Set( "r_volumetricFogSteps", "64" );
			ri.Cvar_Set( "r_volumetricFogSunIntensity", "1.25" );
			ri.Cvar_Set( "r_volumetricFogAniso", "0.6" );
#ifdef USE_VULKAN_RTX
			ri.Cvar_Set( "r_rtx", "1" );
#else
			ri.Cvar_Set( "r_rtx", "0" );
#endif
			ri.Printf( PRINT_ALL,
				"...sim render profile 2 (volumetric): MSAA+FXAA, physical fog composite, 64 march steps, shadowed froxels, temporal blend\n" );
			break;
		default:
			ri.Printf( PRINT_ALL, "Unknown sim render profile %d (use 1=AMBF lightweight, 2=volumetric accurate)\n", profile );
			break;
	}
}
