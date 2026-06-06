/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Open-world sector graph — grid CSR + multi-source k-hop reachability (CPU + optional GPU).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define SECTOR_GRAPH_WINDOW_MAX   64
#define SECTOR_GRAPH_MAX_NODES    ( SECTOR_GRAPH_WINDOW_MAX * SECTOR_GRAPH_WINDOW_MAX )
#define SECTOR_GRAPH_BITWORDS     ( ( SECTOR_GRAPH_MAX_NODES + 31 ) / 32 )

typedef struct sectorGraphGpuQuery_s {
	int        originCellX;
	int        originCellY;
	int        width;
	int        height;
	int        nodeCount;
	int        maxHops;
	int        diagonals;
	const int *sourceNodeIds;
	int        sourceCount;
	const int *rowPtr;
	const int *colIdx;
} sectorGraphGpuQuery_t;

typedef qboolean ( *sectorGraphGpuReach_f )( const sectorGraphGpuQuery_t *query,
	uint32_t *reachBits, int bitWords );

void     SectorGraph_Init( void );
void     SectorGraph_Shutdown( void );
void     SectorGraph_SetGpuReachFn( sectorGraphGpuReach_f fn );

qboolean SectorGraph_StreamReachEnabled( void );
qboolean SectorGraph_ComputeEnabled( void );
qboolean SectorGraph_VerifyEnabled( void );

void SectorGraph_UpdateReachability( const vec3_t viewOrigin, const vec3_t *extraOrigins,
	int extraOriginCount, float sectorSize, float unloadRadius, int maxHops );

qboolean SectorGraph_IsReachable( int cellX, int cellY );
qboolean SectorGraph_ReachTest( int srcX, int srcY, int dstX, int dstY, int maxHops );

void     SectorGraph_ReachTest_f( void );
void     SectorGraph_Status_f( void );

int  SectorGraph_GetNodeCount( void );
void SectorGraph_GetReachBits( const uint32_t **bits, int *bitWords );

#ifdef SECTOR_GRAPH_UNIT_TEST
void SectorGraph_ResetForTest( void );
void SectorGraph_EnableForTest( qboolean enable );
qboolean SectorGraph_RunBfsCpuSources( const int *sourceNodes, int sourceCount, int maxHops );
int  SectorGraph_CellToNode( int cellX, int cellY );
void SectorGraph_BuildWindowForTest( int centerCellX, int centerCellY, int radiusCells, qboolean diagonals );
#endif

#ifdef __cplusplus
}
#endif
