/*
===========================================================================
Mod-Specific ECS Systems

Mod-specific system implementations for inventory, dialogs, and equipment.
===========================================================================
*/

#ifdef USE_ENTT

#include "g_ecs_mod_components.h"
#include "g_ecs.h"
#include <entt/entt.hpp>

extern gentity_t g_entities[MAX_GENTITIES];

/*
================
InventorySystem
Manage inventory components - sync with traditional inventory system
================
*/
void G_ECS_InventorySystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<InventoryComponent>();
	
	for (auto entity : view) {
		auto &inventory = view.get<InventoryComponent>(entity);
		
		// Mark inventory as needing sync if dirty
		if (inventory.inventory.dirty) {
			// This will trigger save on next inventory save call
			// Actual save logic remains in traditional system
		}
	}
}

/*
================
DialogSystem
Handle dialog interactions and state
================
*/
void G_ECS_DialogSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<DialogComponent>();
	
	for (auto entity : view) {
		(void)entity;
		// Update dialog state
		// Dialog logic remains in traditional system for now
		// This system can be extended to handle ECS-based dialog interactions
	}
}

/*
================
EquipmentSystem
Apply equipment stat modifiers to entities
================
*/
void G_ECS_EquipmentSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	// Find entities with both InventoryComponent and EquipmentComponent
	auto view = registry.view<InventoryComponent, EquipmentComponent>();
	
	for (auto entity : view) {
		auto &inventory = view.get<InventoryComponent>(entity);
		auto &equipment = view.get<EquipmentComponent>(entity);
		
		// Calculate total stats from equipped items
		memset(&equipment.totalStats, 0, sizeof(equipment_stats_t));
		
		for (int i = 0; i < EQUIP_SLOT_MAX; i++) {
			if (inventory.inventory.equipment[i].itemId >= 0) {
				equipment_stats_t *stats = &inventory.inventory.equipment[i].stats;
				equipment.totalStats.damage_multiplier *= stats->damage_multiplier;
				equipment.totalStats.rof_multiplier *= stats->rof_multiplier;
				equipment.totalStats.accuracy_bonus += stats->accuracy_bonus;
				equipment.totalStats.armor_bonus += stats->armor_bonus;
				equipment.totalStats.health_bonus += stats->health_bonus;
				equipment.hasEquipment[i] = qtrue;
			} else {
				equipment.hasEquipment[i] = qfalse;
			}
		}
	}
}

/*
================
G_ECS_ModSystems_RunFrame
Run all mod-specific ECS systems for a frame
================
*/
void G_ECS_ModSystems_RunFrame(void) {
	// Check if registry is initialized
	if (ECS_GetRegistry() == nullptr) {
		return; // System not initialized
	}
	
	// Run mod systems
	G_ECS_InventorySystem_Update();
	G_ECS_DialogSystem_Update();
	G_ECS_EquipmentSystem_Update();
}

#endif // USE_ENTT

