/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Inspector chrome: theme, main menu bar, popups, and workspace dock presets
(id Tech 4–style editor layout via ImGui DockBuilder).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"

extern "C" void VkImgui_ApplyInspectorStyle( void )
{
	ImVec4 *colors = ImGui::GetStyle().Colors;
	/* Neutral dark editor (closer to id Tech 4 tool feel than flat black). */
	colors[ImGuiCol_Text] = ImVec4( 0.96f, 0.96f, 0.96f, 1.00f );
	colors[ImGuiCol_TextDisabled] = ImVec4( 0.45f, 0.45f, 0.46f, 1.00f );
	colors[ImGuiCol_WindowBg] = ImVec4( 0.11f, 0.11f, 0.12f, 1.00f );
	colors[ImGuiCol_ChildBg] = ImVec4( 0.09f, 0.09f, 0.10f, 1.00f );
	colors[ImGuiCol_PopupBg] = ImVec4( 0.14f, 0.14f, 0.15f, 0.98f );
	colors[ImGuiCol_Border] = ImVec4( 0.22f, 0.22f, 0.24f, 1.00f );
	colors[ImGuiCol_FrameBg] = ImVec4( 0.16f, 0.16f, 0.18f, 1.00f );
	colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.22f, 0.22f, 0.25f, 1.00f );
	colors[ImGuiCol_FrameBgActive] = ImVec4( 0.26f, 0.26f, 0.29f, 1.00f );
	colors[ImGuiCol_MenuBarBg] = ImVec4( 0.08f, 0.08f, 0.09f, 1.00f );
	colors[ImGuiCol_TitleBg] = ImVec4( 0.08f, 0.08f, 0.09f, 1.00f );
	colors[ImGuiCol_TitleBgActive] = ImVec4( 0.10f, 0.10f, 0.11f, 1.00f );
	colors[ImGuiCol_CheckMark] = ImVec4( 0.45f, 0.72f, 0.95f, 1.00f );
	colors[ImGuiCol_SliderGrab] = ImVec4( 0.36f, 0.58f, 0.82f, 1.00f );
	colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.50f, 0.72f, 0.95f, 1.00f );
	colors[ImGuiCol_Button] = ImVec4( 0.18f, 0.18f, 0.20f, 1.00f );
	colors[ImGuiCol_ButtonHovered] = ImVec4( 0.26f, 0.26f, 0.30f, 1.00f );
	colors[ImGuiCol_ButtonActive] = ImVec4( 0.30f, 0.30f, 0.35f, 1.00f );
	colors[ImGuiCol_Header] = ImVec4( 0.20f, 0.20f, 0.23f, 0.85f );
	colors[ImGuiCol_HeaderHovered] = ImVec4( 0.26f, 0.26f, 0.30f, 0.90f );
	colors[ImGuiCol_HeaderActive] = ImVec4( 0.30f, 0.30f, 0.35f, 1.00f );
	colors[ImGuiCol_Separator] = ImVec4( 0.25f, 0.25f, 0.28f, 1.00f );
	colors[ImGuiCol_Tab] = ImVec4( 0.14f, 0.14f, 0.16f, 1.00f );
	colors[ImGuiCol_TabHovered] = ImVec4( 0.22f, 0.22f, 0.25f, 1.00f );
	colors[ImGuiCol_TabSelected] = ImVec4( 0.24f, 0.22f, 0.20f, 1.00f );
	colors[ImGuiCol_TabDimmed] = ImVec4( 0.12f, 0.12f, 0.13f, 1.00f );
	colors[ImGuiCol_TabDimmedSelected] = ImVec4( 0.20f, 0.18f, 0.16f, 1.00f );
	colors[ImGuiCol_DockingPreview] = ImVec4( 0.33f, 0.67f, 0.86f, 0.55f );
	colors[ImGuiCol_DockingEmptyBg] = ImVec4( 0.06f, 0.06f, 0.07f, 1.00f );

	ImGuiStyle &style = ImGui::GetStyle();
	style.IndentSpacing = 22.0f;
	style.ScrollbarSize = 14.0f;
	style.GrabMinSize = 10.0f;
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowRounding = 6.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 5.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 4.0f;
	style.WindowPadding = ImVec2( 10.0f, 8.0f );
	style.FramePadding = ImVec2( 8.0f, 4.0f );
	style.ItemSpacing = ImVec2( 8.0f, 6.0f );

	ri.Printf( PRINT_ALL,
		"[VK][imgui] inspector editor style applied (Window reset layout for id Tech 4–style dock)\n" );
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
			"PostFX and related panels drive renderer cvars. "
			"Set \\r_studio_tools 1 for id Studio-style session + command strip (see docs/IN_ENGINE_STUDIO_TOOLS.md)." );
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
		ImGui::BulletText( "Window: show/hide docked panels; reset workspace layout" );
		ImGui::BulletText( "Render Mode: \\r_pbr_debug modes (0-8 active)" );
		ImGui::BulletText( "Developer: \\r_speeds, \\r_showtris, \\r_imgui, \\r_studio_tools" );
		ImGui::Spacing();
		if ( ImGui::Button( "Close", ImVec2( 120.0f, 0.0f ) ) ) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

