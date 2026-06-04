/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Sector BSP streaming v1 (broadphase chunks per grid cell).
===========================================================================
*/

#ifndef CM_STREAM_H
#define CM_STREAM_H

#include "q_shared.h"

void CM_Stream_Init( void );
qboolean CM_Stream_LoadSector( int cellX, int cellY );
void CM_Stream_UnloadSector( int cellX, int cellY );
qboolean CM_Stream_IsSectorLoaded( int cellX, int cellY );
void CM_Stream_PrefetchSectorPk3( int cellX, int cellY );

#endif /* CM_STREAM_H */
