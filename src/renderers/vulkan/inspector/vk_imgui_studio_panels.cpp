/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

id Studio-style session + command strip (C++: std::vector command history).
===========================================================================
*/

#ifdef USE_IMGUI

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "vk_imgui_common.hpp"
#include "../../../qcommon/engine_sprite_map.h"
#include "../../../qcommon/engine_decal_map.h"
#include "../tr_sprite_props.h"

namespace {

constexpr size_t kCmdLen = 256;
constexpr size_t kMaxHistory = 48;

std::vector<std::string> g_history;
std::array<char, kCmdLen> g_input{};

void PushHistoryLine( std::string_view line )
{
	if ( line.empty() ) {
		return;
	}
	if ( g_history.size() >= kMaxHistory ) {
		g_history.erase( g_history.begin() );
	}
	g_history.emplace_back( line );
}

void ExecTrimmed( char *start )
{
	char *end;

	if ( !start || !start[0] ) {
		return;
	}
	while ( *start == ' ' || *start == '\t' ) {
		start++;
	}
	if ( *start == '\0' ) {
		return;
	}
	end = start + strlen( start );
	while ( end > start && ( end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n' ) ) {
		*--end = '\0';
	}
	if ( *start == '\0' ) {
		return;
	}
	PushHistoryLine( start );
	{
		char exec[kCmdLen + 8];

		Com_sprintf( exec, sizeof( exec ), "%s\n", start );
		ri.Printf( PRINT_DEVELOPER, "[VK][studio] exec: %s", exec );
		ri.Cmd_ExecuteText( EXEC_APPEND, exec );
	}
}

} // namespace

extern "C" void VkImgui_DrawStudioMapPanel( void )
{
	if ( !r_studio_tools || !r_studio_tools->integer ) {
		return;
	}
	if ( !vkWindows.studioMap.open ) {
		return;
	}

	ImGui::Begin( "Studio / Session", (bool *)&vkWindows.studioMap.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Live session readout (engine cvars). For entity keys shared with idTech3Radiant, see docs/EDITOR_BRIDGE.md in the repo." );
	}
	ImGui::Separator();
	{
		const char *mapn = ri.Cvar_VariableString( "mapname" );
		const char *fsGame = ri.Cvar_VariableString( "fs_game" );
		const char *fsBase = ri.Cvar_VariableString( "fs_basegame" );
		const char *svHost = ri.Cvar_VariableString( "sv_hostname" );

		ImGui::Text( "mapname: %s", mapn[0] ? mapn : "(unset)" );
		ImGui::Text( "fs_game: %s", fsGame[0] ? fsGame : "(default)" );
		ImGui::Text( "fs_basegame: %s", fsBase[0] ? fsBase : "(default)" );
		ImGui::Text( "sv_hostname: %s", svHost[0] ? svHost : "(unset)" );
	}
	ImGui::SeparatorText( "Quick commands" );
	if ( ImGui::Button( "map_restart" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "map_restart\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "disconnect" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "vid_restart" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
	}
	ImGui::SeparatorText( "Dev (cheat-class)" );
	if ( ImGui::Button( "noclip" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "noclip\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "god" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "god\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "notarget" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "notarget\n" );
	}
	ImGui::End();
}

extern "C" void VkImgui_DrawStudioConsolePanel( void )
{
	if ( !r_studio_tools || !r_studio_tools->integer ) {
		return;
	}
	if ( !vkWindows.studioConsole.open ) {
		return;
	}

	ImGui::Begin( "Studio / Console", (bool *)&vkWindows.studioConsole.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Runs text through the main command buffer (same as the drop-down console). "
			"History is local to this panel." );
	}
	ImGui::Separator();
	ImGui::BeginChild( "StudioCmdHistory", ImVec2( 0.0f, -ImGui::GetFrameHeightWithSpacing() ), qfalse,
		ImGuiWindowFlags_HorizontalScrollbar );
	for ( const std::string &line : g_history ) {
		ImGui::TextUnformatted( line.c_str() );
	}
	if ( !g_history.empty() ) {
		ImGui::SetScrollHereY( 1.0f );
	}
	ImGui::EndChild();
	if ( ImGui::InputText( "##studio_cmd", g_input.data(), g_input.size(),
		    ImGuiInputTextFlags_EnterReturnsTrue ) ) {
		ExecTrimmed( g_input.data() );
		g_input[0] = '\0';
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Run" ) ) {
		ExecTrimmed( g_input.data() );
		g_input[0] = '\0';
	}
	ImGui::End();
}

extern "C" void VkImgui_DrawStudioEntitiesPanel( void )
{
	static int exportIndex = -1;
	engineSpriteMapList_t spriteList;
	engineDecalMapList_t decalList;

	if ( !r_studio_tools || !r_studio_tools->integer ) {
		return;
	}
	if ( !vkWindows.studioEntities.open ) {
		return;
	}

	ImGui::Begin( "Studio / Entities", (bool *)&vkWindows.studioEntities.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Lists misc_billboard / flipbook / imposter / misc_decal from the loaded BSP entity lump. "
			"Export writes Radiant-style snippets to studio_exportents.cfg." );
	}