namespace {

static const char *vkRenderModes[] = {
	"Final Image",
	"Direct Lighting",
	"IBL Specular",
	"Diffuse Irradiance",
	"Env/Irradiance Samples",
	"Glint D (log)",
	"Glint Lambda (LOD)",
	"Glint Compensation",
	"Glint Weight",
	"View Direction",
	"Tangents",
	"Light Color",
	"Emissive",
	"Reflectance",
	"NdotL",
	"NdotH",
	"NdotV",
	"IBL Contribution",
	"Volumetric Fog Density",
	"Depth Buffer",
	"Motion Vectors",
	"Fluid Density"
};

static const unsigned int vkRenderModeCount =
	(unsigned int)( sizeof( vkRenderModes ) / sizeof( vkRenderModes[0] ) );

} // namespace

void VkImgui_ResetInspectorWorkspaceLayout( void )
{
	ImGuiID dockspace_id = ImGui::GetID( "InspectorDockSpace" );
	const ImVec2 sz = ImGui::GetWindowSize();

	ImGui::DockBuilderRemoveNode( dockspace_id );
	ImGui::DockBuilderAddNode( dockspace_id, ImGuiDockNodeFlags_DockSpace );
	ImGui::DockBuilderSetNodeSize( dockspace_id, sz );

	ImGuiID id_main = dockspace_id;
	ImGuiID id_left = 0;
	ImGuiID id_right = 0;
	ImGuiID id_bottom = 0;

	ImGui::DockBuilderSplitNode( id_main, ImGuiDir_Left, 0.24f, &id_left, &id_main );
	ImGui::DockBuilderSplitNode( id_main, ImGuiDir_Right, 0.28f, &id_right, &id_main );
	ImGui::DockBuilderSplitNode( id_main, ImGuiDir_Down, 0.26f, &id_bottom, &id_main );

	vkWindows.viewport.open = qtrue;
	vkWindows.objects.open = qtrue;
	vkWindows.inspector.open = qtrue;
	vkWindows.shader.open = qtrue;
	vkWindows.postfx.open = qtrue;
	vkWindows.profiler.open = qtrue;
	vkWindows.physics.open = qtrue;
	vkWindows.volumetrics.open = qtrue;
	if ( r_studio_tools && r_studio_tools->integer ) {
		vkWindows.studioMap.open = qtrue;
		vkWindows.studioConsole.open = qtrue;
		vkWindows.studioAuthor.open = qtrue;
	}

	ImGui::DockBuilderDockWindow( "Viewport", id_main );
	ImGui::DockBuilderDockWindow( "Objects", id_left );
	ImGui::DockBuilderDockWindow( "Inspector", id_left );
	ImGui::DockBuilderDockWindow( "Shaders", id_right );
	ImGui::DockBuilderDockWindow( "PostFX", id_right );
	ImGui::DockBuilderDockWindow( "GPU Profiler", id_bottom );
	ImGui::DockBuilderDockWindow( "Physics", id_bottom );
	ImGui::DockBuilderDockWindow( "Volumetrics", id_bottom );
	ImGui::DockBuilderDockWindow( "Studio / Session", id_bottom );
	ImGui::DockBuilderDockWindow( "Studio / Console", id_bottom );
	ImGui::DockBuilderDockWindow( "Studio / Author", id_right );

	ImGui::DockBuilderFinish( dockspace_id );
	ri.Printf( PRINT_DEVELOPER, "[VK][imgui] workspace layout reset (editor default dock)\n" );
}

