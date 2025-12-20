/*
===========================================================================
Inventory System Type Definitions

Type definitions for the inventory system that don't depend on g_local.h
===========================================================================
*/

#ifndef _G_INVENTORY_TYPES_H
#define _G_INVENTORY_TYPES_H

#include "../common/q_shared.h"

#define MAX_INVENTORY_ITEMS		256
#define MAX_ITEM_NAME			64
#define MAX_ITEM_DESCRIPTION	256
#define MAX_ITEM_ICON_PATH		128

// Item types
typedef enum {
	INV_TYPE_WEAPON, INV_TYPE_AMMO, INV_TYPE_ARMOR, INV_TYPE_HEALTH,
	INV_TYPE_POWERUP, INV_TYPE_HOLDABLE, INV_TYPE_EQUIPMENT, INV_TYPE_CUSTOM,
	INV_TYPE_MAX
} inventory_item_type_t;

// Equipment slot types
typedef enum {
	EQUIP_SLOT_NONE = -1,
	EQUIP_SLOT_WEAPON_MOD = 0,		// Weapon modifications (scope, silencer, etc.)
	EQUIP_SLOT_ARMOR_HELMET,		// Helmet armor piece
	EQUIP_SLOT_ARMOR_VEST,			// Vest/chest armor piece
	EQUIP_SLOT_ARMOR_LEGS,			// Leg armor piece
	EQUIP_SLOT_MAX
} equipment_slot_t;

// Equipment stat modifiers
typedef struct {
	float	damage_multiplier;		// Damage multiplier (1.0 = no change, 1.2 = +20%)
	float	rof_multiplier;			// Rate of fire multiplier
	float	accuracy_bonus;			// Accuracy bonus (0.0 = no change, 0.1 = +10%)
	int		armor_bonus;			// Armor bonus points
	int		health_bonus;			// Health bonus points
	char	model_override[MAX_QPATH];	// Model override path
	char	skin_override[MAX_QPATH];	// Skin override path
} equipment_stats_t;

#endif

