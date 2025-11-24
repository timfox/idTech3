/*
===========================================================================
Client-side Inventory UI Implementation

Overlay menu for inventory management.
===========================================================================
*/

#include "cg_local.h"
#include "cg_inventory.h"

static cg_inventory_ui_t cg_inventory_ui;

/*
================
CG_Inventory_Init
Initialize inventory UI
================
*/
void CG_Inventory_Init( void ) {
	memset( &cg_inventory_ui, 0, sizeof( cg_inventory_ui ) );
	cg_inventory_ui.active = qfalse;
	cg_inventory_ui.selectedItem = -1;
	cg_inventory_ui.selectedSlot = -1;
}

/*
================
CG_Inventory_Shutdown
Clean up inventory UI
================
*/
void CG_Inventory_Shutdown( void ) {
	cg_inventory_ui.active = qfalse;
}

/*
================
CG_Inventory_Toggle
Toggle inventory menu on/off
================
*/
void CG_Inventory_Toggle( void ) {
	cg_inventory_ui.active = !cg_inventory_ui.active;
	if( cg_inventory_ui.active ) {
		// Request inventory data from server
		trap_SendClientCommand( "inventory" );
	}
}

/*
================
CG_Inventory_IsActive
Check if inventory UI is active
================
*/
qboolean CG_Inventory_IsActive( void ) {
	return cg_inventory_ui.active;
}

/*
================
CG_Inventory_Update
Update inventory display data from server
================
*/
void CG_Inventory_Update( int numItems, const int itemIds[], const int quantities[], 
                          const char names[][MAX_ITEM_NAME],
                          const int equipmentSlots[], const int equipmentItemIds[] ) {
	int i;
	
	cg_inventory_ui.numItems = numItems;
	if( numItems > MAX_INVENTORY_DISPLAY_ITEMS ) {
		numItems = MAX_INVENTORY_DISPLAY_ITEMS;
	}
	
	for( i = 0; i < numItems; i++ ) {
		cg_inventory_ui.itemIds[ i ] = itemIds[ i ];
		cg_inventory_ui.itemQuantities[ i ] = quantities[ i ];
		if( names ) {
			Q_strncpyz( cg_inventory_ui.itemNames[ i ], names[ i ], MAX_ITEM_NAME );
		} else {
			Com_sprintf( cg_inventory_ui.itemNames[ i ], MAX_ITEM_NAME, "Item %d", itemIds[ i ] );
		}
	}
	
	for( i = 0; i < EQUIP_SLOT_MAX; i++ ) {
		cg_inventory_ui.equipmentSlots[ i ] = equipmentSlots[ i ];
		cg_inventory_ui.equipmentItemIds[ i ] = equipmentItemIds[ i ];
	}
}

