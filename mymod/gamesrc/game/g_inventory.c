/*
===========================================================================
Inventory System Implementation

A comprehensive inventory system with persistent storage, equipment slots,
and crafting capabilities.
===========================================================================
*/

#include "g_local.h"
#include "g_inventory.h"
#include "g_equipment.h"
#include "inv.h"

#define MAX_INVENTORIES		MAX_CLIENTS

static inventory_t inventories[MAX_INVENTORIES];
static craft_recipe_t craft_recipes[MAX_CRAFT_RECIPES];
static int num_craft_recipes = 0;

/*
================
G_Inventory_Init
Initialize the inventory system
================
*/
void G_Inventory_Init( void ) {
	int i;
	
	memset( inventories, 0, sizeof( inventories ) );
	memset( craft_recipes, 0, sizeof( craft_recipes ) );
	num_craft_recipes = 0;
	
	for( i = 0; i < MAX_INVENTORIES; i++ ) {
		inventories[ i ].playerGuid[ 0 ] = '\0';
		inventories[ i ].numItems = 0;
		inventories[ i ].dirty = qfalse;
		
		// Initialize equipment slots
		for( int j = 0; j < EQUIP_SLOT_MAX; j++ ) {
			inventories[ i ].equipment[ j ].itemId = -1;
			inventories[ i ].equipment[ j ].slot = EQUIP_SLOT_NONE;
		}
	}
	
	G_Printf( "Inventory system initialized\n" );
}

/*
================
G_Inventory_Shutdown
Clean up inventory system
================
*/
void G_Inventory_Shutdown( void ) {
	int i;
	
	// Save all dirty inventories
	for( i = 0; i < MAX_INVENTORIES; i++ ) {
		if( inventories[ i ].dirty && inventories[ i ].playerGuid[ 0 ] != '\0' ) {
			G_Inventory_Save( i );
		}
	}
	
	G_Printf( "Inventory system shutdown\n" );
}

/*
================
G_Inventory_GetInventory
Get inventory for a client
================
*/
inventory_t *G_Inventory_GetInventory( int clientNum ) {
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return NULL;
	}
	
	return &inventories[ clientNum ];
}

/*
================
G_Inventory_FindItemSlot
Find slot containing an item, or first empty slot
================
*/
static int G_Inventory_FindItemSlot( inventory_t *inv, int itemId, qboolean findEmpty ) {
	int i;
	
	for( i = 0; i < inv->numItems && i < MAX_INVENTORY_ITEMS; i++ ) {
		if( findEmpty ) {
			if( inv->items[ i ].itemId == -1 ) {
				return i;
			}
		} else {
			if( inv->items[ i ].itemId == itemId ) {
				return i;
			}
		}
	}
	
	if( findEmpty && inv->numItems < MAX_INVENTORY_ITEMS ) {
		return inv->numItems;
	}
	
	return -1;
}

