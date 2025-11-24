/*
===========================================================================
Equipment System Implementation

Equipment slots and stat modifiers for weapons and armor.
===========================================================================
*/

#include "g_local.h"
#include "g_inventory.h"
#include "g_equipment.h"

static equipment_def_t equipment_defs[MAX_EQUIPMENT_DEFS];
static int num_equipment_defs = 0;

/*
================
G_Equipment_Init
Initialize equipment system
================
*/
void G_Equipment_Init( void ) {
	memset( equipment_defs, 0, sizeof( equipment_defs ) );
	num_equipment_defs = 0;
	
	// Load equipment definitions from file
	G_Equipment_LoadDefinitions( "inventory/equipment.dat" );
	
	G_Printf( "Equipment system initialized\n" );
}

/*
================
G_Equipment_Shutdown
Clean up equipment system
================
*/
void G_Equipment_Shutdown( void ) {
	memset( equipment_defs, 0, sizeof( equipment_defs ) );
	num_equipment_defs = 0;
}

/*
================
G_Equipment_RegisterDefinition
Register an equipment definition
================
*/
void G_Equipment_RegisterDefinition( int itemId, equipment_slot_t slot, 
                                     const equipment_stats_t *stats,
                                     const char *name, const char *description ) {
	equipment_def_t *def;
	
	if( num_equipment_defs >= MAX_EQUIPMENT_DEFS ) {
		G_Printf( "G_Equipment_RegisterDefinition: Too many equipment definitions\n" );
		return;
	}
	
	def = &equipment_defs[ num_equipment_defs ];
	
	def->itemId = itemId;
	def->slot = slot;
	def->active = qtrue;
	
	if( stats ) {
		def->stats = *stats;
	} else {
		memset( &def->stats, 0, sizeof( equipment_stats_t ) );
		def->stats.damage_multiplier = 1.0f;
		def->stats.rof_multiplier = 1.0f;
		def->stats.accuracy_bonus = 0.0f;
	}
	
	if( name ) {
		Q_strncpyz( def->name, name, sizeof( def->name ) );
	} else {
		Com_sprintf( def->name, sizeof( def->name ), "Equipment %d", itemId );
	}
	
	if( description ) {
		Q_strncpyz( def->description, description, sizeof( def->description ) );
	}
	
	num_equipment_defs++;
}

/*
================
G_Equipment_FindDefinition
Find equipment definition by item ID
================
*/
equipment_def_t *G_Equipment_FindDefinition( int itemId ) {
	int i;
	
	for( i = 0; i < num_equipment_defs; i++ ) {
		if( equipment_defs[ i ].itemId == itemId && equipment_defs[ i ].active ) {
			return &equipment_defs[ i ];
		}
	}
	
	return NULL;
}

/*
================
G_Equipment_LoadDefinitions
Load equipment definitions from file
================
*/
qboolean G_Equipment_LoadDefinitions( const char *filename ) {
	fileHandle_t f;
	int len;
	char *cnf, *cnf2;
	const char *t;
	int itemId, slot;
	equipment_stats_t stats;
	char name[MAX_ITEM_NAME];
	char description[MAX_ITEM_DESCRIPTION];
	
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if( len < 0 ) {
		// File doesn't exist, use defaults
		G_Printf( "G_Equipment_LoadDefinitions: File %s not found, using defaults\n", filename );
		
		// Register some default equipment
		// Example: Scope mod (+10% accuracy)
		stats.damage_multiplier = 1.0f;
		stats.rof_multiplier = 1.0f;
		stats.accuracy_bonus = 0.1f;
		stats.armor_bonus = 0;
		stats.health_bonus = 0;
		G_Equipment_RegisterDefinition( 1000, EQUIP_SLOT_WEAPON_MOD, &stats, 
		                                "Weapon Scope", "Increases accuracy by 10%" );
		
		// Example: Armor plate (+25 armor)
		stats.damage_multiplier = 1.0f;
		stats.rof_multiplier = 1.0f;
		stats.accuracy_bonus = 0.0f;
		stats.armor_bonus = 25;
		stats.health_bonus = 0;
		G_Equipment_RegisterDefinition( 1001, EQUIP_SLOT_ARMOR_VEST, &stats,
		                                "Armor Plate", "Adds 25 armor points" );
		
		return qtrue;
	}
	
	cnf = BG_Alloc( len + 1 );
	cnf2 = cnf;
	trap_FS_Read( cnf, len, f );
	*( cnf + len ) = '\0';
	trap_FS_FCloseFile( f );
	
	COM_BeginParseSession( filename );
	
	while( 1 ) {
		t = COM_Parse( (const char **)&cnf );
		if( !*t ) {
			break;
		}
		
		if( Q_strequal( t, "equipment" ) ) {
			// Parse equipment definition
			// Format: equipment <itemId> <slot> <damage_mult> <rof_mult> <accuracy_bonus> <armor_bonus> <health_bonus> "<name>" "<description>"
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			itemId = atoi( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			slot = atoi( t );
			if( slot < 0 || slot >= EQUIP_SLOT_MAX ) {
				continue;
			}
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			stats.damage_multiplier = atof( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			stats.rof_multiplier = atof( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			stats.accuracy_bonus = atof( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			stats.armor_bonus = atoi( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			stats.health_bonus = atoi( t );
			
			t = COM_Parse( (const char **)&cnf );
			if( !*t ) break;
			Q_strncpyz( name, t, sizeof( name ) );
			
			t = COM_Parse( (const char **)&cnf );
			if( *t ) {
				Q_strncpyz( description, t, sizeof( description ) );
			} else {
				description[ 0 ] = '\0';
			}
			
			G_Equipment_RegisterDefinition( itemId, (equipment_slot_t)slot, 
			                                &stats, name, description );
		}
	}
	
	BG_Free( cnf2 );
	
	G_Printf( "Loaded %d equipment definitions from %s\n", num_equipment_defs, filename );
	return qtrue;
}