static void VkImgui_DrawMenuBar( void )
{
	ImGuiIO &io = ImGui::GetIO();
	const float fps = io.Framerate > 0.0f ? io.Framerate : 0.0f;
	const float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;

	if ( ImGui::BeginMainMenuBar() ) {
		if ( ImGui::BeginMenu( "File" ) ) {
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

		if ( ImGui::BeginMenu( "Help" ) ) {
			if ( ImGui::MenuItem( "Inspector shortcuts" ) ) {
				ImGui::OpenPopup( "InspectorShortcuts" );
			}
			if ( ImGui::MenuItem( "About inspector" ) ) {
				ImGui::OpenPopup( "AboutInspector" );
			}
			ImGui::EndMenu();
		}

		if ( r_studio_tools && r_studio_tools->integer ) {
			if ( ImGui::BeginMenu( "Studio" ) ) {
				ImGui::MenuItem( "Session / map strip", nullptr, (bool *)&vkWindows.studioMap.open );
				ImGui::MenuItem( "Command strip", nullptr, (bool *)&vkWindows.studioConsole.open );
				ImGui::MenuItem( "Author (presets / QA)", nullptr, (bool *)&vkWindows.studioAuthor.open );
				ImGui::Separator();
				ImGui::TextDisabled( "Entity key reference: docs/EDITOR_BRIDGE.md" );
				ImGui::EndMenu();
			}
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
			if ( r_studio_tools ) {
				bool stOn = r_studio_tools->integer != 0;
				if ( ImGui::Checkbox( "Studio tools (r_studio_tools)", &stOn ) ) {
					ri.Cvar_SetValue( "r_studio_tools", stOn ? 1.0f : 0.0f );
					if ( stOn ) {
						vkWindows.studioMap.open = qtrue;
						vkWindows.studioConsole.open = qtrue;
						vkWindows.studioAuthor.open = qtrue;
					}
				}
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip(
						"Adds id Studio-style Session, Console, and Author docked panels and a Studio menu. "
						"Requires r_imgui 1. See docs/IN_ENGINE_STUDIO_TOOLS.md." );
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

		if ( ImGui::BeginMenu( "Window" ) ) {
			ImGui::MenuItem( "Viewport", nullptr, (bool *)&vkWindows.viewport.open );
			ImGui::MenuItem( "Shaders", nullptr, (bool *)&vkWindows.shader.open );
			ImGui::MenuItem( "Profiler", nullptr, (bool *)&vkWindows.profiler.open );
			ImGui::MenuItem( "PostFX", nullptr, (bool *)&vkWindows.postfx.open );
			ImGui::MenuItem( "Physics", nullptr, (bool *)&vkWindows.physics.open );
			ImGui::MenuItem( "Volumetrics", nullptr, (bool *)&vkWindows.volumetrics.open );
			ImGui::MenuItem( "Objects", nullptr, (bool *)&vkWindows.objects.open );
			ImGui::MenuItem( "Inspector", nullptr, (bool *)&vkWindows.inspector.open );
			if ( r_studio_tools && r_studio_tools->integer ) {
				ImGui::Separator();
				ImGui::MenuItem( "Studio / Session", nullptr, (bool *)&vkWindows.studioMap.open );
				ImGui::MenuItem( "Studio / Console", nullptr, (bool *)&vkWindows.studioConsole.open );
				ImGui::MenuItem( "Studio / Author", nullptr, (bool *)&vkWindows.studioAuthor.open );
			}
			ImGui::Separator();
			if ( ImGui::MenuItem( "Reset workspace layout" ) ) {
				VkImgui_ResetInspectorWorkspaceLayout();
			}
			if ( ImGui::IsItemHovered() ) {
				ImGui::SetTooltip(
					"Re-docks panels to a default editor layout (viewport center, scene tools left, "
					"shaders/post right, profiler and sim along the bottom). Same spirit as id Tech 4 dock presets." );
			}
			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Render Mode" ) ) {
			int pbrDbg = ri.Cvar_VariableIntegerValue( "r_pbr_debug" );
			for ( unsigned int i = 0; i < vkRenderModeCount; i++ ) {
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

		ImGui::SameLine( ImGui::GetWindowWidth() - 220 );
		ImGui::Text( "%.3f ms/frame (%.1f FPS)", ms, fps );

		ImGui::EndMainMenuBar();
	}
}

void VkImgui_DrawInspectorChrome( void )
{
	VkImgui_DrawMenuBar();
	VkImgui_DrawAboutInspectorPopup();
	VkImgui_DrawShortcutsPopup();
}

#endif /* USE_IMGUI */
