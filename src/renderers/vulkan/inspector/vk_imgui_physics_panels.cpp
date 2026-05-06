/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Physics inspector panel (Bullet / ragdoll cvars).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"
#include "vk_imgui_draw_defaults.hpp"

typedef struct {
	float gravity;
	float stiffness;
	float damping;
} vkPhysicsPanelState_t;

static const vkPhysicsPanelState_t vkPhysicsDefaults = {
	-800.0f, 0.8f, 0.4f
};

static vkPhysicsPanelState_t vkPhysicsState = vkPhysicsDefaults;

static void VkImgui_ResetPhysicsDefaults( void )
{
	vkPhysicsState = vkPhysicsDefaults;
	ri.Cvar_SetValue( "phys_gravity", -800.0f );
	ri.Cvar_SetValue( "phys_ragdoll_stiffness", 0.8f );
	ri.Cvar_SetValue( "phys_ragdoll_damping", 0.4f );
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

#endif /* USE_IMGUI */
