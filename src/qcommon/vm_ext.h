/*
===========================================================================

VM extension helpers

Lightweight helpers to negotiate optional engine features with VMs using
string keys via COM_TRAP_GETVALUE.

===========================================================================
*/

#ifndef VM_EXT_H
#define VM_EXT_H

#include "q_shared.h"

typedef struct
{
	char    *name;     // extension key string
	int     trapKey;   // trap enum value for this extension
	qboolean active;   // set to qtrue once the VM queries this key
} ext_trap_keys_t;

/*
================
VM_Ext_GetValue

Lookup an extension key in the given list and return its trap id as a string.
Marks the extension as active when found.
================
*/
static ID_INLINE qboolean VM_Ext_GetValue( ext_trap_keys_t *list, char *value, int valueSize, const char *key )
{
	int i;

	if ( !list ) {
		return qfalse;
	}

	for ( i = 0; list[ i ].name; i++ ) {
		if ( !Q_stricmp( key, list[ i ].name ) ) {
			Com_sprintf( value, valueSize, "%i", list[ i ].trapKey );
			list[ i ].active = qtrue;
			return qtrue;
		}
	}

	return qfalse;
}

/*
================
VM_Ext_ResetActive

Mark all extensions in the list as inactive.
================
*/
static ID_INLINE void VM_Ext_ResetActive( ext_trap_keys_t *list )
{
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0; list[ i ].name; i++ ) {
		list[ i ].active = qfalse;
	}
}

#ifndef TRAP_EXTENSIONS_LIST
#error "Missing TRAP_EXTENSIONS_LIST definition"
#endif

// Macros for the common "one list per module" pattern
#define VM_Ext_IsActive(x)      (TRAP_EXTENSIONS_LIST[(x)].active)
#define VM_Ext_GetKey(value,size,key) VM_Ext_GetValue( TRAP_EXTENSIONS_LIST, (value), (size), (key) )
#define VM_Ext_ResetAll()       VM_Ext_ResetActive( TRAP_EXTENSIONS_LIST )

#endif // VM_EXT_H


