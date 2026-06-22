/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Floating simulation render debug HUD (post chain + volumetric GPU timings).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"
#include "../vk_sim_render_debug.h"

extern "C" void VkImgui_DrawSimRenderDebugHud( void )
{
	int mode = ri.Cvar_VariableIntegerValue( "r_simRenderDebug" );
	if ( mode < 2 ) {
		return;
	}

	vk_sim_render_debug_stats_t stats;
	VK_SimRenderDebugFillStats( &stats );

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	ImGui::SetNextWindowPos( ImVec2( 12.0f, 12.0f ), ImGuiCond_FirstUseEver );
	ImGui::SetNextWindowBgAlpha( 0.72f );

	if ( !ImGui::Begin( "Sim Render Debug", nullptr, flags ) ) {
		ImGui::End();
		return;
	}

	ImGui::Text( "Profile: %d   MSAA: %dx", stats.profile, stats.msaaSamples );
	ImGui::Text( "AA: %s   Tonemap: %d   Bloom: %s",
		stats.fxaaActive ? "FXAA" : ( stats.smaaActive ? "SMAA" : "none" ),
		stats.tonemapMode,
		stats.bloomActive ? "on" : "off" );
	ImGui::Text( "Fog: %s%s   int=%d   steps=%d quality=%d",
		stats.fogActive ? "on" : "off",
		stats.fogAccurate ? " (accurate)" : "",
		stats.fogIntegration,
		stats.fogSteps,
		stats.fogQuality );

	if ( stats.fogActive ) {
		ImGui::SeparatorText( "Volumetric GPU (ms)" );
		if ( !stats.perfTimestamps ) {
			ImGui::TextDisabled( "Waiting for timestamps..." );
		} else {
			ImGui::Text( "Total: %.3f   Fluid: %.3f", stats.fogTotalMs, stats.fogFluidMs );
			ImGui::Text( "Clear: %.3f   Global: %.3f   Volume: %.3f",
				stats.fogClearMs, stats.fogGlobalMs, stats.fogVolumeMs );
			ImGui::Text( "Sun: %.3f   Local: %.3f   Temporal: %.3f   Composite: %.3f",
				stats.fogSunMs, stats.fogLocalMs, stats.fogTemporalMs, stats.fogCompositeMs );
		}
	}

	ImGui::End();
}

#endif /* USE_IMGUI */