/*
================
G_Inventory_AddItem
Add an item to a player's inventory
================
*/
qboolean G_Inventory_AddItem( int clientNum, int itemId, int quantity ) {
	inventory_t *inv;
	int slot;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	if( quantity <= 0 ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	
	// Try to find existing item slot
	slot = G_Inventory_FindItemSlot( inv, itemId, qfalse );
	
	if( slot >= 0 ) {
		// Add to existing stack
		inv->items[ slot ].quantity += quantity;
		inv->dirty = qtrue;
		return qtrue;
	}
	
	// Find empty slot
	slot = G_Inventory_FindItemSlot( inv, -1, qtrue );
	if( slot < 0 ) {
		G_Printf( "G_Inventory_AddItem: Inventory full for client %d\n", clientNum );
		return qfalse;
	}
	
	// Add new item
	inv->items[ slot ].itemId = itemId;
	inv->items[ slot ].quantity = quantity;
	inv->items[ slot ].durability = -1; // Infinite durability by default
	inv->items[ slot ].type = INV_TYPE_CUSTOM; // Default type
	
	// Set default name based on item ID
	Com_sprintf( inv->items[ slot ].name, sizeof( inv->items[ slot ].name ), 
	             "Item %d", itemId );
	
	if( slot >= inv->numItems ) {
		inv->numItems = slot + 1;
	}
	
	inv->dirty = qtrue;
	return qtrue;
}

/*
================
G_Inventory_RemoveItem
Remove an item from a player's inventory
================
*/
qboolean G_Inventory_RemoveItem( int clientNum, int itemId, int quantity ) {
	inventory_t *inv;
	int slot;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	if( quantity <= 0 ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	
	slot = G_Inventory_FindItemSlot( inv, itemId, qfalse );
	if( slot < 0 ) {
		return qfalse;
	}
	
	if( inv->items[ slot ].quantity < quantity ) {
		return qfalse;
	}
	
	inv->items[ slot ].quantity -= quantity;
	
	if( inv->items[ slot ].quantity <= 0 ) {
		// Remove item completely
		memset( &inv->items[ slot ], 0, sizeof( inventory_item_t ) );
		inv->items[ slot ].itemId = -1;
		
		// Compact inventory
		int i;
		for( i = slot; i < inv->numItems - 1; i++ ) {
			inv->items[ i ] = inv->items[ i + 1 ];
		}
		inv->numItems--;
	}
	
	inv->dirty = qtrue;
	return qtrue;
}

/*
================
G_Inventory_HasItem
Check if player has an item with at least the specified quantity
================
*/
qboolean G_Inventory_HasItem( int clientNum, int itemId, int quantity ) {
	inventory_t *inv;
	int slot;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	
	slot = G_Inventory_FindItemSlot( inv, itemId, qfalse );
	if( slot < 0 ) {
		return qfalse;
	}
	
	return ( inv->items[ slot ].quantity >= quantity );
}

/*
================
G_Inventory_GetItemCount
Get the quantity of an item in player's inventory
================
*/
int G_Inventory_GetItemCount( int clientNum, int itemId ) {
	inventory_t *inv;
	int slot;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return 0;
	}
	
	inv = &inventories[ clientNum ];
	
	slot = G_Inventory_FindItemSlot( inv, itemId, qfalse );
	if( slot < 0 ) {
		return 0;
	}
	
	return inv->items[ slot ].quantity;
}

/*
================
G_Inventory_EquipItem
Equip an item to an equipment slot
================
*/
qboolean G_Inventory_EquipItem( int clientNum, equipment_slot_t slot, int itemId ) {
	inventory_t *inv;
	equipment_item_t *equip;
	equipment_def_t *def;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	if( slot < 0 || slot >= EQUIP_SLOT_MAX ) {
		return qfalse;
	}
	
	if( !G_Inventory_HasItem( clientNum, itemId, 1 ) ) {
		return qfalse;
	}
	
	// Check if this is a valid equipment item
	def = G_Equipment_FindDefinition( itemId );
	if( !def ) {
		return qfalse;
	}
	
	// Check if slot matches
	if( def->slot != slot ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	equip = &inv->equipment[ slot ];
	
	// Unequip current item if any
	if( equip->itemId >= 0 ) {
		G_Inventory_UnequipItem( clientNum, slot );
	}
	
	// Equip new item
	equip->itemId = itemId;
	equip->slot = slot;
	equip->stats = def->stats;
	Q_strncpyz( equip->name, def->name, sizeof( equip->name ) );
	
	inv->dirty = qtrue;
	return qtrue;
}

/*
================
G_Inventory_UnequipItem
Unequip an item from an equipment slot
================
*/
qboolean G_Inventory_UnequipItem( int clientNum, equipment_slot_t slot ) {
	inventory_t *inv;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	if( slot < 0 || slot >= EQUIP_SLOT_MAX ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	
	if( inv->equipment[ slot ].itemId < 0 ) {
		return qfalse;
	}
	
	// Clear equipment slot
	memset( &inv->equipment[ slot ], 0, sizeof( equipment_item_t ) );
	inv->equipment[ slot ].itemId = -1;
	inv->equipment[ slot ].slot = EQUIP_SLOT_NONE;
	
	inv->dirty = qtrue;
	return qtrue;
}

/*
================
G_Inventory_GetEquipment
Get equipment from a specific slot
================
*/
equipment_item_t *G_Inventory_GetEquipment( int clientNum, equipment_slot_t slot ) {
	inventory_t *inv;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return NULL;
	}
	
	if( slot < 0 || slot >= EQUIP_SLOT_MAX ) {
		return NULL;
	}
	
	inv = &inventories[ clientNum ];
	
	if( inv->equipment[ slot ].itemId < 0 ) {
		return NULL;
	}
	
	return &inv->equipment[ slot ];
}

/*
================
G_Inventory_GetTotalEquipmentStats
Calculate total stat modifiers from all equipped items
================
*/
equipment_stats_t G_Inventory_GetTotalEquipmentStats( int clientNum ) {
	inventory_t *inv;
	equipment_stats_t total;
	int i;
	
	memset( &total, 0, sizeof( equipment_stats_t ) );
	total.damage_multiplier = 1.0f;
	total.rof_multiplier = 1.0f;
	total.accuracy_bonus = 0.0f;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return total;
	}
	
	inv = &inventories[ clientNum ];
	
	// Sum up all equipment stats
	for( i = 0; i < EQUIP_SLOT_MAX; i++ ) {
		if( inv->equipment[ i ].itemId >= 0 ) {
			equipment_stats_t *stats = &inv->equipment[ i ].stats;
			
			// Multiply multipliers
			total.damage_multiplier *= stats->damage_multiplier;
			total.rof_multiplier *= stats->rof_multiplier;
			
			// Add bonuses
			total.accuracy_bonus += stats->accuracy_bonus;
			total.armor_bonus += stats->armor_bonus;
			total.health_bonus += stats->health_bonus;
		}
	}
	
	return total;
}

/*
================
G_Crafting_Init
Initialize crafting system
================
*/
void G_Crafting_Init( void ) {
	num_craft_recipes = 0;
	memset( craft_recipes, 0, sizeof( craft_recipes ) );
	
	G_Printf( "Crafting system initialized\n" );
}

/*
================
G_Crafting_RegisterRecipe
Register a crafting recipe
================
*/
void G_Crafting_RegisterRecipe( int input1, int input2, int output, int outputQuantity, const char *name ) {
	craft_recipe_t *recipe;
	
	if( num_craft_recipes >= MAX_CRAFT_RECIPES ) {
		G_Printf( "G_Crafting_RegisterRecipe: Too many recipes\n" );
		return;
	}
	
	recipe = &craft_recipes[ num_craft_recipes ];
	
	recipe->input1 = input1;
	recipe->input2 = input2;
	recipe->output = output;
	recipe->outputQuantity = outputQuantity;
	recipe->active = qtrue;
	
	if( name ) {
		Q_strncpyz( recipe->name, name, sizeof( recipe->name ) );
	} else {
		Com_sprintf( recipe->name, sizeof( recipe->name ), 
		             "Recipe %d+%d", input1, input2 );
	}
	
	num_craft_recipes++;
}

/*
================
G_Crafting_FindRecipe
Find a recipe matching two input items
================
*/
craft_recipe_t *G_Crafting_FindRecipe( int itemId1, int itemId2 ) {
	int i;
	
	for( i = 0; i < num_craft_recipes; i++ ) {
		craft_recipe_t *recipe = &craft_recipes[ i ];
		
		if( !recipe->active ) {
			continue;
		}
		
		// Check both orderings
		if( ( recipe->input1 == itemId1 && recipe->input2 == itemId2 ) ||
		    ( recipe->input1 == itemId2 && recipe->input2 == itemId1 ) ) {
			return recipe;
		}
	}
	
	return NULL;
}

/*
================
G_Crafting_CanCraft
Check if player can craft with two items
================
*/
qboolean G_Crafting_CanCraft( int clientNum, int itemId1, int itemId2 ) {
	craft_recipe_t *recipe;
	
	if( !G_Inventory_HasItem( clientNum, itemId1, 1 ) ) {
		return qfalse;
	}
	
	if( !G_Inventory_HasItem( clientNum, itemId2, 1 ) ) {
		return qfalse;
	}
	
	recipe = G_Crafting_FindRecipe( itemId1, itemId2 );
	return ( recipe != NULL );
}

/*
================
G_Crafting_Craft
Craft items using a recipe
================
*/
qboolean G_Crafting_Craft( int clientNum, int itemId1, int itemId2 ) {
	craft_recipe_t *recipe;
	
	recipe = G_Crafting_FindRecipe( itemId1, itemId2 );
	if( !recipe ) {
		return qfalse;
	}
	
	if( !G_Inventory_HasItem( clientNum, itemId1, 1 ) ||
	    !G_Inventory_HasItem( clientNum, itemId2, 1 ) ) {
		return qfalse;
	}
	
	// Remove input items
	if( !G_Inventory_RemoveItem( clientNum, itemId1, 1 ) ||
	    !G_Inventory_RemoveItem( clientNum, itemId2, 1 ) ) {
		return qfalse;
	}
	
	// Add output item
	if( !G_Inventory_AddItem( clientNum, recipe->output, recipe->outputQuantity ) ) {
		// Failed to add output, restore inputs
		G_Inventory_AddItem( clientNum, itemId1, 1 );
		G_Inventory_AddItem( clientNum, itemId2, 1 );
		return qfalse;
	}
	
	return qtrue;
}

/*
================
G_Inventory_GetSavePath
Get the file path for saving/loading inventory
================
*/
static void G_Inventory_GetSavePath( int clientNum, char *path, int pathSize ) {
	gclient_t *client;
	char guid[33];
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		path[0] = '\0';
		return;
	}
	
	client = &level.clients[ clientNum ];
	
	// Use GUID if available, otherwise use netname
	if( client->pers.guid[0] != '\0' ) {
		Q_strncpyz( guid, client->pers.guid, sizeof( guid ) );
		// Sanitize GUID for filename
		for( int i = 0; i < 32; i++ ) {
			if( guid[i] == '\0' ) break;
			if( guid[i] == '/' || guid[i] == '\\' || guid[i] == ':' ) {
				guid[i] = '_';
			}
		}
		Com_sprintf( path, pathSize, "inventory/%s.inv", guid );
	} else {
		// Fallback to netname
		char safeName[MAX_NETNAME];
		Q_strncpyz( safeName, client->pers.netname, sizeof( safeName ) );
		// Sanitize name for filename
		for( int i = 0; safeName[i] != '\0'; i++ ) {
			if( safeName[i] == '/' || safeName[i] == '\\' || safeName[i] == ':' ) {
				safeName[i] = '_';
			}
		}
		Com_sprintf( path, pathSize, "inventory/%s.inv", safeName );
	}
}

