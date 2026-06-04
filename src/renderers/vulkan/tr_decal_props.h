/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#ifndef TR_DECAL_PROPS_H
#define TR_DECAL_PROPS_H

#include "../common/tr_types.h"

void R_DecalProps_Init( void );
void R_DecalProps_Clear( void );
void R_DecalProps_ParseFromEntityString( const char *entityString );
void R_DecalProps_AddRefEntitiesToScene( int refdefTimeMs );
void RE_AddEngineDecalToScene( const engineDecalDesc_t *desc );

#endif /* TR_DECAL_PROPS_H */
