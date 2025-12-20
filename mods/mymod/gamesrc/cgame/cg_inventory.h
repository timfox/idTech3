/*
===========================================================================
Client-side Inventory UI Header

Overlay menu for inventory management.
===========================================================================
*/

#ifndef _CG_INVENTORY_H
#define _CG_INVENTORY_H

#include "../common/q_shared.h"
#include "cg_public.h"

#define MAX_INVENTORY_DISPLAY_ITEMS	64
#define MAX_INVENTORY_DISPLAY_CHOICES	4
#define MAX_ITEM_NAME			64
#define EQUIP_SLOT_MAX			4

// Equipment slot types (must match server-side)
typedef enum {
	EQUIP_SLOT_NONE = -1,
	EQUIP_SLOT_WEAPON_MOD = 0,
	EQUIP_SLOT_ARMOR_HELMET,
	EQUIP_SLOT_ARMOR_VEST,
	EQUIP_SLOT_ARMOR_LEGS,
} equipment_slot_t;

typedef struct {
	qboolean	active;
	int			selectedItem;
	int			selectedSlot;
	int			numItems;
	int			itemIds[MAX_INVENTORY_DISPLAY_ITEMS];
	int			itemQuantities[MAX_INVENTORY_DISPLAY_ITEMS];
	char		itemNames[MAX_INVENTORY_DISPLAY_ITEMS][MAX_ITEM_NAME];
	int			equipmentSlots[EQUIP_SLOT_MAX];
	int			equipmentItemIds[EQUIP_SLOT_MAX];
	qboolean	showCrafting;
	int			craftingItem1;
	int			craftingItem2;
} cg_inventory_ui_t;

// Inventory UI functions
void CG_Inventory_Init( void );
void CG_Inventory_Shutdown( void );
void CG_Inventory_Toggle( void );
void CG_Inventory_Draw( void );
void CG_Inventory_HandleInput( int key );
qboolean CG_Inventory_IsActive( void );
void CG_Inventory_Update( int numItems, const int itemIds[], const int quantities[], 
                          const char names[][MAX_ITEM_NAME],
                          const int equipmentSlots[], const int equipmentItemIds[] );

#endif

