/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared ImGui "reset to defaults" modal for inspector cvar panels.
===========================================================================
*/
#pragma once

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"

inline void VkImgui_DrawDefaultsConfirmation( const char *buttonLabel, const char *popupId,
	const char *panelName, void ( *resetFn )( void ) )
{
	if ( ImGui::Button( buttonLabel ) ) {
		ImGui::OpenPopup( popupId );
	}

	if ( ImGui::BeginPopupModal( popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::Text( "Reset %s settings to defaults?", panelName );
		ImGui::Separator();

		if ( ImGui::Button( "Cancel", ImVec2( 120.0f, 0.0f ) ) ) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if ( ImGui::Button( "Apply Defaults", ImVec2( 140.0f, 0.0f ) ) ) {
			resetFn();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

#endif /* USE_IMGUI */
