/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Water inspector panel for Arc Blanc ocean controls.
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"
#include "vk_imgui_draw_defaults.hpp"

static void VkImgui_ResetOceanDefaults( void )
{
	ri.Cvar_Set( "r_arcBlanc", "0" );
	ri.Cvar_Set( "r_arcBlancDraw", "1" );
	ri.Cvar_Set( "r_arcBlancWind", "20" );
	ri.Cvar_Set( "r_arcBlancFetch", "1000" );
	ri.Cvar_Set( "r_arcBlancSwell", "0.5" );
	ri.Cvar_Set( "r_arcBlancDirectional", "1" );
	ri.Cvar_Set( "r_arcBlancSpread", "0" );
	ri.Cvar_Set( "r_arcBlancAmplitude", "1" );
	ri.Cvar_Set( "r_arcBlancHeightScale", "1" );
	ri.Cvar_Set( "r_arcBlancChoppiness", "1" );
	ri.Cvar_Set( "r_arcBlancWaveSpeed", "1" );
	ri.Cvar_Set( "r_arcBlancGustStrength", "0" );
	ri.Cvar_Set( "r_arcBlancGustSpeed", "0.5" );
	ri.Cvar_Set( "r_arcBlancUpdateHz", "0" );
	ri.Cvar_Set( "r_arcBlancMaxSubsteps", "4" );
	ri.Cvar_Set( "r_arcBlancGrid", "128" );
	ri.Cvar_Set( "r_arcBlancTile", "256" );
	ri.Cvar_Set( "r_arcBlancMeshDiv", "48" );
	ri.Cvar_Set( "r_arcBlancTileRadius", "1" );
	ri.Cvar_Set( "r_arcBlancFollowCamera", "1" );
	ri.Cvar_Set( "r_arcBlancNormalStrength", "1" );
	ri.Cvar_Set( "r_arcBlancFoam", "1" );
	ri.Cvar_Set( "r_arcBlancFoamIntensity", "0.35" );
	ri.Cvar_Set( "r_arcBlancFoamThreshold", "0.28" );
	ri.Cvar_Set( "r_arcBlancFoamSoftness", "1.5" );
	ri.Cvar_Set( "r_arcBlancTileBreak", "1" );
	ri.Cvar_Set( "r_arcBlancTileBreakOffset", "-500" );
	ri.Cvar_Set( "r_arcBlancTileBreakBlend", "0.45" );
	ri.Cvar_Set( "r_arcBlancTileBreakCell", "768" );
	ri.Cvar_Set( "r_arcBlancLakeMode", "0" );
	ri.Cvar_Set( "r_arcBlancLakeCenter", "0 0 0" );
	ri.Cvar_Set( "r_arcBlancLakeExtents", "1024 1024" );
	ri.Cvar_Set( "r_arcBlancLakeAngle", "0" );
}

static void VkImgui_ApplyUnderwaterLook( void )
{
	ri.Cvar_Set( "r_volumetricFog", "1" );
	ri.Cvar_Set( "r_volumetricFogColorMode", "1" );
	ri.Cvar_Set( "r_volumetricFogTint", "0.12 0.32 0.42" );
	ri.Cvar_Set( "r_volumetricFogDensity", "1.6" );
	ri.Cvar_Set( "r_volumetricFogHeightFalloff", "0.02" );
	ri.Cvar_Set( "r_volumetricFogIntensity", "2.4" );
	ri.Cvar_Set( "r_volumetricFogAmbientIntensity", "0.5" );
	ri.Cvar_Set( "r_volumetricFogSunIntensity", "0.2" );
	ri.Cvar_Set( "r_volumetricFogNoiseStrength", "0.6" );
}

static void VkImgui_ClearUnderwaterLook( void )
{
	ri.Cvar_Set( "r_volumetricFogTint", "1.08 1.00 0.72" );
	ri.Cvar_Set( "r_volumetricFogDensity", "0.6" );
	ri.Cvar_Set( "r_volumetricFogHeightFalloff", "0.015" );
	ri.Cvar_Set( "r_volumetricFogIntensity", "1.5" );
	ri.Cvar_Set( "r_volumetricFogAmbientIntensity", "1.25" );
	ri.Cvar_Set( "r_volumetricFogSunIntensity", "1.25" );
	ri.Cvar_Set( "r_volumetricFogNoiseStrength", "0.85" );
}

static void VkImgui_OceanSlider( const char *label, const char *cvar, float minValue, float maxValue,
	const char *fmt = "%.3f" )
{
	VkImgui_CvarSlider( label, cvar, VkImgui_CvarFloat( cvar ), minValue, maxValue, fmt );
}

static void VkImgui_OceanSliderInt( const char *label, const char *cvar, int minValue, int maxValue )
{
	int value = ri.Cvar_VariableIntegerValue( cvar );
	if ( ImGui::SliderInt( label, &value, minValue, maxValue ) ) {
		ri.Cvar_Set( cvar, va( "%d", value ) );
	}
}

