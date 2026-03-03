/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

ImGui Inspector main module.
Architecture inspired by EternalJK's pbr-rtx-inspector (Sunny JK).
===========================================================================
*/

#ifdef USE_IMGUI

#define VK_NO_PROTOTYPES
#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui_internal.h>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../renderers/common/tr_types.h"
#include "../../renderers/common/tr_public.h"
}

#include "vk_imgui.h"

/* Helper to read float cvar (refimport has no Cvar_VariableValue) */
static float VkImgui_CvarFloat( const char *name )
{
	const char *s = ri.Cvar_VariableString( name );
	return s && s[0] ? (float)Q_atof( s ) : 0.0f;
}

/* Slider that writes to cvar on change (shared by PostFX, Volumetrics, Physics) */
static void VkImgui_CvarSlider( const char *label, const char *cvar, float v, float vMin, float vMax,
	const char *fmt = "%.3f" )
{
	if ( ImGui::SliderFloat( label, &v, vMin, vMax, fmt ) ) {
		ri.Cvar_SetValue( cvar, v );
	}
}

vkImguiInspector_t   vkInspector;
vkImguiWindows_t     vkWindows;
vkImguiGlobal_t      vkImguiState;

static ImGuiContext *imguiContext = nullptr;

typedef struct {
	float threshold;
	float intensity;
	int tonemapMode;
	float exposure;
	float vignette;
	float vigRadius;
	float chromAb;
	float grain;
	float maxDist;
	float stepSz;
	float thick;
	float ssrIntensity;
} vkPostFxPanelState_t;

typedef struct {
	float gravity;
	float stiffness;
	float damping;
} vkPhysicsPanelState_t;

typedef struct {
	float density;
	float heightFalloff;
	float scatter;
	float viscosity;
	float dissipation;
} vkVolumetricsPanelState_t;

/* Engine cvar defaults for Volumetrics reset (must match tr_init.c) */
static const float vkVolumetricsCvarDefaults[] = {
	0.35f,   /* r_volumetricFogDensity */
	0.4f,    /* r_volumetricFogHeightFalloff */
	1.0f,    /* r_volumetricFogIntensity */
	0.05f,   /* r_fogFluidViscosity */
	0.985f,  /* r_fogFluidDissipation */
};

static const vkPostFxPanelState_t vkPostFxDefaults = {
	0.6f, 0.5f, 2, 1.0f,
	0.0f, 0.75f, 0.0f, 0.0f,
	100.0f, 1.0f, 0.5f, 0.8f
};

static const vkPhysicsPanelState_t vkPhysicsDefaults = {
	-800.0f, 0.8f, 0.4f
};

static const vkVolumetricsPanelState_t vkVolumetricsDefaults = {
	0.02f, 0.04f, 1.0f, 0.0001f, 0.995f
};

static vkPostFxPanelState_t vkPostFxState = vkPostFxDefaults;
static vkPhysicsPanelState_t vkPhysicsState = vkPhysicsDefaults;
static vkVolumetricsPanelState_t vkVolumetricsState = vkVolumetricsDefaults;

static void VkImgui_ResetPostFxDefaults(void) {
	vkPostFxState = vkPostFxDefaults;
	ri.Cvar_SetValue( "r_bloom_threshold", 0.6f );
	ri.Cvar_SetValue( "r_bloom_intensity", 0.5f );
	ri.Cvar_SetValue( "r_bloomKnee", 0.5f );
	ri.Cvar_Set( "r_bloom_threshold_mode", "0" );
	ri.Cvar_Set( "r_bloom_modulate", "0" );
	ri.Cvar_Set( "r_tonemap", "3" );
	ri.Cvar_SetValue( "r_exposure", 0.82f );
	ri.Cvar_Set( "r_post", "1" );
	ri.Cvar_SetValue( "r_vignette", 0.55f );
	ri.Cvar_SetValue( "r_vignette_radius", 0.60f );
	ri.Cvar_SetValue( "r_chromaticAberration", 0.22f );
	ri.Cvar_SetValue( "r_filmGrain", 0.75f );
	ri.Cvar_SetValue( "r_ssr_maxDistance", 100.0f );
	ri.Cvar_SetValue( "r_ssr_stepSize", 1.0f );
	ri.Cvar_SetValue( "r_ssr_thickness", 0.5f );
	ri.Cvar_SetValue( "r_ssr_intensity", 0.8f );
}

