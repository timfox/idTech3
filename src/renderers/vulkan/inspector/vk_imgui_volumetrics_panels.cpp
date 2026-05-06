/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Volumetric fog / fluid inspector panel.
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"
#include "vk_imgui_draw_defaults.hpp"

typedef struct {
	float density;
	float heightFalloff;
	float scatter;
	float viscosity;
	float dissipation;
} vkVolumetricsPanelState_t;

/* Engine cvar defaults for Volumetrics reset (must match tr_init.c) */
static const float vkVolumetricsCvarDefaults[] = {
	0.6f,    /* r_volumetricFogDensity */
	0.015f,  /* r_volumetricFogHeightFalloff */
	1.5f,    /* r_volumetricFogIntensity */
	0.95f,   /* r_volumetricFogAlbedo */
	1.0f,    /* r_volumetricFogExtinctionScale */
	0.05f,   /* r_fogFluidViscosity */
	0.985f,  /* r_fogFluidDissipation */
};

static const vkVolumetricsPanelState_t vkVolumetricsDefaults = {
	0.02f, 0.04f, 1.0f, 0.0001f, 0.995f
};

static vkVolumetricsPanelState_t vkVolumetricsState = vkVolumetricsDefaults;

static void VkImgui_ResetVolumetricsDefaults( void )
{
	vkVolumetricsState = vkVolumetricsDefaults;
	ri.Cvar_SetValue( "r_volumetricFogDensity", vkVolumetricsCvarDefaults[0] );
	ri.Cvar_SetValue( "r_volumetricFogHeightFalloff", vkVolumetricsCvarDefaults[1] );
	ri.Cvar_SetValue( "r_volumetricFogIntensity", vkVolumetricsCvarDefaults[2] );
	ri.Cvar_SetValue( "r_volumetricFogAlbedo", vkVolumetricsCvarDefaults[3] );
	ri.Cvar_SetValue( "r_volumetricFogExtinctionScale", vkVolumetricsCvarDefaults[4] );
	ri.Cvar_SetValue( "r_fogFluidViscosity", vkVolumetricsCvarDefaults[5] );
	ri.Cvar_SetValue( "r_fogFluidDissipation", vkVolumetricsCvarDefaults[6] );
}

static void VkImgui_VolumetricsSlider( const char *label, const char *cvar, float v, float vMin, float vMax,
	const char *fmt = "%.3f" )
{
	VkImgui_CvarSlider( label, cvar, v, vMin, vMax, fmt );
}

