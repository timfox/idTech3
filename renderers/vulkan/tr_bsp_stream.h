/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Visual BSP sector overlay — append sector brush tops without replacing the
base world map (parallel to cm_stream_merge collision overlay).
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"

void     R_BspStream_Init( void );
void     RE_BspStream_ClearAll( void );
qboolean RE_BspStream_MergeSector( int cellX, int cellY, float sectorSize );
void     RE_BspStream_UnmergeSector( int cellX, int cellY );
void     R_BspStream_AddSurfaces( void );
