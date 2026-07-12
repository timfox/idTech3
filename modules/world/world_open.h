/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Open-world coordinator: modular BSP sector streaming, per-chunk nav tiles,
and billboard scatter residency (id Tech 8-style infinite walkable worlds).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define WORLD_OPEN_SECTOR_MAX 1024

typedef enum {
	WO_LAYER_COLLISION = 0,
	WO_LAYER_NAV,
	WO_LAYER_SPRITES
} worldOpenLayer_t;

typedef uint32_t worldOpenLayerMask_t;

#define WO_LAYER_MASK_COLLISION  ( 1u << WO_LAYER_COLLISION )
#define WO_LAYER_MASK_NAV        ( 1u << WO_LAYER_NAV )
#define WO_LAYER_MASK_SPRITES    ( 1u << WO_LAYER_SPRITES )
#define WO_LAYER_MASK_ALL        ( WO_LAYER_MASK_COLLISION | WO_LAYER_MASK_NAV | WO_LAYER_MASK_SPRITES )

typedef struct worldOpenSector_s {
	qboolean active;
	int      cellX;
	int      cellY;
	qboolean collision;
	qboolean nav;
	qboolean sprites;
} worldOpenSector_t;

typedef qboolean ( *worldOpenSectorLoad_f )( int cellX, int cellY, worldOpenLayer_t layer );
typedef void ( *worldOpenSectorUnload_f )( int cellX, int cellY, worldOpenLayer_t layer );

void     WorldOpen_Init( void );
void     WorldOpen_Shutdown( void );
void     WorldOpen_SetSectorLoad( worldOpenSectorLoad_f fn );
void     WorldOpen_SetSectorUnload( worldOpenSectorUnload_f fn );

void     WorldOpen_Enable( qboolean enable );
qboolean WorldOpen_IsEnabled( void );

void     WorldOpen_UpdateView( const vec3_t viewOrigin, float radius );
qboolean WorldOpen_LoadSector( int cellX, int cellY, worldOpenLayerMask_t layerMask );
void     WorldOpen_UnloadSector( int cellX, int cellY );
void     WorldOpen_UnloadSectorLayers( int cellX, int cellY, worldOpenLayerMask_t layerMask );

int      WorldOpen_GetSectorCount( void );
const worldOpenSector_t *WorldOpen_GetSector( int index );
int      WorldOpen_FindSector( int cellX, int cellY );

void     WorldOpen_List( void );
void     WorldOpen_Status( void );

#ifdef __cplusplus
}
#endif
