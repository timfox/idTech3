/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Inspector chrome: theme, main menu bar, popups, and workspace dock presets
(id Tech 4–style editor layout via ImGui DockBuilder).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"
#include "vk_imgui_theme.hpp"

extern cvar_t *r_imguiTheme;

extern "C" void VkImgui_ApplyInspectorStyle( void )
{
	const int theme = ( r_imguiTheme && r_imguiTheme->integer == 1 ) ? 1 : 0;
	VkImguiTheme::Apply( theme );
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
			"Set \\r_studio_tools 1 for id Studio-style panels "
			"(Session, Console, Entities, Paint, Animation — see docs/IN_ENGINE_STUDIO_TOOLS.md)." );
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
		ImGui::BulletText( "Studio (r_studio_tools): Session, Console, Entities, Paint, Animation" );
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
		vkWindows.studioEntities.open = qtrue;
		vkWindows.studioPaint.open = qtrue;
		vkWindows.studioAnimation.open = qtrue;
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
	ImGui::DockBuilderDockWindow( "Studio / Entities", id_bottom );
	ImGui::DockBuilderDockWindow( "Studio / Paint", id_bottom );
	ImGui::DockBuilderDockWindow( "Studio / Animation", id_bottom );

	ImGui::DockBuilderFinish( dockspace_id );
	ri.Printf( PRINT_DEVELOPER, "[VK][imgui] workspace layout reset (editor default dock)\n" );
}

static void VkImgui_SetViewportDebugMode( int pbrDebug, int postDebug, int fogDebug )
{
	ri.Cvar_Set( "r_pbr_debug", va( "%d", pbrDebug ) );
	ri.Cvar_Set( "r_post_debug", va( "%d", postDebug ) );
	ri.Cvar_Set( "r_fogDebug", va( "%d", fogDebug ) );
}

static void VkImgui_DrawViewMenu( void )
{
	if ( !ImGui::BeginMenu( "View" ) ) {
		return;
	}

	if ( ImGui::MenuItem( "Main (final)" ) ) {
		VkImgui_SetViewportDebugMode( 0, 0, 0 );
	}
	if ( ImGui::MenuItem( "Pre-tonemap HDR" ) ) {
		VkImgui_SetViewportDebugMode( 0, 1, 0 );
	}
	if ( ImGui::MenuItem( "Luminance heatmap" ) ) {
		VkImgui_SetViewportDebugMode( 0, 2, 0 );
	}
	ImGui::Separator();
	if ( ImGui::BeginMenu( "Volumetrics" ) ) {
		if ( ImGui::MenuItem( "Extinction" ) ) {
			VkImgui_SetViewportDebugMode( 0, 0, 2 );
		}
		if ( ImGui::MenuItem( "Scattering" ) ) {
			VkImgui_SetViewportDebugMode( 0, 0, 3 );
		}
		if ( ImGui::MenuItem( "Spot shadow map" ) ) {
			VkImgui_SetViewportDebugMode( 0, 0, 8 );
		}
		if ( ImGui::MenuItem( "Point shadow map" ) ) {
			VkImgui_SetViewportDebugMode( 0, 0, 9 );
		}
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "PBR buffers" ) ) {
		if ( ImGui::MenuItem( "Direct lighting" ) ) {
			VkImgui_SetViewportDebugMode( 1, 0, 0 );
		}
		if ( ImGui::MenuItem( "IBL specular" ) ) {
			VkImgui_SetViewportDebugMode( 2, 0, 0 );
		}
		if ( ImGui::MenuItem( "Depth (PBR debug 19)" ) ) {
			VkImgui_SetViewportDebugMode( 19, 0, 0 );
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if ( r_imguiTheme ) {
		const int theme = r_imguiTheme->integer;
		if ( ImGui::MenuItem( "Theme: Pablo dark", nullptr, theme == 0 ) ) {
			ri.Cvar_Set( "r_imguiTheme", "0" );
			VkImgui_ApplyInspectorStyle();
			ri.Printf( PRINT_ALL, "[VK][imgui] theme: Pablo dark\n" );
		}
		if ( ImGui::MenuItem( "Theme: Spectrum light", nullptr, theme == 1 ) ) {
			ri.Cvar_Set( "r_imguiTheme", "1" );
			VkImgui_ApplyInspectorStyle();
			ri.Printf( PRINT_ALL, "[VK][imgui] theme: Spectrum light\n" );
		}
	}
	{
		int simDbg = ri.Cvar_VariableIntegerValue( "r_simRenderDebug" );
		if ( ImGui::MenuItem( "Sim render debug HUD", nullptr, simDbg >= 2 ) ) {
			ri.Cvar_Set( "r_simRenderDebug", simDbg >= 2 ? "0" : "2" );
		}
	}
	ImGui::EndMenu();
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

		VkImgui_DrawViewMenu();

		if ( r_studio_tools && r_studio_tools->integer ) {
			if ( ImGui::BeginMenu( "Studio" ) ) {
				ImGui::MenuItem( "Session / map strip", nullptr, (bool *)&vkWindows.studioMap.open );
				ImGui::MenuItem( "Command strip", nullptr, (bool *)&vkWindows.studioConsole.open );
				ImGui::MenuItem( "Entities (misc_*)", nullptr, (bool *)&vkWindows.studioEntities.open );
				ImGui::MenuItem( "Material paint", nullptr, (bool *)&vkWindows.studioPaint.open );
				ImGui::MenuItem( "Animation", nullptr, (bool *)&vkWindows.studioAnimation.open );
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
						vkWindows.studioEntities.open = qtrue;
						vkWindows.studioPaint.open = qtrue;
						vkWindows.studioAnimation.open = qtrue;
					}
				}
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip(
						"Adds id Studio-style Session, Console, Entities, Paint, and Animation panels "
						"plus a Studio menu. Requires r_imgui 1. See docs/IN_ENGINE_STUDIO_TOOLS.md." );
				}
			}
			{
				int sp = ri.Cvar_VariableIntegerValue( "r_speeds" );
				if ( sp < 0 ) {
					sp = 0;
				}
				if ( sp > 7 ) {
					sp = 7;
				}
				const int spPrev = sp;
				ImGui::SliderInt( "r_speeds (debug HUD)", &sp, 0, 7 );
				if ( sp != spPrev ) {
					ri.Cvar_Set( "r_speeds", va( "%d", sp ) );
				}
				if ( ImGui::IsItemHovered() ) {
					ImGui::SetTooltip( "Console stats overlay (cheat cvar). 0=off, 1=BSP, 2=patch cull, 3=cluster, 4=dlights, 5=zFar, 6=flares, 7=sim render." );
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
				ImGui::MenuItem( "Studio / Entities", nullptr, (bool *)&vkWindows.studioEntities.open );
				ImGui::MenuItem( "Studio / Paint", nullptr, (bool *)&vkWindows.studioPaint.open );
				ImGui::MenuItem( "Studio / Animation", nullptr, (bool *)&vkWindows.studioAnimation.open );
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
