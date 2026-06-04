/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#ifndef SV_ENGINE_DECALS_H
#define SV_ENGINE_DECALS_H

#include "../qcommon/q_shared.h"
#include "../qcommon/engine_decal_map.h"

void SV_EngineDecals_Init( void );
void SV_EngineDecals_Clear( void );
void SV_EngineDecals_LoadMap( const char *entityString );
void SV_EngineDecals_SpawnMapEntities( void );
int SV_EngineDecalShaderIndex( const char *shaderName );
int SV_EngineDecal_SpawnFromDef( const engineDecalMapDef_t *def );

#endif /* SV_ENGINE_DECALS_H */
