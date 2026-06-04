/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Engine-native billboard / flipbook / imposter map props and scene helpers.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void R_SpriteProps_Clear( void );
void R_SpriteProps_ParseFromEntityString( const char *entityString );
const char *R_MapProps_EntityString( void );
void R_SpriteProps_AddRefEntitiesToScene( int refdefTimeMs );
void R_SpriteProps_Init( void );
void RE_AddEngineSpriteToScene( const engineSpriteDesc_t *desc );
void RE_AddEngineSpriteToSceneAtTime( const engineSpriteDesc_t *desc, int refdefTimeMs );

#ifdef __cplusplus
}
#endif
