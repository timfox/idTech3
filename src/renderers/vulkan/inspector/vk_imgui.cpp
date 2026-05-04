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

#define IMGUI_DEFINE_MATH_OPERATORS

#include <float.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../renderers/common/tr_types.h"
#include "../../renderers/common/tr_public.h"

extern glconfig_t glConfig;
extern cvar_t *r_imgui;
}

#if defined( USE_SDL ) && !defined( ANDROID )
#	if defined( USE_LOCAL_HEADERS )
#		include "SDL.h"
#	else
#		include <SDL2/SDL.h>
#	endif
extern "C" struct SDL_Window *SDL_window;
#endif

#ifdef USE_VULKAN
#define USE_VK_PBR
#include "../vk.h"
#endif

#include "vk_imgui.h"

extern "C" bool VkImgui_InitVulkanBackend( ImGui_ImplVulkan_InitInfo *outInfo, char *errBuf, size_t errBufSize );
extern "C" void VkImgui_ShutdownVulkanBackend( void );
extern "C" void VkImgui_NewFrameVulkan( void );
extern "C" void VkImgui_RenderDrawDataVulkan( ImDrawData *drawData, VkCommandBuffer cmd );
extern "C" void VkImgui_UpdateMouseFromSDL( ImGuiIO *io, qboolean inspectorWantsInput );
extern "C" void VkImgui_NotifySwapchainRestart( void );
extern "C" void VkImgui_SetVulkanBackendReady( qboolean ready );

static qboolean vkImgBackendReady = qfalse;

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
static int imguiLastFrameTimeMs = 0;

static void VkImgui_SetCurrentContext( void )
{
	if ( imguiContext ) {
		ImGui::SetCurrentContext( imguiContext );
	}
}

static void VkImgui_PrepareIO( void )
{
	ImGuiIO &io = ImGui::GetIO();
	const int windowWidth = ( glConfig.vidWidth > 0 ) ? glConfig.vidWidth : 0;
	const int windowHeight = ( glConfig.vidHeight > 0 ) ? glConfig.vidHeight : 0;
	const int nowMs = ri.Milliseconds();
	float deltaSeconds = 1.0f / 60.0f;

	if ( imguiLastFrameTimeMs > 0 && nowMs > imguiLastFrameTimeMs ) {
		deltaSeconds = (float)( nowMs - imguiLastFrameTimeMs ) * 0.001f;
	}
	if ( deltaSeconds <= 0.0f ) {
		deltaSeconds = 1.0f / 60.0f;
	}

	io.DisplaySize = ImVec2( (float)( windowWidth >= 0 ? windowWidth : 0 ),
		(float)( windowHeight >= 0 ? windowHeight : 0 ) );
	{
		float sx = 1.0f;
		float sy = 1.0f;
#if defined( USE_SDL ) && !defined( ANDROID )
		if ( SDL_window != nullptr ) {
			int winW = 0;
			int winH = 0;
			SDL_GetWindowSize( SDL_window, &winW, &winH );
			if ( winW > 0 && winH > 0 && glConfig.vidWidth > 0 && glConfig.vidHeight > 0 ) {
				sx = (float)glConfig.vidWidth / (float)winW;
				sy = (float)glConfig.vidHeight / (float)winH;
			}
		}
#endif
		io.DisplayFramebufferScale = ImVec2( sx, sy );
	}
	io.DeltaTime = deltaSeconds;

	{
		const qboolean wantInput = ( r_imgui && r_imgui->integer ) ? qtrue : qfalse;
		vkImguiState.inputState = wantInput;
		if ( wantInput ) {
			VkImgui_UpdateMouseFromSDL( &io, wantInput );
		} else {
			io.MousePos = ImVec2( -FLT_MAX, -FLT_MAX );
		}
	}

	imguiLastFrameTimeMs = nowMs;
}

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
	0.6f,    /* r_volumetricFogDensity */
	0.015f,  /* r_volumetricFogHeightFalloff */
	1.5f,    /* r_volumetricFogIntensity */
	0.95f,   /* r_volumetricFogAlbedo */
	1.0f,    /* r_volumetricFogExtinctionScale */
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
	ri.Cvar_SetValue( "r_volumetricFogAlbedo", vkVolumetricsCvarDefaults[3] );
	ri.Cvar_SetValue( "r_volumetricFogExtinctionScale", vkVolumetricsCvarDefaults[4] );
	ri.Cvar_SetValue( "r_fogFluidViscosity", vkVolumetricsCvarDefaults[5] );
	ri.Cvar_SetValue( "r_fogFluidDissipation", vkVolumetricsCvarDefaults[6] );
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
	VkImgui_SetCurrentContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	VkImgui_DarkTheme();

	memset(&vkInspector, 0, sizeof(vkInspector));
	memset(&vkWindows, 0, sizeof(vkWindows));
	vkWindows.viewport.open = qtrue;
	vkWindows.postfx.open = qtrue;
	vkWindows.volumetrics.open = qtrue;

	vkImguiState.active = qtrue;
	vkImguiState.inputState = qfalse;
	imguiLastFrameTimeMs = 0;