static void VkImgui_ResetPhysicsDefaults(void) {
	vkPhysicsState = vkPhysicsDefaults;
	ri.Cvar_SetValue( "phys_gravity", -800.0f );
	ri.Cvar_SetValue( "phys_ragdoll_stiffness", 0.8f );
	ri.Cvar_SetValue( "phys_ragdoll_damping", 0.4f );
}

static void VkImgui_ResetVolumetricsDefaults(void) {
	vkVolumetricsState = vkVolumetricsDefaults;
	ri.Cvar_SetValue( "r_volumetricFogDensity", vkVolumetricsCvarDefaults[0] );
	ri.Cvar_SetValue( "r_volumetricFogHeightFalloff", vkVolumetricsCvarDefaults[1] );
	ri.Cvar_SetValue( "r_volumetricFogIntensity", vkVolumetricsCvarDefaults[2] );
	ri.Cvar_SetValue( "r_fogFluidViscosity", vkVolumetricsCvarDefaults[3] );
	ri.Cvar_SetValue( "r_fogFluidDissipation", vkVolumetricsCvarDefaults[4] );
}

static void VkImgui_DrawDefaultsConfirmation(
	const char *buttonLabel,
	const char *popupId,
	const char *panelName,
	void (*resetFn)(void)
) {
	if (ImGui::Button(buttonLabel)) {
		ImGui::OpenPopup(popupId);
	}

	if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Reset %s settings to defaults?", panelName);
		ImGui::Separator();

		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply Defaults", ImVec2(140.0f, 0.0f))) {
			resetFn();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

static void VkImgui_DarkTheme(void) {
	ImVec4 *colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_PopupBg]            = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
	colors[ImGuiCol_Border]             = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_FrameBg]            = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_MenuBarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBg]            = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgActive]      = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_CheckMark]          = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_SliderGrab]         = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_Button]             = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ButtonHovered]      = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_Header]             = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_Tab]                = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabHovered]         = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_DockingPreview]     = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);

	ImGuiStyle &style = ImGui::GetStyle();
	style.IndentSpacing     = 25;
	style.ScrollbarSize     = 15;
	style.GrabMinSize       = 10;
	style.WindowBorderSize  = 0;
	style.WindowRounding    = 7;
	style.ChildRounding     = 4;
	style.FrameRounding     = 3;
	style.PopupRounding     = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding      = 3;
	style.TabRounding       = 7;
}

extern "C" void VkImgui_Initialize(void) {
	if (vkImguiState.active) return;

	imguiContext = ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	VkImgui_DarkTheme();

	memset(&vkInspector, 0, sizeof(vkInspector));
	memset(&vkWindows, 0, sizeof(vkWindows));
	vkWindows.viewport.open = qtrue;
	vkWindows.postfx.open = qtrue;
	vkWindows.volumetrics.open = qtrue;

	vkImguiState.active = qtrue;
	vkImguiState.inputState = qfalse;
}

extern "C" void VkImgui_Shutdown(void) {
	if (!vkImguiState.active) return;

	if (imguiContext) {
		ImGui::DestroyContext(imguiContext);
		imguiContext = nullptr;
	}

	memset(&vkImguiState, 0, sizeof(vkImguiState));
}

extern "C" void VkImgui_BeginFrame(void) {
	if (!vkImguiState.active) return;
	ImGui::NewFrame();
}

