/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client bridge: draw networked engine sprite entities from snapshots without
cgame billboard code. See cl_engine_sprites.c for entityState field contract.
===========================================================================
*/

#pragma once

#include "../renderers/common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void CL_EngineSprites_Init( void );
void CL_EngineSprites_AddFromSnapshot( void );
void CL_EngineSprite_AddLocal( const engineSpriteDesc_t *desc );
void CL_SpriteSpawn_f( void );

#ifdef __cplusplus
}
#endif