#ifdef USE_VULKAN
	{
		char errBuf[256];

		vkImgBackendReady = qfalse;
		VkImgui_SetVulkanBackendReady( qfalse );
		if ( vk.device != VK_NULL_HANDLE && vk.render_pass.overlay_compose != VK_NULL_HANDLE ) {
			if ( VkImgui_InitVulkanBackend( nullptr, errBuf, sizeof( errBuf ) ) ) {
				vkImgBackendReady = qtrue;
				VkImgui_SetVulkanBackendReady( qtrue );
				ri.Printf( PRINT_ALL, "ImGui: Vulkan renderer backend initialized (overlay pass)\n" );
			} else {
				ri.Printf( PRINT_WARNING, "ImGui: Vulkan renderer backend failed (%s)\n", errBuf );
			}
		}
	}
#endif
}

extern "C" void VkImgui_Shutdown(void) {
	if (!vkImguiState.active) return;

	if (imguiContext) {
		VkImgui_SetCurrentContext();
#ifdef USE_VULKAN
		if ( vkImgBackendReady ) {
			VkImgui_ShutdownVulkanBackend();
			vkImgBackendReady = qfalse;
			VkImgui_SetVulkanBackendReady( qfalse );
		}
#endif
		ImGui::DestroyContext(imguiContext);
		imguiContext = nullptr;
	}

	memset(&vkImguiState, 0, sizeof(vkImguiState));
	imguiLastFrameTimeMs = 0;
}

extern "C" void VkImgui_BeginFrame(void) {
	if (!vkImguiState.active) return;
	VkImgui_SetCurrentContext();
	VkImgui_PrepareIO();
#ifdef USE_VULKAN
	if ( vkImgBackendReady ) {
		VkImgui_NewFrameVulkan();
	}
#endif
	ImGui::NewFrame();
}

