/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Sector BSP streaming v1 (broadphase chunks per grid cell).
===========================================================================
*/

#ifndef CM_STREAM_H
#define CM_STREAM_H

#include "q_shared.h"

typedef void ( *cm_stream_prefetch_f )( const char *localName, const char *remoteURL );

void CM_Stream_Init( void );
void CM_Stream_SetPrefetchHandler( cm_stream_prefetch_f handler );
void CM_Stream_WorldToCell( const vec3_t origin, float sectorSize, int *cellX, int *cellY );
qboolean CM_Stream_LoadSector( int cellX, int cellY );
void CM_Stream_UnloadSector( int cellX, int cellY );
qboolean CM_Stream_IsSectorLoaded( int cellX, int cellY );
void CM_Stream_PrefetchSectorPk3( int cellX, int cellY );
void CM_Stream_UpdateView( const vec3_t viewOrigin, float radius, float sectorSize, qboolean loadCollision );
void CM_Stream_BuildLoadedList( char *buf, int bufSize );

#endif /* CM_STREAM_H */
