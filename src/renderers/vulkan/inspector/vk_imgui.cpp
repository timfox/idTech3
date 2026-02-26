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
}

#include "vk_imgui.h"

vkImguiInspector_t   vkInspector;
vkImguiWindows_t     vkWindows;
vkImguiGlobal_t      vkImguiState;

static ImGuiContext *imguiContext = nullptr;

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

	if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
		static float threshold = 0.6f, intensity = 0.5f;
		ImGui::SliderFloat("Threshold", &threshold, 0.0f, 2.0f);
		ImGui::SliderFloat("Intensity", &intensity, 0.0f, 2.0f);
	}

	if (ImGui::CollapsingHeader("Tonemapping", ImGuiTreeNodeFlags_DefaultOpen)) {
		static int tonemapMode = 2;
		const char *modes[] = { "None", "Reinhard", "ACES" };
		ImGui::Combo("Mode", &tonemapMode, modes, 3);
		static float exposure = 1.0f;
		ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f);
	}

	if (ImGui::CollapsingHeader("Lens Effects")) {
		static float vignette = 0.0f, vigRadius = 0.75f, chromAb = 0.0f, grain = 0.0f;
		ImGui::SliderFloat("Vignette", &vignette, 0.0f, 1.0f);
		ImGui::SliderFloat("Vignette Radius", &vigRadius, 0.3f, 1.5f);
		ImGui::SliderFloat("Chromatic Aberration", &chromAb, 0.0f, 5.0f);
		ImGui::SliderFloat("Film Grain", &grain, 0.0f, 1.0f);
	}

	if (ImGui::CollapsingHeader("SSR")) {
		static float maxDist = 100.0f, stepSz = 1.0f, thick = 0.5f, inten = 0.8f;
		ImGui::SliderFloat("Max Distance", &maxDist, 10.0f, 500.0f);
		ImGui::SliderFloat("Step Size", &stepSz, 0.1f, 5.0f);
		ImGui::SliderFloat("Thickness", &thick, 0.1f, 5.0f);
		ImGui::SliderFloat("SSR Intensity", &inten, 0.0f, 1.0f);
	}

	ImGui::End();
}

extern "C" void VkImgui_DrawPhysicsPanel(void) {
	if (!vkWindows.physics.open) return;
	ImGui::Begin("Physics", (bool *)&vkWindows.physics.open);

	if (ImGui::CollapsingHeader("Bullet Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Rigid Bodies: --");
		ImGui::Text("Constraints: --");
		ImGui::Text("Ragdolls: --");
		ImGui::Text("DMM Objects: --");
		static float gravity = -800.0f;
		ImGui::SliderFloat("Gravity", &gravity, -2000.0f, 0.0f);
		static float stiffness = 0.8f, damping = 0.4f;
		ImGui::SliderFloat("Ragdoll Stiffness", &stiffness, 0.0f, 1.0f);
		ImGui::SliderFloat("Ragdoll Damping", &damping, 0.0f, 1.0f);
	}

	ImGui::End();
}

extern "C" void VkImgui_DrawVolumetricsPanel(void) {
	if (!vkWindows.volumetrics.open) return;
	ImGui::Begin("Volumetrics", (bool *)&vkWindows.volumetrics.open);

	if (ImGui::CollapsingHeader("Volumetric Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
		static float density = 0.02f, heightFalloff = 0.04f, scatter = 1.0f;
		ImGui::SliderFloat("Density", &density, 0.0f, 0.5f);
		ImGui::SliderFloat("Height Falloff", &heightFalloff, 0.0f, 0.5f);
		ImGui::SliderFloat("Scatter", &scatter, 0.0f, 5.0f);
	}

	if (ImGui::CollapsingHeader("Fluid Simulation")) {
		static float viscosity = 0.0001f, dissipation = 0.995f;
		ImGui::SliderFloat("Viscosity", &viscosity, 0.0f, 0.01f, "%.5f");
		ImGui::SliderFloat("Dissipation", &dissipation, 0.9f, 1.0f, "%.4f");
	}

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
