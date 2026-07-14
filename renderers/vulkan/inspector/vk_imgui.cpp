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

#include "vk_imgui_common.hpp"

static qboolean vkImgBackendReady = qfalse;
static int vkImgLastTheme = -1;

extern cvar_t *r_imguiTheme;

extern "C" void VkImgui_LoadFonts( void );

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

extern "C" void VkImgui_Initialize(void) {
	if (vkImguiState.active) return;

	imguiContext = ImGui::CreateContext();
	VkImgui_SetCurrentContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	VkImgui_LoadFonts();
	VkImgui_ApplyInspectorStyle();
	vkImgLastTheme = ( r_imguiTheme && r_imguiTheme->integer == 1 ) ? 1 : 0;
	ri.Printf( PRINT_ALL,
		"[VK][imgui] editor UI: Pablo/VEditor-style dock, theme %s (r_imguiTheme %d)\n",
		vkImgLastTheme ? "Spectrum light" : "Pablo dark",
		vkImgLastTheme );

	memset(&vkInspector, 0, sizeof(vkInspector));
	memset(&vkWindows, 0, sizeof(vkWindows));
	vkWindows.viewport.open = qtrue;
	vkWindows.postfx.open = qtrue;
	vkWindows.volumetrics.open = qtrue;
	vkWindows.ocean.open = qtrue;
	if ( r_studio_tools && r_studio_tools->integer ) {
		vkWindows.studioMap.open = qtrue;
		vkWindows.studioConsole.open = qtrue;
		vkWindows.studioEntities.open = qtrue;
		vkWindows.studioPaint.open = qtrue;
		vkWindows.studioAnimation.open = qtrue;
	}
	if ( ri.Cvar_VariableIntegerValue( "cl_oscarUi" ) ) {
		vkWindows.oscar.open = qtrue;
	}

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
	if ( !vkImgBackendReady ) {
		if ( r_imgui && r_imgui->integer ) {
			ri.Printf( PRINT_WARNING, "ImGui: renderer backend unavailable, disabling r_imgui to avoid invalid font-atlas state\n" );
			ri.Cvar_Set( "r_imgui", "0" );
		}
		return;
	}
	VkImgui_SetCurrentContext();
	VkImgui_PrepareIO();
	if ( r_imguiTheme ) {
		const int theme = r_imguiTheme->integer == 1 ? 1 : 0;
		if ( theme != vkImgLastTheme ) {
			vkImgLastTheme = theme;
			VkImgui_ApplyInspectorStyle();
		}
	}
#ifdef USE_VULKAN
	VkImgui_NewFrameVulkan();
#endif
	ImGui::NewFrame();
}

extern "C" void VkImgui_Draw(void) {
	if (!vkImguiState.active) return;
	if ( !vkImgBackendReady ) return;
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

	VkImgui_DrawInspectorChrome();
	VkImgui_DrawViewport();
	VkImgui_DrawShaderEditor();
	VkImgui_DrawObjects();
	VkImgui_DrawInspector();
	VkImgui_DrawPostFXPanel();
	VkImgui_DrawPhysicsPanel();
	VkImgui_DrawVolumetricsPanel();
	VkImgui_DrawOceanPanel();
	VkImgui_DrawProfiler();
	VkImgui_DrawSimRenderDebugHud();
	VkImgui_DrawStudioMapPanel();
	VkImgui_DrawStudioConsolePanel();
	VkImgui_DrawStudioEntitiesPanel();
	VkImgui_DrawStudioPaintPanel();
	VkImgui_DrawStudioAnimationPanel();
	VkImgui_DrawOscarPanel();

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