	{
		const char *entityString = R_MapProps_EntityString();
		if ( !entityString || !entityString[0] ) {
			ImGui::TextWrapped( "No world entity lump loaded." );
			ImGui::End();
			return;
		}

		EngineSpriteMap_Parse( entityString, &spriteList );
		EngineDecalMap_Parse( entityString, &decalList );
	}

	ImGui::SeparatorText( "Sprites" );
	for ( int i = 0; i < spriteList.count; i++ ) {
		const engineSpriteMapDef_t *def = &spriteList.defs[i];
		const char *typeLabel = "billboard";
		char label[256];

		if ( def->type == ENGINE_SPRITE_FLIPBOOK ) {
			typeLabel = "flipbook";
		} else if ( def->type == ENGINE_SPRITE_IMPOSTER ) {
			typeLabel = "imposter";
		}
		Com_sprintf( label, sizeof( label ), "%s [%s] %.0f %.0f %.0f",
			typeLabel, def->shader, def->origin[0], def->origin[1], def->origin[2] );
		if ( ImGui::Selectable( label, exportIndex == i ) ) {
			exportIndex = i;
		}
	}

	ImGui::SeparatorText( "Decals" );
	for ( int i = 0; i < decalList.count; i++ ) {
		const engineDecalMapDef_t *def = &decalList.defs[i];
		const int row = spriteList.count + i;
		char label[256];

		Com_sprintf( label, sizeof( label ), "decal [%s] %.0f %.0f %.0f",
			def->shader, def->origin[0], def->origin[1], def->origin[2] );
		if ( ImGui::Selectable( label, exportIndex == row ) ) {
			exportIndex = row;
		}
	}

	if ( ImGui::Button( "Export selection to studio_exportents.cfg" ) ) {
		char cmd[512];
		if ( exportIndex >= 0 && exportIndex < spriteList.count ) {
			const engineSpriteMapDef_t *def = &spriteList.defs[exportIndex];
			const char *cls = "misc_billboard";
			if ( def->type == ENGINE_SPRITE_FLIPBOOK ) {
				cls = "misc_flipbook";
			} else if ( def->type == ENGINE_SPRITE_IMPOSTER ) {
				cls = "misc_imposter";
			}
			Com_sprintf( cmd, sizeof( cmd ),
				"echo \"{\\n\\\"classname\\\" \\\"%s\\\"\\n\\\"origin\\\" \\\"%.0f %.0f %.0f\\\"\\n"
				"\\\"shader\\\" \\\"%s\\\"\\n\\\"scale\\\" \\\"%.0f\\\"\\n}\" > studio_exportents.cfg\n",
				cls, def->origin[0], def->origin[1], def->origin[2], def->shader, def->radius );
		} else if ( exportIndex >= spriteList.count &&
			exportIndex < spriteList.count + decalList.count ) {
			const engineDecalMapDef_t *def =
				&decalList.defs[exportIndex - spriteList.count];
			Com_sprintf( cmd, sizeof( cmd ),
				"echo \"{\\n\\\"classname\\\" \\\"misc_decal\\\"\\n\\\"origin\\\" \\\"%.0f %.0f %.0f\\\"\\n"
				"\\\"shader\\\" \\\"%s\\\"\\n\\\"scale\\\" \\\"%.0f\\\"\\n}\" > studio_exportents.cfg\n",
				def->origin[0], def->origin[1], def->origin[2], def->shader, def->radius );
		} else {
			Q_strncpyz( cmd, "echo \"// select an entity first\" > studio_exportents.cfg\n", sizeof( cmd ) );
		}
		ri.Cmd_ExecuteText( EXEC_APPEND, cmd );
		ri.Printf( PRINT_ALL, "[VK][studio] wrote studio_exportents.cfg\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "r_spritePropsMapParse 1" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "r_spritePropsMapParse 1\n" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "r_decalPropsMapParse 1" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "r_decalPropsMapParse 1\n" );
	}
	ImGui::End();
}

extern "C" void VkImgui_DrawStudioAnimationPanel( void )
{
	if ( !r_studio_tools || !r_studio_tools->integer ) {
		return;
	}

	ImGui::Begin( "Studio / Animation", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
	ImGui::TextDisabled( "animgraph/*.json — see docs/ANIMGRAPH.md" );
	if ( ImGui::Button( "g_animgraph 1" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "set g_animgraph 1\n" );
	}
	ImGui::SameLine();
	if ( ImGui::InputText( "##ag_path", g_input.data(), g_input.size(),
		    ImGuiInputTextFlags_EnterReturnsTrue ) ) {
		char cmd[320];
		Com_sprintf( cmd, sizeof( cmd ), "echo load animgraph: %s\n", g_input.data() );
		ri.Cmd_ExecuteText( EXEC_APPEND, cmd );
	}
	ImGui::End();
}

#endif /* USE_IMGUI */
