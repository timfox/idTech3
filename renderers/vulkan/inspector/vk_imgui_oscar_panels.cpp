/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OSCAR / AIM buddy list panel (hybrid shared local account).
===========================================================================
*/

#ifdef USE_IMGUI

#include <cstring>

#include "vk_imgui_common.hpp"

namespace {

constexpr size_t kImName = 64;
constexpr size_t kImText = 256;

char g_buddyName[kImName]{};
char g_imText[kImText]{};
char g_roomName[kImName]{};
char g_roomText[kImText]{};

void ExecCmd( const char *cmd )
{
	char line[512];

	if ( !cmd || !cmd[0] ) {
		return;
	}
	Com_sprintf( line, sizeof( line ), "%s\n", cmd );
	ri.Cmd_ExecuteText( EXEC_APPEND, line );
}

} // namespace

extern "C" void VkImgui_DrawOscarPanel( void )
{
	const int uiOn = ri.Cvar_VariableIntegerValue( "cl_oscarUi" );
	const int enable = ri.Cvar_VariableIntegerValue( "oscar_enable" );
	const char *state;
	const char *room;
	const char *roster;
	const char *account;

	if ( !uiOn || !vkWindows.oscar.open ) {
		return;
	}

	ImGui::Begin( "OSCAR / AIM", (bool *)&vkWindows.oscar.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Hybrid AIM client: one shared oscar_account per process. "
			"Dedicated servers use a service screen name; the game client uses a local account. "
			"Password via IDTECH3_OSCAR_PASSWORD. See docs/OSCAR_INTEGRATION.md." );
	}
	ImGui::Separator();

	state = ri.Cvar_VariableString( "oscar_rosterGen" ); /* presence of snapshot cvars */
	(void)state;
	account = ri.Cvar_VariableString( "oscar_account" );
	room = ri.Cvar_VariableString( "oscar_defaultRoom" );
	roster = ri.Cvar_VariableString( "oscar_rosterSnapshot" );

	ImGui::Text( "oscar_enable: %d", enable );
	ImGui::Text( "account: %s", account[0] ? account : "(unset)" );
	ImGui::TextWrapped( "Use console oscar_status for live session state." );

	if ( ImGui::Button( "Connect" ) ) {
		ExecCmd( "oscar_connect" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Disconnect" ) ) {
		ExecCmd( "oscar_disconnect" );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Status" ) ) {
		ExecCmd( "oscar_status" );
	}

	ImGui::Separator();
	ImGui::TextUnformatted( "Buddy roster" );
	if ( ImGui::BeginChild( "OscarRoster", ImVec2( 0, 140 ), true ) ) {
		if ( !roster[0] ) {
			ImGui::TextDisabled( "(empty — oscar_buddy_add <name>)" );
		} else {
			const char *p = roster;
			while ( *p ) {
				char entry[128];
				const char *semi = strchr( p, ';' );
				size_t n = semi ? (size_t)( semi - p ) : strlen( p );
				char *colon;

				if ( n >= sizeof( entry ) ) {
					n = sizeof( entry ) - 1;
				}
				memcpy( entry, p, n );
				entry[n] = '\0';
				colon = strchr( entry, ':' );
				if ( colon ) {
					*colon = '\0';
					ImGui::BulletText( "%s — %s", entry, colon + 1 );
					if ( ImGui::IsItemClicked() ) {
						Q_strncpyz( g_buddyName, entry, sizeof( g_buddyName ) );
					}
				} else if ( entry[0] ) {
					ImGui::BulletText( "%s", entry );
				}
				if ( !semi ) {
					break;
				}
				p = semi + 1;
			}
		}
	}
	ImGui::EndChild();

	ImGui::InputText( "Buddy", g_buddyName, sizeof( g_buddyName ) );
	if ( ImGui::Button( "Add buddy" ) && g_buddyName[0] ) {
		char cmd[128];
		Com_sprintf( cmd, sizeof( cmd ), "oscar_buddy_add %s", g_buddyName );
		ExecCmd( cmd );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Remove buddy" ) && g_buddyName[0] ) {
		char cmd[128];
		Com_sprintf( cmd, sizeof( cmd ), "oscar_buddy_del %s", g_buddyName );
		ExecCmd( cmd );
	}

	ImGui::InputText( "IM text", g_imText, sizeof( g_imText ) );
	if ( ImGui::Button( "Send IM" ) && g_buddyName[0] && g_imText[0] ) {
		char cmd[MAX_STRING_CHARS];
		Com_sprintf( cmd, sizeof( cmd ), "oscar_im %s %s", g_buddyName, g_imText );
		ExecCmd( cmd );
	}

	ImGui::Separator();
	ImGui::TextUnformatted( "Chat room" );
	ImGui::InputText( "Room", g_roomName, sizeof( g_roomName ) );
	if ( !g_roomName[0] && room[0] ) {
		Q_strncpyz( g_roomName, room, sizeof( g_roomName ) );
	}
	if ( ImGui::Button( "Join room" ) && g_roomName[0] ) {
		char cmd[128];
		Com_sprintf( cmd, sizeof( cmd ), "oscar_join %s", g_roomName );
		ExecCmd( cmd );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Leave room" ) ) {
		ExecCmd( "oscar_leave" );
	}
	ImGui::InputText( "Room message", g_roomText, sizeof( g_roomText ) );
	if ( ImGui::Button( "Announce" ) && g_roomText[0] ) {
		char cmd[MAX_STRING_CHARS];
		Com_sprintf( cmd, sizeof( cmd ), "oscar_announce %s", g_roomText );
		ExecCmd( cmd );
	}

	ImGui::End();
}

#endif /* USE_IMGUI */