static void VkImgui_DrawMenuBar(void) {
	ImGuiIO &io = ImGui::GetIO();

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			ImGui::MenuItem("Quit");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window")) {
			ImGui::MenuItem("Viewport", nullptr, (bool *)&vkWindows.viewport.open);
			ImGui::MenuItem("Shader Editor", nullptr, (bool *)&vkWindows.shader.open);
			ImGui::MenuItem("Profiler", nullptr, (bool *)&vkWindows.profiler.open);
			ImGui::MenuItem("PostFX", nullptr, (bool *)&vkWindows.postfx.open);
			ImGui::MenuItem("Physics", nullptr, (bool *)&vkWindows.physics.open);
			ImGui::MenuItem("Volumetrics", nullptr, (bool *)&vkWindows.volumetrics.open);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render Mode")) {
			for (unsigned int i = 0; i < NUM_RENDER_MODES; i++) {
				bool selected = (vkInspector.renderMode.index == (int)i);
				if (ImGui::MenuItem(vkRenderModes[i], nullptr, selected))
					vkInspector.renderMode.index = (int)i;
			}
			ImGui::EndMenu();
		}

		ImGui::SameLine(ImGui::GetWindowWidth() - 220);
		ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

		ImGui::EndMainMenuBar();
	}
}

extern "C" void VkImgui_DrawPostFXPanel(void) {
	if (!vkWindows.postfx.open) return;
	ImGui::Begin("PostFX", (bool *)&vkWindows.postfx.open);
	VkImgui_DrawDefaultsConfirmation(
		"Defaults##PostFX",
		"ConfirmDefaultsPostFX",
		"PostFX",
		VkImgui_ResetPostFxDefaults
	);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Apply baseline values. All controls drive engine cvars." );
	}
	ImGui::Separator();
	ImGui::BeginChild("PostFXScrollRegion", ImVec2(0.0f, 0.0f), qfalse, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	/* Pipeline toggle */
	{
		int on = ri.Cvar_VariableIntegerValue( "r_post" );
		bool enabled = ( on != 0 );
		if ( ImGui::Checkbox( "Enable Post-Processing", &enabled ) ) {
			ri.Cvar_Set( "r_post", enabled ? "1" : "0" );
		}
		int dbg = ri.Cvar_VariableIntegerValue( "r_post_debug" );
		const char *dbgModes[] = { "Final", "Pre-tonemap HDR", "Luminance heatmap" };
		if ( ImGui::Combo( "Debug View", &dbg, dbgModes, 3 ) ) {
			ri.Cvar_Set( "r_post_debug", va( "%d", dbg ) );
		}
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "0=final, 1=HDR, 2=luma. 97-99=Panini debug." );
		}
	}

	if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
		int bloomOn = ri.Cvar_VariableIntegerValue( "r_bloom" );
		bool bloomEnabled = ( bloomOn != 0 );
		if ( ImGui::Checkbox( "Enable Bloom", &bloomEnabled ) ) {
			ri.Cvar_Set( "r_bloom", bloomEnabled ? "1" : "0" );
		}
		float thresh = VkImgui_CvarFloat( "r_bloom_threshold" );
		float intensity = VkImgui_CvarFloat( "r_bloom_intensity" );
		float knee = VkImgui_CvarFloat( "r_bloomKnee" );
		int threshMode = ri.Cvar_VariableIntegerValue( "r_bloom_threshold_mode" );
		int modulate = ri.Cvar_VariableIntegerValue( "r_bloom_modulate" );
		VkImgui_CvarSlider( "Threshold", "r_bloom_threshold", thresh, 0.0f, 2.0f );
		VkImgui_CvarSlider( "Intensity", "r_bloom_intensity", intensity, 0.0f, 2.0f );
		VkImgui_CvarSlider( "Knee", "r_bloomKnee", knee, 0.0f, 4.0f );
		const char *threshModes[] = { "RGB >= thresh", "Avg >= thresh", "Luma >= thresh" };
		if ( ImGui::Combo( "Threshold Mode", &threshMode, threshModes, 3 ) ) {
			ri.Cvar_Set( "r_bloom_threshold_mode", va( "%d", threshMode ) );
		}
		const char *modModes[] = { "Off", "Color^2", "Color * Luma" };
		if ( ImGui::Combo( "Modulate", &modulate, modModes, 3 ) ) {
			ri.Cvar_Set( "r_bloom_modulate", va( "%d", modulate ) );
		}
	}

	if (ImGui::CollapsingHeader("Tonemapping", ImGuiTreeNodeFlags_DefaultOpen)) {
		int tonemap = ri.Cvar_VariableIntegerValue( "r_tonemap" );
		float exposure = VkImgui_CvarFloat( "r_exposure" );
		const char *modes[] = { "None", "Reinhard", "ACES", "Filmic", "AgX" };
		if ( ImGui::Combo( "Mode", &tonemap, modes, 5 ) ) {
			ri.Cvar_Set( "r_tonemap", va( "%d", tonemap ) );
		}
		VkImgui_CvarSlider( "Exposure", "r_exposure", exposure, 0.01f, 10.0f );
	}

	if (ImGui::CollapsingHeader("Lens Effects")) {
		float vignette = VkImgui_CvarFloat( "r_vignette" );
		float vigRadius = VkImgui_CvarFloat( "r_vignette_radius" );
		float chromAb = VkImgui_CvarFloat( "r_chromaticAberration" );
		float grain = VkImgui_CvarFloat( "r_filmGrain" );
		VkImgui_CvarSlider( "Vignette", "r_vignette", vignette, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Vignette Radius", "r_vignette_radius", vigRadius, 0.3f, 1.5f );
		VkImgui_CvarSlider( "Chromatic Aberration", "r_chromaticAberration", chromAb, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Film Grain", "r_filmGrain", grain, 0.0f, 1.0f );
	}

	if (ImGui::CollapsingHeader("SSR")) {
		int ssrOn = ri.Cvar_VariableIntegerValue( "r_ssr" );
		bool ssrEnabled = ( ssrOn != 0 );
		if ( ImGui::Checkbox( "Enable SSR", &ssrEnabled ) ) {
			ri.Cvar_Set( "r_ssr", ssrEnabled ? "1" : "0" );
		}
		float maxDist = VkImgui_CvarFloat( "r_ssr_maxDistance" );
		float stepSz = VkImgui_CvarFloat( "r_ssr_stepSize" );
		float thick = VkImgui_CvarFloat( "r_ssr_thickness" );
		float ssrInt = VkImgui_CvarFloat( "r_ssr_intensity" );
		VkImgui_CvarSlider( "Max Distance", "r_ssr_maxDistance", maxDist, 10.0f, 500.0f );
		VkImgui_CvarSlider( "Step Size", "r_ssr_stepSize", stepSz, 0.1f, 5.0f );
		VkImgui_CvarSlider( "Thickness", "r_ssr_thickness", thick, 0.1f, 5.0f );
		VkImgui_CvarSlider( "Intensity", "r_ssr_intensity", ssrInt, 0.0f, 1.0f );
	}

	ImGui::EndChild();
	ImGui::End();
}

extern "C" void VkImgui_DrawPhysicsPanel(void) {
	if (!vkWindows.physics.open) return;
	ImGui::Begin("Physics", (bool *)&vkWindows.physics.open);
	VkImgui_DrawDefaultsConfirmation(
		"Defaults##Physics",
		"ConfirmDefaultsPhysics",
		"Physics",
		VkImgui_ResetPhysicsDefaults
	);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Apply baseline values. Controls drive phys_* cvars." );
	}
	ImGui::Separator();
	ImGui::BeginChild("PhysicsScrollRegion", ImVec2(0.0f, 0.0f), qfalse, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	if (ImGui::CollapsingHeader("Bullet Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
		int physOn = ri.Cvar_VariableIntegerValue( "phys_enabled" );
		bool physEnabled = ( physOn != 0 );
		if ( ImGui::Checkbox( "Enable Physics", &physEnabled ) ) {
			ri.Cvar_Set( "phys_enabled", physEnabled ? "1" : "0" );
		}
		float gravity = VkImgui_CvarFloat( "phys_gravity" );
		float stiffness = VkImgui_CvarFloat( "phys_ragdoll_stiffness" );
		float damping = VkImgui_CvarFloat( "phys_ragdoll_damping" );
		VkImgui_CvarSlider( "Gravity", "phys_gravity", gravity, -2000.0f, 0.0f, "%.0f" );
		VkImgui_CvarSlider( "Ragdoll Stiffness", "phys_ragdoll_stiffness", stiffness, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Ragdoll Damping", "phys_ragdoll_damping", damping, 0.0f, 1.0f );
	}

	ImGui::EndChild();
	ImGui::End();
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
		VkImgui_VolumetricsSlider( "Density", "r_volumetricFogDensity", density, 0.0f, 5.0f );
		VkImgui_VolumetricsSlider( "Height Falloff", "r_volumetricFogHeightFalloff", heightFalloff, 0.0f, 5.0f );
		VkImgui_VolumetricsSlider( "Scatter Intensity", "r_volumetricFogIntensity", intensity, 0.0f, 50.0f );
	}

	if (ImGui::CollapsingHeader("Fluid Simulation")) {
		float viscosity = VkImgui_CvarFloat( "r_fogFluidViscosity" );
		float dissipation = VkImgui_CvarFloat( "r_fogFluidDissipation" );
		int fluidOn = ri.Cvar_VariableIntegerValue( "r_fogFluid" );
		bool fluidEnabled = ( fluidOn != 0 );
		if ( ImGui::Checkbox( "Enable Fluid Advection", &fluidEnabled ) ) {
			ri.Cvar_Set( "r_fogFluid", fluidEnabled ? "1" : "0" );
		}
		VkImgui_VolumetricsSlider( "Viscosity", "r_fogFluidViscosity", viscosity, 0.0f, 10.0f, "%.4f" );
		VkImgui_VolumetricsSlider( "Dissipation", "r_fogFluidDissipation", dissipation, 0.0f, 1.0f, "%.4f" );
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

	if (ImGui::CollapsingHeader("Temporal")) {
		float temporalWeight = VkImgui_CvarFloat( "r_volumetricFogTemporalWeight" );
		float temporalStability = VkImgui_CvarFloat( "r_volumetricFogTemporalStability" );
		float reprojThresh = VkImgui_CvarFloat( "r_volumetricFogReprojectionThreshold" );
		VkImgui_VolumetricsSlider( "Temporal Weight", "r_volumetricFogTemporalWeight", temporalWeight, 0.0f, 1.0f );
		VkImgui_VolumetricsSlider( "Temporal Stability", "r_volumetricFogTemporalStability", temporalStability, 0.0f, 1.0f );
		VkImgui_VolumetricsSlider( "Reprojection Threshold", "r_volumetricFogReprojectionThreshold", reprojThresh, 0.0f, 2.0f, "%.3f" );
	}

	if (ImGui::CollapsingHeader("Shadows")) {
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

extern "C" void VkImgui_DrawProfiler(void) {
	if (!vkWindows.profiler.open) return;
	ImGuiIO &io = ImGui::GetIO();
	ImGui::Begin("GPU Profiler", (bool *)&vkWindows.profiler.open);

	ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);
	ImGui::Text("FPS: %.1f", io.Framerate);

	static float frameTimes[120] = {};
	static int frameIdx = 0;
	frameTimes[frameIdx] = 1000.0f / io.Framerate;
	frameIdx = (frameIdx + 1) % 120;
	ImGui::PlotLines("Frame Time (ms)", frameTimes, 120, frameIdx, nullptr, 0.0f, 33.0f, ImVec2(0, 80));

	ImGui::End();
}

extern "C" void VkImgui_Draw(void) {
	if (!vkImguiState.active) return;

	ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpace", nullptr, dockFlags);
	ImGui::PopStyleVar(3);

	ImGuiID dockId = ImGui::GetID("InspectorDockSpace");
	ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	VkImgui_DrawMenuBar();
	VkImgui_DrawPostFXPanel();
	VkImgui_DrawPhysicsPanel();
	VkImgui_DrawVolumetricsPanel();
	VkImgui_DrawProfiler();

	ImGui::End();
	ImGui::Render();
}

extern "C" void VkImgui_SwapchainRestarted(void) { }
extern "C" void VkImgui_BindGameColorImage(void) { }
extern "C" void VkImgui_DrawObjects(void) { }
extern "C" void VkImgui_DrawInspector(void) { }
extern "C" void VkImgui_DrawViewport(void) { }
extern "C" void VkImgui_DrawShaderEditor(void) { }

#endif /* USE_IMGUI */