static void VkImgui_OceanPresetButtonRow( void )
{
	if ( ImGui::Button( "Calm" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "arc_blanc_preset calm\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Lake" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "arc_blanc_preset lake\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Ocean" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "arc_blanc_preset ocean\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Storm" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "arc_blanc_preset storm\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Cinematic" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "arc_blanc_preset cinematic\n" );
	}
}

extern "C" void VkImgui_DrawOceanPanel( void )
{
	if ( !vkWindows.ocean.open ) {
		return;
	}

	ImGui::Begin( "Water", (bool *)&vkWindows.ocean.open );
	VkImgui_DrawDefaultsConfirmation(
		"Defaults##Water",
		"ConfirmDefaultsWater",
		"Water",
		VkImgui_ResetOceanDefaults
	);
	ImGui::SameLine();
	if ( ImGui::Button( "Underwater Look" ) ) {
		VkImgui_ApplyUnderwaterLook();
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Clear Underwater" ) ) {
		VkImgui_ClearUnderwaterLook();
	}
	ImGui::SameLine();
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Arc Blanc workflow panel. Presets call arc_blanc_preset; sliders write cvars directly. "
			"Underwater buttons tune volumetric fog for submerged shots." );
	}
	ImGui::Separator();
	ImGui::BeginChild( "WaterScrollRegion", ImVec2( 0.0f, 0.0f ), qfalse, ImGuiWindowFlags_AlwaysVerticalScrollbar );

	{
		int enabled = ri.Cvar_VariableIntegerValue( "r_arcBlanc" );
		bool on = ( enabled != 0 );
		if ( ImGui::Checkbox( "Enable Arc Blanc", &on ) ) {
			ri.Cvar_Set( "r_arcBlanc", on ? "1" : "0" );
		}
		ImGui::SameLine();
		int drawEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancDraw" );
		bool drawOn = ( drawEnabled != 0 );
		if ( ImGui::Checkbox( "Draw Surface", &drawOn ) ) {
			ri.Cvar_Set( "r_arcBlancDraw", drawOn ? "1" : "0" );
		}
	}

	if ( ImGui::CollapsingHeader( "Presets", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		VkImgui_OceanPresetButtonRow();
		ImGui::TextDisabled( "Console: arc_blanc_status, arc_blanc_sample <x> <z>, arc_blanc_preset <name>" );
	}

	if ( ImGui::CollapsingHeader( "Wave Shape", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		VkImgui_OceanSlider( "Wind Speed", "r_arcBlancWind", 0.0f, 40.0f, "%.1f" );
		VkImgui_OceanSlider( "Fetch", "r_arcBlancFetch", 1.0f, 4000.0f, "%.0f" );
		VkImgui_OceanSlider( "Swell", "r_arcBlancSwell", 0.0f, 1.0f );
		VkImgui_OceanSlider( "Directionality", "r_arcBlancDirectional", 0.0f, 1.0f );
		VkImgui_OceanSlider( "Spread", "r_arcBlancSpread", 0.0f, 1.0f );
		VkImgui_OceanSlider( "Amplitude", "r_arcBlancAmplitude", 0.0f, 3.0f, "%.2f" );
		VkImgui_OceanSlider( "Height Scale", "r_arcBlancHeightScale", 0.0f, 3.0f, "%.2f" );
		VkImgui_OceanSlider( "Choppiness", "r_arcBlancChoppiness", 0.0f, 3.0f, "%.2f" );
		VkImgui_OceanSlider( "Wave Speed", "r_arcBlancWaveSpeed", 0.0f, 4.0f, "%.2f" );
		VkImgui_OceanSlider( "Wind Direction", "r_arcBlancWindDir", -180.0f, 180.0f, "%.0f" );
		VkImgui_OceanSlider( "Sea Level", "r_arcBlancSeaLevel", -2048.0f, 2048.0f, "%.0f" );
	}

	if ( ImGui::CollapsingHeader( "Surface Detail", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		int foamEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancFoam" );
		bool foamOn = ( foamEnabled != 0 );
		if ( ImGui::Checkbox( "Enable Crest Foam", &foamOn ) ) {
			ri.Cvar_Set( "r_arcBlancFoam", foamOn ? "1" : "0" );
		}
		VkImgui_OceanSlider( "Foam Intensity", "r_arcBlancFoamIntensity", 0.0f, 1.5f, "%.2f" );
		VkImgui_OceanSlider( "Foam Threshold", "r_arcBlancFoamThreshold", 0.0f, 1.0f, "%.2f" );
		VkImgui_OceanSlider( "Foam Softness", "r_arcBlancFoamSoftness", 0.05f, 4.0f, "%.2f" );
		VkImgui_OceanSlider( "Normal Strength", "r_arcBlancNormalStrength", 0.1f, 3.0f, "%.2f" );
		VkImgui_OceanSlider( "Gust Strength", "r_arcBlancGustStrength", 0.0f, 1.0f, "%.2f" );
		VkImgui_OceanSlider( "Gust Speed", "r_arcBlancGustSpeed", 0.0f, 4.0f, "%.2f" );
	}

	if ( ImGui::CollapsingHeader( "Tiling And Layout", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		int tileBreakEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancTileBreak" );
		bool tileBreakOn = ( tileBreakEnabled != 0 );
		if ( ImGui::Checkbox( "Tile Break", &tileBreakOn ) ) {
			ri.Cvar_Set( "r_arcBlancTileBreak", tileBreakOn ? "1" : "0" );
		}
		ImGui::SameLine();
		int followEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancFollowCamera" );
		bool followOn = ( followEnabled != 0 );
		if ( ImGui::Checkbox( "Follow Camera", &followOn ) ) {
			ri.Cvar_Set( "r_arcBlancFollowCamera", followOn ? "1" : "0" );
		}
		VkImgui_OceanSlider( "Patch Size", "r_arcBlancTile", 32.0f, 2048.0f, "%.0f" );
		VkImgui_OceanSlider( "Tile Break Offset", "r_arcBlancTileBreakOffset", -4096.0f, 4096.0f, "%.0f" );
		VkImgui_OceanSlider( "Tile Break Blend", "r_arcBlancTileBreakBlend", 0.0f, 1.0f, "%.2f" );
		VkImgui_OceanSlider( "Tile Break Cell", "r_arcBlancTileBreakCell", 64.0f, 4096.0f, "%.0f" );
		VkImgui_OceanSlider( "Tile Radius", "r_arcBlancTileRadius", 0.0f, 6.0f, "%.0f" );
	}

	if ( ImGui::CollapsingHeader( "Lake Mode" ) ) {
		int lakeEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancLakeMode" );
		bool lakeOn = ( lakeEnabled != 0 );
		if ( ImGui::Checkbox( "Enable Lake Mode", &lakeOn ) ) {
			ri.Cvar_Set( "r_arcBlancLakeMode", lakeOn ? "1" : "0" );
		}
		{
			char centerBuf[64];
			char extentsBuf[64];
			Q_strncpyz( centerBuf, ri.Cvar_VariableString( "r_arcBlancLakeCenter" ), sizeof( centerBuf ) );
			Q_strncpyz( extentsBuf, ri.Cvar_VariableString( "r_arcBlancLakeExtents" ), sizeof( extentsBuf ) );
			if ( ImGui::InputText( "Center (x y z)", centerBuf, sizeof( centerBuf ) ) ) {
				ri.Cvar_Set( "r_arcBlancLakeCenter", centerBuf );
			}
			if ( ImGui::InputText( "Extents (x z)", extentsBuf, sizeof( extentsBuf ) ) ) {
				ri.Cvar_Set( "r_arcBlancLakeExtents", extentsBuf );
			}
		}
		VkImgui_OceanSlider( "Lake Angle", "r_arcBlancLakeAngle", -180.0f, 180.0f, "%.0f" );
	}

	if ( ImGui::CollapsingHeader( "Performance", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		int gpuEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancGpu" );
		bool gpuOn = ( gpuEnabled != 0 );
		if ( ImGui::Checkbox( "GPU FFT", &gpuOn ) ) {
			ri.Cvar_Set( "r_arcBlancGpu", gpuOn ? "1" : "0" );
		}
		ImGui::SameLine();
		int velocityGpuEnabled = ri.Cvar_VariableIntegerValue( "r_arcBlancGpuVelocity" );
		bool velocityGpuOn = ( velocityGpuEnabled != 0 );
		if ( ImGui::Checkbox( "GPU Velocity", &velocityGpuOn ) ) {
			ri.Cvar_Set( "r_arcBlancGpuVelocity", velocityGpuOn ? "1" : "0" );
		}
		VkImgui_OceanSliderInt( "Update Hz", "r_arcBlancUpdateHz", 0, 120 );
		VkImgui_OceanSliderInt( "Max Substeps", "r_arcBlancMaxSubsteps", 1, 8 );
		{
			const char *gridModes[] = { "32", "64", "128", "256" };
			int currentGrid = ri.Cvar_VariableIntegerValue( "r_arcBlancGrid" );
			int gridIndex = 2;
			switch ( currentGrid ) {
				case 32: gridIndex = 0; break;
				case 64: gridIndex = 1; break;
				case 128: gridIndex = 2; break;
				case 256: gridIndex = 3; break;
				default: gridIndex = 2; break;
			}
			if ( ImGui::Combo( "Grid Resolution", &gridIndex, gridModes, 4 ) ) {
				static const int gridValues[] = { 32, 64, 128, 256 };
				ri.Cvar_Set( "r_arcBlancGrid", va( "%d", gridValues[gridIndex] ) );
			}
		}
		VkImgui_OceanSliderInt( "Mesh Divisions", "r_arcBlancMeshDiv", 8, 128 );
		VkImgui_OceanSlider( "Wake Scale", "r_arcBlancWake", 0.0f, 4.0f, "%.2f" );
		ImGui::TextDisabled( "Use lower Update Hz and Mesh Divisions for gameplay, higher values for close shots." );
	}

	ImGui::EndChild();
	ImGui::End();
}

#endif /* USE_IMGUI */
