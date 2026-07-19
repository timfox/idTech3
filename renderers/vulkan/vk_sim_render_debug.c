#include "tr_local.h"
#include "vk.h"
#include "vk_sim_render_debug.h"

/*
===============
vk_volumetric_perf_wanted
===============
Record/read GPU timestamps when explicit perf timers or sim debug HUD is on.
===============
*/
qboolean vk_volumetric_perf_wanted( void )
{
	if ( r_volumetricFogPerfTimers && r_volumetricFogPerfTimers->integer ) {
		return qtrue;
	}
	if ( r_simRenderDebug && r_simRenderDebug->integer ) {
		return qtrue;
	}
	return qfalse;
}

static int vk_sim_render_debug_msaa_samples( void )
{
	if ( !r_ext_multisample ) {
		return 1;
	}
	{
		const int ms = r_ext_multisample->integer;
		if ( ms <= 1 ) {
			return 1;
		}
		if ( ms >= 8 ) {
			return 8;
		}
		if ( ms >= 4 ) {
			return 4;
		}
		return 2;
	}
}

static float vk_sim_render_debug_stage_ms( vk_volumetry_query_index_t afterStage )
{
	const int idx = (int)afterStage - (int)VK_VOLUMETRY_QUERY_FOG_START;
	if ( idx < 0 || idx >= (int)( sizeof( vk.volumetric_stage_ms ) / sizeof( vk.volumetric_stage_ms[0] ) ) ) {
		return 0.0f;
	}
	return vk.volumetric_stage_ms[idx];
}

/*
===============
VK_SimRenderDebugFillStats
===============
*/
void VK_SimRenderDebugFillStats( vk_sim_render_debug_stats_t *out )
{
	if ( !out ) {
		return;
	}

	Com_Memset( out, 0, sizeof( *out ) );

	out->profile = r_simRenderProfile ? r_simRenderProfile->integer : 0;
	out->msaaSamples = vk_sim_render_debug_msaa_samples();
	out->tonemapMode = r_tonemap ? r_tonemap->integer : 0;
	out->fxaaActive = vk.fxaaActive;
	out->smaaActive = vk.smaaActive;
	out->bloomActive = ( r_bloom && r_bloom->integer ) ? qtrue : qfalse;
	out->fogActive = ( r_volumetricFog && r_volumetricFog->integer ) ? qtrue : qfalse;
	out->fogAccurate = ( r_volumetricFogAccurate && r_volumetricFogAccurate->integer ) ? qtrue : qfalse;
	out->fogIntegration = r_volumetricFogIntegration ? r_volumetricFogIntegration->integer : 0;
	out->fogSteps = r_volumetricFogSteps ? r_volumetricFogSteps->integer : 0;
	out->fogQuality = r_volumetricFogQuality ? r_volumetricFogQuality->integer : 0;
	out->perfTimestamps = vk_volumetric_perf_wanted();

	out->fogTotalMs = vk.volumetric_total_ms;
	out->fogFluidMs = vk.volumetric_fluid_ms;
	out->fogClearMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_CLEAR );
	out->fogGlobalMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_GLOBAL_DENSITY );
	out->fogVolumeMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_VOLUME_DENSITY );
	out->fogSunMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_SUN );
	out->fogLocalMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_LOCAL );
	out->fogTemporalMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_TEMPORAL );
	out->fogCompositeMs = vk_sim_render_debug_stage_ms( VK_VOLUMETRY_QUERY_AFTER_COMPOSITE );
}

static const char *vk_sim_render_debug_aa_label( const vk_sim_render_debug_stats_t *stats )
{
	if ( stats->fxaaActive ) {
		return "FXAA";
	}
	if ( stats->smaaActive ) {
		return "SMAA";
	}
	return "none";
}

static const char *vk_sim_render_debug_tonemap_label( int mode )
{
	switch ( mode ) {
		case 0: return "none";
		case 1: return "Reinhard";
		case 2: return "ACES";
		case 3: return "Filmic";
		case 4: return "AgX";
		default: return "custom";
	}
}

static void vk_sim_render_debug_print_line( const vk_sim_render_debug_stats_t *stats )
{
	ri.Printf( PRINT_ALL,
		"[sim] profile=%d MSAA=%dx AA=%s tonemap=%s bloom=%s fog=%s%s int=%d steps=%d q=%d | fog GPU total=%.2fms fluid=%.2f clear=%.2f sun=%.2f local=%.2f temporal=%.2f composite=%.2f\n",
		stats->profile,
		stats->msaaSamples,
		vk_sim_render_debug_aa_label( stats ),
		vk_sim_render_debug_tonemap_label( stats->tonemapMode ),
		stats->bloomActive ? "on" : "off",
		stats->fogActive ? "on" : "off",
		stats->fogAccurate ? " (accurate)" : "",
		stats->fogIntegration,
		stats->fogSteps,
		stats->fogQuality,
		stats->fogTotalMs,
		stats->fogFluidMs,
		stats->fogClearMs,
		stats->fogSunMs,
		stats->fogLocalMs,
		stats->fogTemporalMs,
		stats->fogCompositeMs );
}

/*
===============
VK_SimRenderDebugFrameEnd
===============
Throttled console stats when r_simRenderDebug 1 (or 2 without ImGui).
===============
*/
void VK_SimRenderDebugFrameEnd( void )
{
	vk_sim_render_debug_stats_t stats;

	if ( !r_simRenderDebug || r_simRenderDebug->integer <= 0 ) {
		return;
	}

	VK_SimRenderDebugFillStats( &stats );

	if ( r_simRenderDebug->integer >= 2 ) {
#ifdef USE_IMGUI
		if ( r_imgui && r_imgui->integer ) {
			return;
		}
#endif
	}

	{
		const int interval = 60;
		if ( ( vk.frame_count % (uint32_t)interval ) != 0u ) {
			return;
		}
	}

	vk_sim_render_debug_print_line( &stats );
}

/*
===============
VK_SimRenderDebugStartupLog
===============
*/
void VK_SimRenderDebugStartupLog( void )
{
	if ( !r_simRenderDebug || r_simRenderDebug->integer <= 0 ) {
		return;
	}

	ri.Printf( PRINT_ALL,
		"...sim render debug: mode %d (0=off 1=console 2=ImGui HUD; mode 2 needs r_imgui 1)\n",
		r_simRenderDebug->integer );
}
