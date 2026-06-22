/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Modular BSP sector merge — overlay sector brush collision on the base CM
without replacing CM_LoadMap (open-world walkable streaming).
===========================================================================
*/

#ifndef CM_STREAM_MERGE_H
#define CM_STREAM_MERGE_H

#include "q_shared.h"

void     CM_Stream_Merge_Init( void );
void     CM_Stream_Merge_ClearAll( void );
qboolean CM_Stream_MergeSector( int cellX, int cellY );
void     CM_Stream_UnmergeSector( int cellX, int cellY );
qboolean CM_Stream_IsSectorMerged( int cellX, int cellY );
int      CM_Stream_MergedCount( void );

#endif /* CM_STREAM_MERGE_H */
