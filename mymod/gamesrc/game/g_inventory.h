/*
===========================================================================
Inventory System Header

A comprehensive inventory system with persistent storage, equipment slots,
and crafting capabilities.
===========================================================================
*/

#ifndef _G_INVENTORY_H
#define _G_INVENTORY_H

#include "g_inventory_types.h"
#include "g_local.h"

// Inventory item structure
typedef struct {
	int					itemId;				// Item ID (from inv.h constants or custom)
	int					quantity;			// Quantity of this item
	int					durability;			// Durability (optional, -1 for infinite)
	inventory_item_type_t type;				// Item type
	char				name[MAX_ITEM_NAME];
	char				description[MAX_ITEM_DESCRIPTION];
	char				icon[MAX_ITEM_ICON_PATH];
} inventory_item_t;

// Equipment item structure
typedef struct {
	int					itemId;				// Item ID
	equipment_slot_t	slot;				// Which slot this occupies
	equipment_stats_t	stats;				// Stat modifications
	char				name[MAX_ITEM_NAME];
	char				icon[MAX_ITEM_ICON_PATH];
} equipment_item_t;

// Player inventory structure
typedef struct {
	char				playerGuid[33];		// Player GUID for identification
	int					numItems;			// Number of items in inventory
	inventory_item_t	items[MAX_INVENTORY_ITEMS];
	equipment_item_t	equipment[EQUIP_SLOT_MAX];	// Equipped items
	qboolean			dirty;				// Needs saving
} inventory_t;

// Crafting recipe structure
typedef struct {
	int		input1;			// First input item ID
	int		input2;			// Second input item ID
	int		output;				// Output item ID
	int		outputQuantity;		// Quantity of output
	char	name[MAX_ITEM_NAME];
	qboolean active;			// Is recipe active/enabled
} craft_recipe_t;

#define MAX_CRAFT_RECIPES		128

// Inventory system functions
void G_Inventory_Init( void );
void G_Inventory_Shutdown( void );
qboolean G_Inventory_Load( int clientNum );
qboolean G_Inventory_Save( int clientNum );
qboolean G_Inventory_AddItem( int clientNum, int itemId, int quantity );
qboolean G_Inventory_RemoveItem( int clientNum, int itemId, int quantity );
qboolean G_Inventory_HasItem( int clientNum, int itemId, int quantity );
int G_Inventory_GetItemCount( int clientNum, int itemId );
inventory_t *G_Inventory_GetInventory( int clientNum );

// Equipment functions
qboolean G_Inventory_EquipItem( int clientNum, equipment_slot_t slot, int itemId );
qboolean G_Inventory_UnequipItem( int clientNum, equipment_slot_t slot );
equipment_item_t *G_Inventory_GetEquipment( int clientNum, equipment_slot_t slot );
equipment_stats_t G_Inventory_GetTotalEquipmentStats( int clientNum );

// Crafting functions
void G_Crafting_Init( void );
void G_Crafting_RegisterRecipe( int input1, int input2, int output, int outputQuantity, const char *name );
qboolean G_Crafting_CanCraft( int clientNum, int itemId1, int itemId2 );
qboolean G_Crafting_Craft( int clientNum, int itemId1, int itemId2 );
craft_recipe_t *G_Crafting_FindRecipe( int itemId1, int itemId2 );

#endif

