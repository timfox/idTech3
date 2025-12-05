/*
===========================================================================
Mod-Specific ECS Components

Mod-specific component definitions that use the engine ECS API.
These components are for mod features like inventory, dialogs, and equipment.
===========================================================================
*/

#ifndef __G_ECS_MOD_COMPONENTS_H__
#define __G_ECS_MOD_COMPONENTS_H__

#if defined(USE_ENTT) && defined(__cplusplus)

#include <entt/entt.hpp>
#include "g_inventory_types.h"
#include "g_inventory.h"
#include "g_dialog.h"

// Inventory Component - Maps to existing inventory_t structure
struct InventoryComponent {
	inventory_t inventory;
	
	InventoryComponent() {
		memset(&inventory, 0, sizeof(inventory));
		inventory.numItems = 0;
		inventory.dirty = qfalse;
		for (int i = 0; i < EQUIP_SLOT_MAX; i++) {
			inventory.equipment[i].itemId = -1;
			inventory.equipment[i].slot = EQUIP_SLOT_NONE;
		}
	}
	
	InventoryComponent(const inventory_t &inv) {
		memcpy(&inventory, &inv, sizeof(inventory));
	}
};

// Dialog Component - Maps to existing dialog_t structure
struct DialogComponent {
	dialog_t dialog;
	int clientNum;  // Client viewing this dialog
	
	DialogComponent() : clientNum(-1) {
		memset(&dialog, 0, sizeof(dialog));
		dialog.id = -1;
		dialog.active = qfalse;
		dialog.currentPage = 0;
		dialog.numPages = 0;
	}
	
	DialogComponent(const dialog_t &d, int client) : dialog(d), clientNum(client) {}
};

// Equipment Component - Tracks equipped items and their stat modifiers
struct EquipmentComponent {
	equipment_stats_t totalStats;  // Sum of all equipped item stats
	qboolean hasEquipment[EQUIP_SLOT_MAX];  // Which slots have items
	
	EquipmentComponent() {
		memset(&totalStats, 0, sizeof(totalStats));
		memset(hasEquipment, 0, sizeof(hasEquipment));
		// Initialize stats to defaults
		totalStats.damage_multiplier = 1.0f;
		totalStats.rof_multiplier = 1.0f;
		totalStats.accuracy_bonus = 0.0f;
		totalStats.armor_bonus = 0;
		totalStats.health_bonus = 0;
		totalStats.model_override[0] = '\0';
		totalStats.skin_override[0] = '\0';
	}
};

#endif // defined(USE_ENTT) && defined(__cplusplus)

#endif // __G_ECS_MOD_COMPONENTS_H__