static void VkImgui_DrawAboutInspectorPopup( void )
{
	if ( ImGui::BeginPopupModal( "AboutInspector", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::TextUnformatted( Q3_VERSION );
		ImGui::Separator();
		ImGui::Text( "ImGui %s", IMGUI_VERSION );
#ifdef USE_VULKAN
		ImGui::Text( "Renderer API: Vulkan" );
#else
		ImGui::Text( "Renderer API: OpenGL" );
#endif
		ImGui::Spacing();
		ImGui::TextWrapped( "Vendor: %s", glConfig.vendor_string );
		ImGui::TextWrapped( "Device: %s", glConfig.renderer_string );
		ImGui::TextWrapped( "Version: %s", glConfig.version_string );
		ImGui::Spacing();
		ImGui::TextWrapped(
			"Toggle overlay input with F11 or \\toggle_imgui; set \\r_imgui 0 to hide CPU/UI work. "
			"PostFX and related panels drive renderer cvars." );
		ImGui::Spacing();
		if ( ImGui::Button( "OK", ImVec2( 120.0f, 0.0f ) ) ) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static void VkImgui_DrawShortcutsPopup( void )
{
	if ( ImGui::BeginPopupModal( "InspectorShortcuts", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::TextWrapped(
			"F11 or \\toggle_imgui toggles the inspector when the client is built with ImGui. "
			"\\r_imgui 0 skips inspector CPU work; use Developer menu for a quick toggle." );
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::BulletText( "File: JPEG screenshot (silent), console, quit" );
		ImGui::BulletText( "Window: show/hide docked panels" );
		ImGui::BulletText( "Render Mode: \\r_pbr_debug modes (0-8 active)" );
		ImGui::BulletText( "Developer: \\r_speeds, \\r_showtris, \\r_imgui" );
		ImGui::Spacing();
		if ( ImGui::Button( "Close", ImVec2( 120.0f, 0.0f ) ) ) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static void VkImgui_DrawMenuBar(void) {
	ImGuiIO &io = ImGui::GetIO();
	const float fps = io.Framerate > 0.0f ? io.Framerate : 0.0f;
	const float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if ( ImGui::MenuItem( "Screenshot (JPEG)" ) ) {
				ri.Cmd_ExecuteText( EXEC_APPEND, "screenshotJPEG silent\n" );
			}
			if ( ImGui::MenuItem( "Toggle console" ) ) {
				ri.Cmd_ExecuteText( EXEC_APPEND, "toggleconsole\n" );
			}
			ImGui::Separator();
			if ( ImGui::MenuItem( "Quit" ) ) {
				ri.Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
			}
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu("Help")) {
			if ( ImGui::MenuItem( "Inspector shortcuts" ) ) {
				ImGui::OpenPopup( "InspectorShortcuts" );
			}
			if ( ImGui::MenuItem( "About inspector" ) ) {
				ImGui::OpenPopup( "AboutInspector" );
			}
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Developer" ) ) {
			if ( r_imgui ) {
				bool riOn = r_imgui->integer != 0;
				if ( ImGui::Checkbox( "Inspector overlay (r_imgui)", &riOn ) ) {
					ri.Cvar_SetValue( "r_imgui", riOn ? 1.0f : 0.0f );
				}
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip( "When off, skips ImGui BeginFrame/Draw CPU work. F11 still toggles from the client." );
				}
			}
			{
				int sp = ri.Cvar_VariableIntegerValue( "r_speeds" );
				if ( sp < 0 ) {
					sp = 0;
				}
				if ( sp > 6 ) {
					sp = 6;
				}
				const int spPrev = sp;
				ImGui::SliderInt( "r_speeds (debug HUD)", &sp, 0, 6 );
				if ( sp != spPrev ) {
					ri.Cvar_Set( "r_speeds", va( "%d", sp ) );
				}
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip( "Console stats overlay (cheat cvar). 0=off, 1=BSP, 2=patch cull, 3=cluster, 4=dlights, 5=zFar, 6=flares." );
				}
			}
			{
				int st = ri.Cvar_VariableIntegerValue( "r_showtris" );
				bool showTris = ( st != 0 );
				if ( ImGui::Checkbox( "Wireframe world (r_showtris)", &showTris ) ) {
					ri.Cvar_Set( "r_showtris", showTris ? "1" : "0" );
				}
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip( "World triangle overlay (cheat). May reduce performance." );
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window")) {
			ImGui::MenuItem("Viewport", nullptr, (bool *)&vkWindows.viewport.open);
			ImGui::MenuItem("Shader Editor", nullptr, (bool *)&vkWindows.shader.open);
			ImGui::MenuItem("Profiler", nullptr, (bool *)&vkWindows.profiler.open);
			ImGui::MenuItem("PostFX", nullptr, (bool *)&vkWindows.postfx.open);
			ImGui::MenuItem("Physics", nullptr, (bool *)&vkWindows.physics.open);
			ImGui::MenuItem("Volumetrics", nullptr, (bool *)&vkWindows.volumetrics.open);
			ImGui::MenuItem("Objects", nullptr, (bool *)&vkWindows.objects.open);
			ImGui::MenuItem("Inspector", nullptr, (bool *)&vkWindows.inspector.open);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render Mode")) {
			int pbrDbg = ri.Cvar_VariableIntegerValue( "r_pbr_debug" );
			for (unsigned int i = 0; i < NUM_RENDER_MODES; i++) {
				bool selected = ( pbrDbg == (int)i );
				if ( ImGui::MenuItem( vkRenderModes[i], nullptr, selected ) ) {
					/* r_pbr_debug supports 0-8; indices 9+ are placeholders */
					if ( (int)i <= 8 ) {
						ri.Cvar_Set( "r_pbr_debug", va( "%d", (int)i ) );
					}
				}
			}
			ImGui::EndMenu();
		}

		ImGui::SameLine(ImGui::GetWindowWidth() - 220);
		ImGui::Text("%.3f ms/frame (%.1f FPS)", ms, fps);

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
		const char *dbgModes[] = { "Final", "Pre-tonemap HDR", "Luminance heatmap",
			"Panini logical UV (97)", "Panini remapped UV (98)", "Panini OOB mask (99)" };
		int dbgCombo = ( dbg == 97 ) ? 3 : ( dbg == 98 ) ? 4 : ( dbg == 99 ) ? 5 : ( dbg >= 1 && dbg <= 2 ) ? dbg : 0;
		if ( ImGui::Combo( "Debug View", &dbgCombo, dbgModes, 6 ) ) {
			int v = ( dbgCombo == 3 ) ? 97 : ( dbgCombo == 4 ) ? 98 : ( dbgCombo == 5 ) ? 99 : dbgCombo;
			ri.Cvar_Set( "r_post_debug", va( "%d", v ) );
		}
	}

	if (ImGui::CollapsingHeader("Sky / Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
		int atmOn = ri.Cvar_VariableIntegerValue( "r_atmosphere" );
		bool atmEnabled = ( atmOn != 0 );
		if ( ImGui::Checkbox( "Procedural atmospheric sky", &atmEnabled ) ) {
			ri.Cvar_Set( "r_atmosphere", atmEnabled ? "1" : "0" );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "Rayleigh+Mie scattering sky. Replaces grey sky when no HDR skybox. Use r_skyboxHDR for custom EXR panorama." );
		}
		if ( atmEnabled ) {
			float sunX = VkImgui_CvarFloat( "r_atmosphere_sunDirX" );
			float sunY = VkImgui_CvarFloat( "r_atmosphere_sunDirY" );
			float sunZ = VkImgui_CvarFloat( "r_atmosphere_sunDirZ" );
			float sunInt = VkImgui_CvarFloat( "r_atmosphere_sunIntensity" );
			float atmScale = VkImgui_CvarFloat( "r_atmosphere_scale" );
			VkImgui_CvarSlider( "Sun dir X", "r_atmosphere_sunDirX", sunX, -1.0f, 1.0f );
			VkImgui_CvarSlider( "Sun dir Y", "r_atmosphere_sunDirY", sunY, -1.0f, 1.0f );
			VkImgui_CvarSlider( "Sun dir Z", "r_atmosphere_sunDirZ", sunZ, -1.0f, 1.0f );
			VkImgui_CvarSlider( "Sun intensity", "r_atmosphere_sunIntensity", sunInt, 1.0f, 100.0f );
			VkImgui_CvarSlider( "Sky scale (HDR)", "r_atmosphere_scale", atmScale, 0.5f, 16.0f );
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

	if (ImGui::CollapsingHeader("SSAO / HBAO")) {
		int ssaoOn = ri.Cvar_VariableIntegerValue( "r_ssao" );
		bool ssaoEnabled = ( ssaoOn != 0 );
		if ( ImGui::Checkbox( "Enable SSAO/HBAO", &ssaoEnabled ) ) {
			ri.Cvar_Set( "r_ssao", ssaoEnabled ? "1" : "0" );
		}
		if ( ssaoEnabled ) {
			ImGui::SameLine();
			ImGui::TextDisabled( "(?)" );
			if ( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "Screen-space ambient occlusion. Requires r_fbo 1. vid_restart after toggle." );
			int method = ri.Cvar_VariableIntegerValue( "r_ssaoMethod" );
			const char *methods[] = { "SSAO (sphere)", "HBAO (horizon)" };
			if ( ImGui::Combo( "Method", &method, methods, 2 ) )
				ri.Cvar_Set( "r_ssaoMethod", va( "%d", method ) );
			if ( method == 1 ) {
				int dirs = ri.Cvar_VariableIntegerValue( "r_hbaoDirections" );
				int steps = ri.Cvar_VariableIntegerValue( "r_hbaoSteps" );
				if ( ImGui::SliderInt( "HBAO Directions", &dirs, 4, 16 ) )
					ri.Cvar_Set( "r_hbaoDirections", va( "%d", dirs ) );
				if ( ImGui::SliderInt( "HBAO Steps", &steps, 2, 8 ) )
					ri.Cvar_Set( "r_hbaoSteps", va( "%d", steps ) );
			} else {
				int samples = ri.Cvar_VariableIntegerValue( "r_ssaoSamples" );
				if ( ImGui::SliderInt( "SSAO Samples", &samples, 4, 32 ) )
					ri.Cvar_Set( "r_ssaoSamples", va( "%d", samples ) );
			}
			float radius = VkImgui_CvarFloat( "r_ssaoRadius" );
			float bias = VkImgui_CvarFloat( "r_ssaoBias" );
			float intensity = VkImgui_CvarFloat( "r_ssaoIntensity" );
			float power = VkImgui_CvarFloat( "r_ssaoPower" );
			VkImgui_CvarSlider( "Radius", "r_ssaoRadius", radius, 0.1f, 4.0f );
			VkImgui_CvarSlider( "Bias", "r_ssaoBias", bias, 0.0f, 0.1f, "%.4f" );
			VkImgui_CvarSlider( "Intensity", "r_ssaoIntensity", intensity, 0.0f, 4.0f );
			VkImgui_CvarSlider( "Power", "r_ssaoPower", power, 0.5f, 4.0f );
			int blurRadius = ri.Cvar_VariableIntegerValue( "r_ssaoBlurRadius" );
			if ( ImGui::SliderInt( "Blur Radius", &blurRadius, 0, 8 ) )
				ri.Cvar_Set( "r_ssaoBlurRadius", va( "%d", blurRadius ) );
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
		int expAuto = ri.Cvar_Get( "r_exposure_auto", "0", 0 )->integer;
		bool expAutoOn = ( expAuto != 0 );
		if ( ImGui::Checkbox( "Eye adaptation (auto exposure)", &expAutoOn ) ) {
			ri.Cvar_SetValue( "r_exposure_auto", expAutoOn ? 1.0f : 0.0f );
		}
		if ( expAutoOn ) {
			ImGui::SameLine();
			ImGui::TextDisabled( "(?)" );
			if ( ImGui::IsItemHovered() ) {
				ImGui::SetTooltip( "Temporal adaptation toward target. Luminance pass planned for full Source Lost Coast style." );
			}
			float expTarget = VkImgui_CvarFloat( "r_exposure_auto_target" );
			float expSpeed = VkImgui_CvarFloat( "r_exposure_auto_speed" );
			VkImgui_CvarSlider( "Adapt target", "r_exposure_auto_target", expTarget, 0.1f, 2.0f );
			VkImgui_CvarSlider( "Adapt speed", "r_exposure_auto_speed", expSpeed, 0.1f, 10.0f );
		}
		float lmScale = VkImgui_CvarFloat( "r_hdr_lightmap_scale" );
		VkImgui_CvarSlider( "HDR lightmap scale", "r_hdr_lightmap_scale", lmScale, 0.5f, 8.0f );
		if ( VkImgui_CvarFloat( "r_hdr" ) > 0.0f ) {
			bool lmSrgb = VkImgui_CvarFloat( "r_lightmap_srgb_decode" ) > 0.5f;
			if ( ImGui::Checkbox( "Lightmap sRGB decode", &lmSrgb ) )
				ri.Cvar_Set( "r_lightmap_srgb_decode", lmSrgb ? "1" : "0" );
		}
		float preExp = VkImgui_CvarFloat( "r_pre_exposure_scale" );
		VkImgui_CvarSlider( "Pre-exposure scale", "r_pre_exposure_scale", preExp, 0.1f, 4.0f );
		float postContrast = VkImgui_CvarFloat( "r_post_contrast" );
		float postSaturation = VkImgui_CvarFloat( "r_post_saturation" );
		VkImgui_CvarSlider( "Contrast", "r_post_contrast", postContrast, 0.25f, 4.0f );
		VkImgui_CvarSlider( "Saturation", "r_post_saturation", postSaturation, 0.0f, 3.0f );
	}

	if (ImGui::CollapsingHeader("PBR")) {
		bool pbrOn = ri.Cvar_VariableIntegerValue( "r_pbr" ) != 0;
		if ( ImGui::Checkbox( "Enable PBR", &pbrOn ) ) {
			ri.Cvar_Set( "r_pbr", pbrOn ? "1" : "0" );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Physically Based Rendering. Requires r_fbo 1. Changes need vid_restart." );
		bool fboOn = ri.Cvar_VariableIntegerValue( "r_fbo" ) != 0;
		if ( !fboOn && pbrOn ) {
			ImGui::SameLine();
			ImGui::TextColored( ImVec4( 1.0f, 0.5f, 0.0f, 1.0f ), "r_fbo 1 required" );
		}
	}

	if (ImGui::CollapsingHeader("Anti-Aliasing")) {
		int msaa = ri.Cvar_VariableIntegerValue( "r_ext_multisample" );
		const char *msaaModes[] = { "Off", "2x", "4x", "8x", "16x" };
		int msaaIdx = ( msaa <= 0 ) ? 0 : ( msaa <= 2 ) ? 1 : ( msaa <= 4 ) ? 2 : ( msaa <= 8 ) ? 3 : 4;
		if ( ImGui::Combo( "MSAA", &msaaIdx, msaaModes, 5 ) ) {
			int v = ( msaaIdx == 0 ) ? 0 : ( msaaIdx == 1 ) ? 2 : ( msaaIdx == 2 ) ? 4 : ( msaaIdx == 3 ) ? 8 : 16;
			ri.Cvar_Set( "r_ext_multisample", va( "%d", v ) );
		}
		if ( msaa > 0 ) {
			bool sampleShading = ri.Cvar_VariableIntegerValue( "r_msaa_sample_shading" ) != 0;
			if ( ImGui::Checkbox( "Sample shading (better alpha, ~2x cost)", &sampleShading ) )
				ri.Cvar_Set( "r_msaa_sample_shading", sampleShading ? "1" : "0" );
			bool alphaToCov = ri.Cvar_VariableIntegerValue( "r_ext_alpha_to_coverage" ) != 0;
			if ( ImGui::Checkbox( "Alpha-to-coverage (foliage, grates)", &alphaToCov ) )
				ri.Cvar_Set( "r_ext_alpha_to_coverage", alphaToCov ? "1" : "0" );
		}
		bool smaaOn = ri.Cvar_VariableIntegerValue( "r_ext_smaa" ) != 0;
		if ( ImGui::Checkbox( "SMAA (post-process)", &smaaOn ) )
			ri.Cvar_Set( "r_ext_smaa", smaaOn ? "1" : "0" );
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "MSAA + SMAA: geometry + alpha edges. MSAA/SMAA changes need vid_restart." );
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

	if (ImGui::CollapsingHeader("Detail Textures")) {
		int dtOn = ri.Cvar_VariableIntegerValue( "r_detailtextures" );
		bool dtEnabled = ( dtOn != 0 );
		if ( ImGui::Checkbox( "Enable detail maps", &dtEnabled ) ) {
			ri.Cvar_Set( "r_detailtextures", dtEnabled ? "1" : "0" );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "High-frequency tiling overlay for PBR materials. Use detailMap in shaders or _detail suffix." );
		}
		float dtScale = VkImgui_CvarFloat( "r_detail_scale" );
		VkImgui_CvarSlider( "Detail scale (tiling)", "r_detail_scale", dtScale, 0.5f, 32.0f );
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
		float maxGrad = VkImgui_CvarFloat( "r_ssr_maxDepthGradient" );
		VkImgui_CvarSlider( "Max Distance", "r_ssr_maxDistance", maxDist, 10.0f, 500.0f );
		VkImgui_CvarSlider( "Step Size", "r_ssr_stepSize", stepSz, 0.1f, 5.0f );
		VkImgui_CvarSlider( "Thickness", "r_ssr_thickness", thick, 0.1f, 5.0f );
		VkImgui_CvarSlider( "Intensity", "r_ssr_intensity", ssrInt, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Max depth gradient", "r_ssr_maxDepthGradient", maxGrad, 0.01f, 0.5f, "%.3f" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Skip SSR at depth edges (object silhouettes, horizon) to reduce thin line artifacts. Lower = stricter." );
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

extern "C" void VkImgui_DrawProfiler(void) {
	if (!vkWindows.profiler.open) return;
	ImGuiIO &io = ImGui::GetIO();
	const float fps = io.Framerate > 0.0f ? io.Framerate : 0.0f;
	const float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
	ImGui::Begin("GPU Profiler", (bool *)&vkWindows.profiler.open);

	ImGui::Text("Frame Time: %.3f ms", ms);
	ImGui::Text("FPS: %.1f", fps);

	static float frameTimes[120] = {};
	static int frameIdx = 0;
	frameTimes[frameIdx] = ms;
	frameIdx = (frameIdx + 1) % 120;
	ImGui::PlotLines("Frame Time (ms)", frameTimes, 120, frameIdx, nullptr, 0.0f, 33.0f, ImVec2(0, 80));

	ImGui::End();
}

extern "C" void VkImgui_DrawViewport(void) {
	if ( !vkWindows.viewport.open ) return;
	ImGui::Begin( "Viewport", (bool *)&vkWindows.viewport.open );
	ImGui::Text( "Viewport preview is host-rendered." );
	ImGui::TextDisabled( "Game scene remains visible behind the dockspace." );
	ImGui::TextDisabled( "Live texture binding hook: pending backend integration." );
	ImGui::End();
}

extern "C" void VkImgui_DrawShaderEditor(void) {
	if ( !vkWindows.shader.open ) return;
	ImGui::Begin( "Shader Editor", (bool *)&vkWindows.shader.open );
	ImGui::Text( "Shader live-edit pipeline is not wired yet." );
	ImGui::TextDisabled( "Use r_reloadShaders / vid_restart for now." );
	ImGui::End();
}

extern "C" void VkImgui_DrawObjects(void) {
	if ( !vkWindows.objects.open ) return;
	ImGui::Begin( "Objects", (bool *)&vkWindows.objects.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() )
		ImGui::SetTooltip( "Scene hierarchy (World, Entities, Models). Full implementation pending." );
	ImGui::Separator();
	if ( ImGui::TreeNodeEx( "World", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		ImGui::TextDisabled( "BSP surfaces" );
		ImGui::TreePop();
	}
	if ( ImGui::TreeNodeEx( "Entities" ) ) {
		ImGui::TextDisabled( "RefEntities" );
		ImGui::TreePop();
	}
	if ( ImGui::TreeNodeEx( "Models" ) ) {
		ImGui::TextDisabled( "Cached models" );
		ImGui::TreePop();
	}
	ImGui::End();
}

extern "C" void VkImgui_DrawInspector(void) {
	if ( !vkWindows.inspector.open ) return;
	ImGui::Begin( "Inspector", (bool *)&vkWindows.inspector.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() )
		ImGui::SetTooltip( "Properties of selected object. Select in Objects panel. Full implementation pending." );
	ImGui::Separator();
	ImGui::Text( "No selection" );
	ImGui::TextDisabled( "Select an object in the Objects panel." );
	ImGui::End();
}

extern "C" void VkImgui_Draw(void) {
	if (!vkImguiState.active) return;
	VkImgui_SetCurrentContext();

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
	VkImgui_DrawAboutInspectorPopup();
	VkImgui_DrawShortcutsPopup();
	VkImgui_DrawViewport();
	VkImgui_DrawShaderEditor();
	VkImgui_DrawObjects();
	VkImgui_DrawInspector();
	VkImgui_DrawPostFXPanel();
	VkImgui_DrawPhysicsPanel();
	VkImgui_DrawVolumetricsPanel();
	VkImgui_DrawProfiler();

	ImGui::End();
	ImGui::Render();
}

extern "C" void VkImgui_SwapchainRestarted(void) {
#ifdef USE_VULKAN
	VkImgui_NotifySwapchainRestart();
#endif
}

extern "C" void VkImgui_BindGameColorImage(void) { }

#endif /* USE_IMGUI */
