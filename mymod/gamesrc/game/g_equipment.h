/*
===========================================================================
Equipment System Header

Equipment slots and stat modifiers for weapons and armor.
===========================================================================
*/

#ifndef _G_EQUIPMENT_H
#define _G_EQUIPMENT_H

#include "g_inventory_types.h"

// Equipment item definitions (loaded from equipment.dat)
typedef struct {
	int					itemId;				// Item ID
	equipment_slot_t	slot;				// Which slot this fits
	equipment_stats_t	stats;				// Stat modifications
	char				name[MAX_ITEM_NAME];
	char				description[MAX_ITEM_DESCRIPTION];
	char				icon[MAX_ITEM_ICON_PATH];
	char				model[MAX_QPATH];
	char				skin[MAX_QPATH];
	qboolean			active;				// Is this equipment active/enabled
} equipment_def_t;

#define MAX_EQUIPMENT_DEFS	64

// Equipment system functions
void G_Equipment_Init( void );
void G_Equipment_Shutdown( void );
qboolean G_Equipment_LoadDefinitions( const char *filename );
equipment_def_t *G_Equipment_FindDefinition( int itemId );
void G_Equipment_RegisterDefinition( int itemId, equipment_slot_t slot, 
                                     const equipment_stats_t *stats,
                                     const char *name, const char *description );

#endif