/*
================
CG_Inventory_Draw
Draw the inventory overlay menu
================
*/
void CG_Inventory_Draw( void ) {
	int x, y, w, h;
	int itemX, itemY;
	int equipX, equipY;
	float color[4];
	float bgColor[4];
	char line[256];
	int i;
	int itemsPerRow = 8;
	int itemWidth = 70;
	int itemHeight = 70;
	int startX, startY;
	
	if( !cg_inventory_ui.active ) {
		return;
	}
	
	// Draw semi-transparent background (similar to scoreboard)
	bgColor[0] = 0.0f;
	bgColor[1] = 0.0f;
	bgColor[2] = 0.0f;
	bgColor[3] = 0.7f;
	
	w = 640;
	h = 480;
	x = ( 640 - w ) / 2;
	y = ( 480 - h ) / 2;
	
	CG_FillRect( x, y, w, h, bgColor );
	
	// Draw border
	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = 1.0f;
	CG_DrawRect( x, y, w, h, 2.0f, color );
	
	// Draw title
	CG_DrawStringExt( x + 20, y + 20, "INVENTORY", color, qfalse, qtrue,
	                  16, 16, 0 );
	
	// Draw items grid
	startX = x + 20;
	startY = y + 60;
	
	CG_DrawStringExt( startX, startY - 30, "Items:", color, qfalse, qtrue,
	                  16, 16, 0 );
	
	for( i = 0; i < cg_inventory_ui.numItems && i < MAX_INVENTORY_DISPLAY_ITEMS; i++ ) {
		itemX = startX + ( i % itemsPerRow ) * ( itemWidth + 10 );
		itemY = startY + ( i / itemsPerRow ) * ( itemHeight + 10 );
		
		// Draw item box
		if( cg_inventory_ui.selectedItem == i ) {
			// Highlight selected item
			float selColor[4] = { 1.0f, 1.0f, 0.0f, 0.5f };
			CG_FillRect( itemX, itemY, itemWidth, itemHeight, selColor );
		}
		
		CG_DrawRect( itemX, itemY, itemWidth, itemHeight, 1.0f, color );
		
		// Draw item name (truncated)
		Com_sprintf( line, sizeof( line ), "%s", cg_inventory_ui.itemNames[ i ] );
		if( CG_DrawStrlen( line ) > 8 ) {
			line[ 8 ] = '\0';
		}
		CG_DrawStringExt( itemX + 5, itemY + 5, line, color, qfalse, qtrue,
		                  8, 8, 0 );
		
		// Draw quantity
		if( cg_inventory_ui.itemQuantities[ i ] > 1 ) {
			Com_sprintf( line, sizeof( line ), "x%d", cg_inventory_ui.itemQuantities[ i ] );
			CG_DrawStringExt( itemX + itemWidth - 30, itemY + itemHeight - 15, line, color, qfalse, qtrue,
			                  8, 8, 0 );
		}
	}
	
	// Draw equipment slots
	equipX = x + 20;
	equipY = y + 300;
	
	CG_DrawStringExt( equipX, equipY - 30, "Equipment:", color, qfalse, qtrue,
	                  16, 16, 0 );
	
	const char *slotNames[] = {
		"Weapon Mod",
		"Helmet",
		"Vest",
		"Legs"
	};
	
	for( i = 0; i < EQUIP_SLOT_MAX; i++ ) {
		int slotY = equipY + i * 40;
		
		// Draw slot name
		CG_DrawStringExt( equipX, slotY, slotNames[ i ], color, qfalse, qtrue,
		                  8, 8, 0 );
		
		// Draw equipped item
		if( cg_inventory_ui.equipmentItemIds[ i ] >= 0 ) {
			Com_sprintf( line, sizeof( line ), "Item %d", cg_inventory_ui.equipmentItemIds[ i ] );
			CG_DrawStringExt( equipX + 120, slotY, line, color, qfalse, qtrue,
			                  8, 8, 0 );
		} else {
			CG_DrawStringExt( equipX + 120, slotY, "[Empty]", color, qfalse, qtrue,
			                  8, 8, 0 );
		}
	}
	
	// Draw crafting section
	if( cg_inventory_ui.showCrafting ) {
		int craftY = y + 400;
		CG_DrawStringExt( equipX, craftY, "Crafting:", color, qfalse, qtrue,
		                  16, 16, 0 );
		
		if( cg_inventory_ui.craftingItem1 >= 0 && cg_inventory_ui.craftingItem2 >= 0 ) {
			Com_sprintf( line, sizeof( line ), "Combine Item %d + Item %d", 
			             cg_inventory_ui.craftingItem1, cg_inventory_ui.craftingItem2 );
			CG_DrawStringExt( equipX, craftY + 30, line, color, qfalse, qtrue,
			                  8, 8, 0 );
		}
	}
	
	// Draw help text
	CG_DrawStringExt( x + 20, y + h - 40, "Press TAB to close | Number keys to select | ENTER to use/equip", 
	                  color, qfalse, qtrue, 8, 8, 0 );
}

/*
================
CG_Inventory_HandleInput
Handle keyboard input for inventory
================
*/
void CG_Inventory_HandleInput( int key ) {
	if( !cg_inventory_ui.active ) {
		return;
	}
	
	// Close inventory
	if( key == K_TAB || key == K_ESCAPE ) {
		cg_inventory_ui.active = qfalse;
		return;
	}
	
	// Number keys select items
	if( key >= K_1 && key <= K_9 ) {
		int itemIndex = key - K_1;
		if( itemIndex < cg_inventory_ui.numItems ) {
			cg_inventory_ui.selectedItem = itemIndex;
		}
		return;
	}
	
	// ENTER uses selected item
	if( key == K_ENTER || key == K_KP_ENTER ) {
		if( cg_inventory_ui.selectedItem >= 0 && 
		    cg_inventory_ui.selectedItem < cg_inventory_ui.numItems ) {
			int itemId = cg_inventory_ui.itemIds[ cg_inventory_ui.selectedItem ];
			// Send use command to server
			trap_SendClientCommand( va( "inventoryuse %d", itemId ) );
		}
		return;
	}
}

