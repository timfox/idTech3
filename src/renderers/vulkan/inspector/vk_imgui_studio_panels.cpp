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

static void StudioExec( const char *cmd )
{
	if ( !cmd || !cmd[0] ) {
		return;
	}
	ri.Cmd_ExecuteText( EXEC_APPEND, va( "%s\n", cmd ) );
}

static void StudioApplyClassicPreset( void )
{
	StudioExec( "classic_mod" );
	StudioExec( "vid_restart" );
	ri.Printf( PRINT_ALL, "[VK][studio] applied classic Q3/OA preset (classic_mod)\n" );
}

static void StudioApplyModernPreset( void )
{
	ri.Cvar_Set( "r_classicMod", "0" );
	ri.Cvar_Set( "r_forwardPlus", "1" );
	ri.Cvar_Set( "r_volumetricFog", "1" );
	ri.Cvar_Set( "r_ssao", "1" );
	ri.Cvar_Set( "r_bloom", "1" );
	StudioExec( "vid_restart" );
	ri.Printf( PRINT_ALL, "[VK][studio] applied modern Vulkan preset (r_classicMod 0)\n" );
}

extern "C" void VkImgui_DrawStudioAuthorPanel( void )
{
	if ( !r_studio_tools || !r_studio_tools->integer ) {
		return;
	}
	if ( !vkWindows.studioAuthor.open ) {
		return;
	}

	ImGui::Begin( "Studio / Author", (bool *)&vkWindows.studioAuthor.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Author workflow: renderer presets, Forward+ debug, beta trace QA. "
			"See docs/IN_ENGINE_STUDIO_TOOLS.md and docs/BETA_AUTOMATED_TESTING.md." );
	}
	ImGui::SeparatorText( "Renderer presets" );
	if ( ImGui::Button( "Classic (Q3/OA)" ) ) {
		StudioApplyClassicPreset();
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Modern (Vulkan)" ) ) {
		StudioApplyModernPreset();
	}
	ImGui::SameLine();
	if ( ImGui::Button( "FBO safe cfg" ) ) {
		StudioExec( "exec q3_fbo_safe" );
	}
	if ( ImGui::Button( "exec q3_classic_mod.cfg" ) ) {
		StudioExec( "exec q3_classic_mod" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "exec q3_vulkan_compat.cfg" ) ) {
		StudioExec( "exec q3_vulkan_compat" );
	}
	{
		const int classic = ri.Cvar_VariableIntegerValue( "r_classicMod" );
		const int fp = ri.Cvar_VariableIntegerValue( "r_forwardPlus" );
		ImGui::Text( "r_classicMod=%d  r_forwardPlus=%d", classic, fp );
	}

	ImGui::SeparatorText( "Forward+" );
	{
		int fpOn = ri.Cvar_VariableIntegerValue( "r_forwardPlus" );
		if ( ImGui::Checkbox( "r_forwardPlus", (bool *)&fpOn ) ) {
			ri.Cvar_Set( "r_forwardPlus", fpOn ? "1" : "0" );
			StudioExec( "vid_restart" );
		}
	}
	{
		float fpDbg = ri.Cvar_VariableValue( "r_forwardPlusDebug" );
		if ( ImGui::SliderFloat( "r_forwardPlusDebug", &fpDbg, 0.0f, 0.35f, "%.2f" ) ) {
			ri.Cvar_Set( "r_forwardPlusDebug", va( "%f", fpDbg ) );
		}
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "Tile light-count heatmap overlay on PBR (requires r_forwardPlus 1)." );
		}
	}
	{
		int fpShade = ri.Cvar_VariableIntegerValue( "r_forwardPlusShade" );
		if ( ImGui::SliderInt( "r_forwardPlusShade", &fpShade, 0, 4 ) ) {
			ri.Cvar_Set( "r_forwardPlusShade", va( "%d", fpShade ) );
		}
	}
	{
		const int fpMax = ri.Cvar_VariableIntegerValue( "r_forwardPlusMaxPerTile" );
		ImGui::Text( "r_forwardPlusMaxPerTile=%d (latched; vid_restart after change)", fpMax );
	}

	ImGui::SeparatorText( "Beta trace / QA" );
	{
		const int bt = ri.Cvar_VariableIntegerValue( "cl_betaTrace" );
		const int mode = ri.Cvar_VariableIntegerValue( "cl_betaTraceStudioMode" );
		const char *base = ri.Cvar_VariableString( "cl_betaTraceStudioBase" );
		static const char *const kModeNames[] = { "idle", "record", "replay", "test" };
		const char *modeName = "unknown";

		if ( mode >= 0 && mode <= 3 ) {
			modeName = kModeNames[mode];
		}
		ImGui::Text( "cl_betaTrace=%d  mode=%s", bt, modeName );
		ImGui::Text( "basename: %s", base[0] ? base : "(none)" );
	}
	if ( ImGui::Button( "beta_status" ) ) {
		StudioExec( "beta_status" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "beta_petri_status" ) ) {
		StudioExec( "beta_petri_status" );
	}
	if ( ImGui::Button( "beta_stop" ) ) {
		StudioExec( "beta_stop" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Screenshot (JPEG)" ) ) {
		StudioExec( "screenshotJPEG silent" );
	}

	ImGui::End();
}

#endif /* USE_IMGUI */