extern "C" void VkImgui_DrawVolumetricsPanel(void) {
	if (!vkWindows.volumetrics.open) return;
	ImGui::Begin("Volumetrics", (bool *)&vkWindows.volumetrics.open);
	VkImgui_DrawDefaultsConfirmation(
		"Defaults##Volumetrics",
		"ConfirmDefaultsVolumetrics",
		"Volumetrics",
		VkImgui_ResetVolumetricsDefaults
	);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Apply baseline values. All controls drive r_volumetricFog* / r_fogFluid* cvars." );
	}
	ImGui::Separator();
	ImGui::BeginChild("VolumetricsScrollRegion", ImVec2(0.0f, 0.0f), qfalse, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	/* Enable toggle */
	{
		int on = ri.Cvar_VariableIntegerValue( "r_volumetricFog" );
		bool enabled = ( on != 0 );
		if ( ImGui::Checkbox( "Enable Volumetric Fog", &enabled ) ) {
			ri.Cvar_Set( "r_volumetricFog", enabled ? "1" : "0" );
		}
	}

	if (ImGui::CollapsingHeader("Volumetric Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
		float density = VkImgui_CvarFloat( "r_volumetricFogDensity" );
		float heightFalloff = VkImgui_CvarFloat( "r_volumetricFogHeightFalloff" );
		float intensity = VkImgui_CvarFloat( "r_volumetricFogIntensity" );
		float albedo = VkImgui_CvarFloat( "r_volumetricFogAlbedo" );
		float extinctionScale = VkImgui_CvarFloat( "r_volumetricFogExtinctionScale" );
		VkImgui_VolumetricsSlider( "Density", "r_volumetricFogDensity", density, 0.0f, 5.0f );
		VkImgui_VolumetricsSlider( "Height Falloff", "r_volumetricFogHeightFalloff", heightFalloff, 0.0f, 5.0f );
		VkImgui_VolumetricsSlider( "Scatter Intensity", "r_volumetricFogIntensity", intensity, 0.0f, 50.0f );
		VkImgui_VolumetricsSlider( "Albedo", "r_volumetricFogAlbedo", albedo, 0.0f, 1.0f );
		VkImgui_VolumetricsSlider( "Extinction Scale", "r_volumetricFogExtinctionScale", extinctionScale, 0.1f, 10.0f );
		float blendDist = VkImgui_CvarFloat( "r_volumetricFogBlendDistance" );
		VkImgui_VolumetricsSlider( "Volume Blend Distance", "r_volumetricFogBlendDistance", blendDist, 0.0f, 256.0f, "%.0f" );
		int sliceMode = (int)VkImgui_CvarFloat( "r_volumetricFogSliceMode" );
		if ( sliceMode < 0 || sliceMode > 2 ) sliceMode = 0;
		const char *sliceModes[] = { "Exponential (near)", "Linear", "Logarithmic (far)" };
		if ( ImGui::Combo( "Slice Distribution", &sliceMode, sliceModes, 3 ) ) {
			ri.Cvar_SetValue( "r_volumetricFogSliceMode", (float)sliceMode );
		}
		ImGui::SameLine();
		ImGui::SetTooltip( "Depth-slice allocation: Exponential=more near camera, Linear=equal spacing, Logarithmic=more in distance." );
	}

	if (ImGui::CollapsingHeader("Sphere Volumes (Debug)")) {
		bool sphereOn = ( ri.Cvar_Get( "r_volumetricFogSphere", "0", 0 )->integer != 0 );
		if ( ImGui::Checkbox( "Enable Debug Sphere", &sphereOn ) ) {
			ri.Cvar_SetValue( "r_volumetricFogSphere", sphereOn ? 1.0f : 0.0f );
		}
		if ( sphereOn ) {
			char buf[64];
			Q_strncpyz( buf, ri.Cvar_VariableString( "r_volumetricFogSphereCenter" ), sizeof( buf ) );
			if ( ImGui::InputText( "Center (x y z)", buf, sizeof( buf ) ) ) {
				ri.Cvar_Set( "r_volumetricFogSphereCenter", buf );
			}
			float rad = VkImgui_CvarFloat( "r_volumetricFogSphereRadius" );
			VkImgui_VolumetricsSlider( "Radius", "r_volumetricFogSphereRadius", rad, 1.0f, 2048.0f, "%.0f" );
			float dens = VkImgui_CvarFloat( "r_volumetricFogSphereDensity" );
			VkImgui_VolumetricsSlider( "Density", "r_volumetricFogSphereDensity", dens, 0.0f, 1.0f, "%.4f" );
		}
	}

	if (ImGui::CollapsingHeader("Quality Tiers")) {
		const char *tierNames[] = { "Full Froxel", "Reduced", "Mobile Height", "Mobile Sprites", "Off" };
		int tier = (int)VkImgui_CvarFloat( "r_volumetricFogTier" );
		if ( tier < 0 || tier > 4 ) tier = 0;
		if ( ImGui::Combo( "Fog Tier", &tier, tierNames, 5 ) ) {
			ri.Cvar_SetValue( "r_volumetricFogTier", (float)tier );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "0=full volumetric, 1=reduced, 2=mobile height fog, 3=mobile sprites, 4=off" );
		}
	}

	if (ImGui::CollapsingHeader("Fluid Simulation")) {
		float viscosity = VkImgui_CvarFloat( "r_fogFluidViscosity" );
		float dissipation = VkImgui_CvarFloat( "r_fogFluidDissipation" );
		float vorticity = VkImgui_CvarFloat( "r_fogFluidVorticity" );
		float buoyancy = VkImgui_CvarFloat( "r_fogFluidBuoyancy" );
		int fluidOn = ri.Cvar_VariableIntegerValue( "r_fogFluid" );
		bool fluidEnabled = ( fluidOn != 0 );
		if ( ImGui::Checkbox( "Enable Fluid Advection", &fluidEnabled ) ) {
			ri.Cvar_Set( "r_fogFluid", fluidEnabled ? "1" : "0" );
		}
		VkImgui_VolumetricsSlider( "Viscosity", "r_fogFluidViscosity", viscosity, 0.0f, 10.0f, "%.4f" );
		VkImgui_VolumetricsSlider( "Dissipation", "r_fogFluidDissipation", dissipation, 0.0f, 1.0f, "%.4f" );
		VkImgui_VolumetricsSlider( "Vorticity", "r_fogFluidVorticity", vorticity, 0.0f, 2.0f, "%.2f" );
		VkImgui_VolumetricsSlider( "Buoyancy", "r_fogFluidBuoyancy", buoyancy, 0.0f, 4.0f, "%.2f" );
	}

	if (ImGui::CollapsingHeader("Noise")) {
		float noiseScale = VkImgui_CvarFloat( "r_volumetricFogNoiseScale" );
		float noiseStrength = VkImgui_CvarFloat( "r_volumetricFogNoiseStrength" );
		float noiseThreshold = VkImgui_CvarFloat( "r_volumetricFogNoiseThreshold" );
		VkImgui_VolumetricsSlider( "Noise Scale", "r_volumetricFogNoiseScale", noiseScale, 0.0f, 1.0f, "%.4f" );
		VkImgui_VolumetricsSlider( "Noise Strength", "r_volumetricFogNoiseStrength", noiseStrength, 0.0f, 1.0f );
		VkImgui_VolumetricsSlider( "Noise Threshold", "r_volumetricFogNoiseThreshold", noiseThreshold, 0.0f, 1.0f );
	}

	if (ImGui::CollapsingHeader("Wind")) {
		float windSpeed = VkImgui_CvarFloat( "r_volumetricFogWindSpeed" );
		VkImgui_VolumetricsSlider( "Wind Speed", "r_volumetricFogWindSpeed", windSpeed, 0.0f, 64.0f );
		ImGui::TextDisabled( "Wind direction: r_volumetricFogWindDirection (x y z)" );
		ImGui::TextDisabled( "Noise scroll: r_volumetricFogNoiseScroll (x y z)" );
	}

	if (ImGui::CollapsingHeader("Color & Lighting")) {
		int colorMode = ri.Cvar_VariableIntegerValue( "r_volumetricFogColorMode" );
		float sunInt = VkImgui_CvarFloat( "r_volumetricFogSunIntensity" );
		float ambInt = VkImgui_CvarFloat( "r_volumetricFogAmbientIntensity" );
		const char *modes[] = { "Map/IBL", "r_volumetricFogTint", "IBL SH" };
		if ( ImGui::Combo( "Color Mode", &colorMode, modes, 3 ) ) {
			ri.Cvar_Set( "r_volumetricFogColorMode", va( "%d", colorMode ) );
		}
		VkImgui_VolumetricsSlider( "Sun Intensity", "r_volumetricFogSunIntensity", sunInt, 0.0f, 64.0f );
		VkImgui_VolumetricsSlider( "Ambient Intensity", "r_volumetricFogAmbientIntensity", ambInt, 0.0f, 64.0f );
		ImGui::TextDisabled( "Tint: r_volumetricFogTint \"r g b\"" );
	}

	if (ImGui::CollapsingHeader("Denoising")) {
		bool denoiseOn = ( ri.Cvar_Get( "r_volumetricFogDenoise", "0", 0 )->integer != 0 );
		if ( ImGui::Checkbox( "Gaussian Spatial Denoise", &denoiseOn ) ) {
			ri.Cvar_SetValue( "r_volumetricFogDenoise", denoiseOn ? 1.0f : 0.0f );
		}
		if ( denoiseOn ) {
			float sigma = VkImgui_CvarFloat( "r_volumetricFogDenoiseSigma" );
			VkImgui_VolumetricsSlider( "Sigma", "r_volumetricFogDenoiseSigma", sigma, 0.1f, 3.0f, "%.2f" );
		}
	}

	if (ImGui::CollapsingHeader("Temporal")) {
		float temporalWeight = VkImgui_CvarFloat( "r_volumetricFogTemporalWeight" );
		float temporalStability = VkImgui_CvarFloat( "r_volumetricFogTemporalStability" );
		float reprojThresh = VkImgui_CvarFloat( "r_volumetricFogReprojectionThreshold" );
		VkImgui_VolumetricsSlider( "Temporal Weight", "r_volumetricFogTemporalWeight", temporalWeight, 0.0f, 1.0f );
		VkImgui_VolumetricsSlider( "Temporal Stability", "r_volumetricFogTemporalStability", temporalStability, 0.0f, 1.0f );
		VkImgui_VolumetricsSlider( "Reprojection Threshold", "r_volumetricFogReprojectionThreshold", reprojThresh, 0.0f, 2.0f, "%.3f" );
	}

	if (ImGui::CollapsingHeader("Shadows")) {
		int cgShadows = ri.Cvar_VariableIntegerValue( "cg_shadows" );
		const char *cgShadowModes[] = { "Off", "Blob", "Stencil", "Planar" };
		int cgShadowIdx = ( cgShadows < 0 || cgShadows > 3 ) ? 0 : cgShadows;
		if ( ImGui::Combo( "Entity Shadows (cg_shadows)", &cgShadowIdx, cgShadowModes, 4 ) ) {
			ri.Cvar_Set( "cg_shadows", va( "%d", cgShadowIdx ) );
		}
		if ( cgShadowIdx == 2 ) {
			ImGui::SameLine();
			ImGui::TextDisabled( "(?)" );
			if ( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "Stencil shadows require r_stencilbits 8 and vid_restart. Polygon offset reduces thin black lines and single-pixel artifacts." );
			float svFactor = VkImgui_CvarFloat( "r_shadowVolumeOffsetFactor" );
			float svUnits = VkImgui_CvarFloat( "r_shadowVolumeOffsetUnits" );
			VkImgui_VolumetricsSlider( "Shadow volume offset factor", "r_shadowVolumeOffsetFactor", svFactor, 0.0f, 8.0f );
			VkImgui_VolumetricsSlider( "Shadow volume offset units", "r_shadowVolumeOffsetUnits", svUnits, 0.0f, 8.0f );
		}
		int shadowsOn = ri.Cvar_VariableIntegerValue( "r_fog_shadows" );
		bool shadowsEnabled = ( shadowsOn != 0 );
		if ( ImGui::Checkbox( "Sun Shadows in Fog", &shadowsEnabled ) ) {
			ri.Cvar_Set( "r_fog_shadows", shadowsEnabled ? "1" : "0" );
		}
		float shadowContrast = VkImgui_CvarFloat( "r_volumetricFogShadowContrast" );
		float shadowBias = VkImgui_CvarFloat( "r_fogShadowBias" );
		float shadowPcf = VkImgui_CvarFloat( "r_fogShadowPcfRadius" );
		VkImgui_VolumetricsSlider( "Shadow Contrast", "r_volumetricFogShadowContrast", shadowContrast, 0.5f, 4.0f );
		VkImgui_VolumetricsSlider( "Shadow Bias", "r_fogShadowBias", shadowBias, 0.0f, 0.05f, "%.4f" );
		VkImgui_VolumetricsSlider( "PCF Radius", "r_fogShadowPcfRadius", shadowPcf, 0.0f, 4.0f );
	}

	ImGui::EndChild();
	ImGui::End();
}

#endif /* USE_IMGUI */
