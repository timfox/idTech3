/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

i18n string tables: loc/<language>.loc (key=value lines).
Supports escaped/multiline values, language fallback, hash lookup.
===========================================================================
*/

#ifndef COM_LOC_H
#define COM_LOC_H

#include "q_shared.h"

#define COM_LOC_KEY_SIZE    64
#define COM_LOC_VALUE_SIZE  1024
#define COM_LOC_MAX_ENTRIES 4096

void Com_Loc_Init( void );
void Com_Loc_Reload( void );
void Com_Loc_Clear( void );
int Com_Loc_Lookup( const char *key, char *out, int outSize );
int Com_Loc_Count( void );
const char *Com_Loc_Language( void );
void Com_Loc_Status( void );

#endif /* COM_LOC_H */
