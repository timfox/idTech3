/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Server-side engine sprite shader table (configstrings) and spawn helpers.
Map misc_* entities register shader paths; clients resolve modelindex -> CS path.
===========================================================================
*/

#pragma once

#include "../qcommon/engine_sprite_map.h"

#ifdef __cplusplus
extern "C" {
#endif

void SV_EngineSprites_Init( void );
void SV_EngineSprites_Clear( void );
void SV_EngineSprites_LoadMap( const char *entityString );
void SV_EngineSprites_SpawnMapEntities( void );
int SV_EngineSpriteShaderIndex( const char *shaderName );
int SV_EngineSprite_SpawnFromDef( const engineSpriteMapDef_t *def );

#ifdef __cplusplus
}
#endif