/*
================
G_Inventory_Load
Load inventory from file
================
*/
qboolean G_Inventory_Load( int clientNum ) {
	inventory_t *inv;
	fileHandle_t f;
	int len;
	char *cnf, *cnf2;
	const char *t;
	char path[MAX_QPATH];
	int itemCount, equipCount;
	int i;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	
	// Get save file path
	G_Inventory_GetSavePath( clientNum, path, sizeof( path ) );
	
	// Try to open file
	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if( len < 0 ) {
		// File doesn't exist, start with empty inventory
		return qtrue;
	}
	
	// Read file
	cnf = BG_Alloc( len + 1 );
	cnf2 = cnf;
	trap_FS_Read( cnf, len, f );
	*( cnf + len ) = '\0';
	trap_FS_FCloseFile( f );
	
	// Parse inventory file
	COM_BeginParseSession( path );
	
	// Read version (for future compatibility)
	t = COM_Parse( (const char **)&cnf );
	if( Q_strequal( t, "version" ) ) {
		t = COM_Parse( (const char **)&cnf );
		// Version number, ignore for now
		t = COM_Parse( (const char **)&cnf );
	}
	
	// Read GUID
	if( Q_strequal( t, "guid" ) ) {
		t = COM_Parse( (const char **)&cnf );
		Q_strncpyz( inv->playerGuid, t, sizeof( inv->playerGuid ) );
		t = COM_Parse( (const char **)&cnf );
	}
	
	// Read items
	if( Q_strequal( t, "items" ) ) {
		t = COM_Parse( (const char **)&cnf );
		itemCount = atoi( t );
		inv->numItems = 0;
		
		for( i = 0; i < itemCount && i < MAX_INVENTORY_ITEMS; i++ ) {
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			
			inv->items[ i ].itemId = atoi( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			inv->items[ i ].quantity = atoi( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( *t ) {
				inv->items[ i ].durability = atoi( t );
			} else {
				inv->items[ i ].durability = -1;
			}
			
			inv->numItems++;
		}
		t = COM_Parse( (const char **)&cnf );
	}
	
	// Read equipment
	if( Q_strequal( t, "equipment" ) ) {
		t = COM_Parse( (const char **)&cnf );
		equipCount = atoi( t );
		
		for( i = 0; i < equipCount && i < EQUIP_SLOT_MAX; i++ ) {
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			
			int slot = atoi( t );
			if( slot < 0 || slot >= EQUIP_SLOT_MAX ) {
				continue;
			}
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			inv->equipment[ slot ].itemId = atoi( t );
			inv->equipment[ slot ].slot = slot;
			
			// Read stat modifiers
			t = COM_Parse( (const char **)&cnf );
			if( *t ) inv->equipment[ slot ].stats.damage_multiplier = atof( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( *t ) inv->equipment[ slot ].stats.rof_multiplier = atof( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( *t ) inv->equipment[ slot ].stats.accuracy_bonus = atof( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( *t ) inv->equipment[ slot ].stats.armor_bonus = atoi( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( *t ) inv->equipment[ slot ].stats.health_bonus = atoi( t );
		}
	}
	
	BG_Free( cnf2 );
	inv->dirty = qfalse;
	
	return qtrue;
}

/*
================
G_Inventory_Save
Save inventory to file
================
*/
qboolean G_Inventory_Save( int clientNum ) {
	inventory_t *inv;
	fileHandle_t f;
	char path[MAX_QPATH];
	char line[MAX_STRING_CHARS];
	int i;
	
	if( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return qfalse;
	}
	
	inv = &inventories[ clientNum ];
	
	// Don't save if inventory is empty and not dirty
	if( !inv->dirty && inv->numItems == 0 ) {
		return qtrue;
	}
	
	// Get save file path
	G_Inventory_GetSavePath( clientNum, path, sizeof( path ) );
	
	// Open file for writing
	if( trap_FS_FOpenFile( path, &f, FS_WRITE ) < 0 ) {
		G_Printf( "G_Inventory_Save: Could not open file %s\n", path );
		return qfalse;
	}
	
	// Write version
	Com_sprintf( line, sizeof( line ), "version 1\n" );
	trap_FS_Write( line, strlen( line ), f );
	
	// Write GUID
	Com_sprintf( line, sizeof( line ), "guid %s\n", inv->playerGuid );
	trap_FS_Write( line, strlen( line ), f );
	
	// Write items
	Com_sprintf( line, sizeof( line ), "items %d\n", inv->numItems );
	trap_FS_Write( line, strlen( line ), f );
	
	for( i = 0; i < inv->numItems; i++ ) {
		if( inv->items[ i ].itemId >= 0 ) {
			Com_sprintf( line, sizeof( line ), "%d %d %d\n", 
			             inv->items[ i ].itemId,
			             inv->items[ i ].quantity,
			             inv->items[ i ].durability );
			trap_FS_Write( line, strlen( line ), f );
		}
	}
	
	// Write equipment
	int equipCount = 0;
	for( i = 0; i < EQUIP_SLOT_MAX; i++ ) {
		if( inv->equipment[ i ].itemId >= 0 ) {
			equipCount++;
		}
	}
	
	Com_sprintf( line, sizeof( line ), "equipment %d\n", equipCount );
	trap_FS_Write( line, strlen( line ), f );
	
	for( i = 0; i < EQUIP_SLOT_MAX; i++ ) {
		if( inv->equipment[ i ].itemId >= 0 ) {
			equipment_item_t *equip = &inv->equipment[ i ];
			Com_sprintf( line, sizeof( line ), "%d %d %.2f %.2f %.2f %d %d\n",
			             equip->slot,
			             equip->itemId,
			             equip->stats.damage_multiplier,
			             equip->stats.rof_multiplier,
			             equip->stats.accuracy_bonus,
			             equip->stats.armor_bonus,
			             equip->stats.health_bonus );
			trap_FS_Write( line, strlen( line ), f );
		}
	}
	
	trap_FS_FCloseFile( f );
	inv->dirty = qfalse;
	
	return qtrue;
}

