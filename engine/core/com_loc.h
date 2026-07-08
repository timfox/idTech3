/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

i18n string tables: loc/<language>.loc (key=value lines).
===========================================================================
*/

#ifndef COM_LOC_H
#define COM_LOC_H

#include "q_shared.h"

void Com_Loc_Init( void );
void Com_Loc_Reload( void );
void Com_Loc_Clear( void );
int Com_Loc_Lookup( const char *key, char *out, int outSize );

#endif /* COM_LOC_H */
