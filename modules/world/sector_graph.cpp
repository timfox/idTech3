/* C++20 migration: extern "C" API boundary preserved. */
extern "C" {
/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/cm_stream.h"
#include "world_open.h"
#include "sector_graph.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	int originCellX;
	int originCellY;
	int width;
	int height;
	int nodeCount;
	int rowPtr[SECTOR_GRAPH_MAX_NODES + 1];
	int colIdx[SECTOR_GRAPH_MAX_NODES * 8];
	int edgeCount;
} sectorGraphWindow_t;

static cvar_t *r_graphStreamReach;
static cvar_t *r_graphStreamHops;
static cvar_t *r_graphSectorDiagonals;
static cvar_t *r_graphBlockUnloaded;
static cvar_t *r_graphCompute;
static cvar_t *r_graphStreamVerify;
static cvar_t *r_graphNavInfluence;

static sectorGraphWindow_t s_window;
static uint32_t s_reachBits[SECTOR_GRAPH_BITWORDS];
static uint32_t s_gpuReachBits[SECTOR_GRAPH_BITWORDS];
static int8_t s_hopDist[SECTOR_GRAPH_MAX_NODES];
static qboolean s_reachValid;
static int s_lastMaxHops;
static sectorGraphGpuReach_f s_gpuReachFn;
static qboolean s_loggedEnable;
static int s_cacheCenterX;
static int s_cacheCenterY;
static int s_cacheRadiusCells;
static int s_cacheMaxHops;
static unsigned s_cacheOriginHash;
static qboolean s_cacheDiagonals;
#ifdef SECTOR_GRAPH_UNIT_TEST
static qboolean s_testForceReach;
#endif

static unsigned SG_HashExtraOrigins( const vec3_t *origins, int originCount, float sectorSize )
{
	unsigned hash = 0;
	int i;

	for ( i = 0; i < originCount; i++ ) {
		int cx, cy;
		if ( !origins ) {
			break;
		}
		CM_Stream_WorldToCell( origins[i], sectorSize, &cx, &cy );
		hash = hash * 31u + (unsigned)( cx + 8192 );
		hash = hash * 31u + (unsigned)( cy + 8192 );
	}
	return hash;
}

static qboolean SG_CacheHit( int centerX, int centerY, int radiusCells, int maxHops,
	qboolean diagonals, unsigned originHash )
{
	if ( !s_reachValid ) {
		return qfalse;
	}
	if ( SectorGraph_ComputeEnabled() || SectorGraph_VerifyEnabled() ) {
		return qfalse;
	}
	return s_cacheCenterX == centerX && s_cacheCenterY == centerY &&
		s_cacheRadiusCells == radiusCells && s_cacheMaxHops == maxHops &&
		s_cacheDiagonals == diagonals && s_cacheOriginHash == originHash;
}

static void SG_CacheStore( int centerX, int centerY, int radiusCells, int maxHops,
	qboolean diagonals, unsigned originHash )
{
	s_cacheCenterX = centerX;
	s_cacheCenterY = centerY;
	s_cacheRadiusCells = radiusCells;
	s_cacheMaxHops = maxHops;
	s_cacheDiagonals = diagonals;
	s_cacheOriginHash = originHash;
}

static int SG_NodeId( int cellX, int cellY )
{
	int lx = cellX - s_window.originCellX;
	int ly = cellY - s_window.originCellY;

	if ( lx < 0 || ly < 0 || lx >= s_window.width || ly >= s_window.height ) {
		return -1;
	}
	return ly * s_window.width + lx;
}

static void SG_CellFromNode( int nodeId, int *cellX, int *cellY )
{
	if ( cellX ) {
		*cellX = s_window.originCellX + ( nodeId % s_window.width );
	}
	if ( cellY ) {
		*cellY = s_window.originCellY + ( nodeId / s_window.width );
	}
}

static qboolean SG_IsLoadedCell( int cellX, int cellY )
{
	int i, n;
	const worldOpenSector_t *sec;

	if ( !r_graphBlockUnloaded || !r_graphBlockUnloaded->integer ) {
		return qtrue;
	}
	n = WorldOpen_GetSectorCount();
	for ( i = 0; i < n; i++ ) {
		sec = WorldOpen_GetSector( i );
		if ( !sec || !sec->active ) {
			continue;
		}
		if ( sec->cellX == cellX && sec->cellY == cellY ) {
			return qtrue;
		}
	}
	if ( Cvar_VariableIntegerValue( "cm_stream" ) ) {
		return CM_Stream_IsSectorLoaded( cellX, cellY );
	}
	return qfalse;
}

static void SG_ClearReach( void )
{
	Com_Memset( s_reachBits, 0, sizeof( s_reachBits ) );
	Com_Memset( s_hopDist, -1, sizeof( s_hopDist ) );
	s_reachValid = qfalse;
}

static void SG_SetReachNode( int nodeId )
{
	if ( nodeId >= 0 && nodeId < s_window.nodeCount ) {
		s_reachBits[nodeId >> 5] |= ( 1u << ( nodeId & 31 ) );
	}
}

static qboolean SG_GetReachNode( int nodeId )
{
	if ( nodeId < 0 || nodeId >= s_window.nodeCount ) {
		return qfalse;
	}
	return ( s_reachBits[nodeId >> 5] & ( 1u << ( nodeId & 31 ) ) ) != 0;
}

static void SG_BuildWindow( int centerCellX, int centerCellY, int radiusCells, qboolean diagonals )
{
	static const int card[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	static const int diag[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
	int minX, maxX, minY, maxY;
	int w, h;
	int x, y;
	int nodeId;
	int n;

	minX = centerCellX - radiusCells;
	maxX = centerCellX + radiusCells;
	minY = centerCellY - radiusCells;
	maxY = centerCellY + radiusCells;

	w = maxX - minX + 1;
	h = maxY - minY + 1;
	if ( w > SECTOR_GRAPH_WINDOW_MAX ) {
		minX = centerCellX - SECTOR_GRAPH_WINDOW_MAX / 2;
		w = SECTOR_GRAPH_WINDOW_MAX;
		maxX = minX + w - 1;
	}
	if ( h > SECTOR_GRAPH_WINDOW_MAX ) {
		minY = centerCellY - SECTOR_GRAPH_WINDOW_MAX / 2;
		h = SECTOR_GRAPH_WINDOW_MAX;
		maxY = minY + h - 1;
	}

	s_window.originCellX = minX;
	s_window.originCellY = minY;
	s_window.width = w;
	s_window.height = h;
	s_window.nodeCount = w * h;
	s_window.edgeCount = 0;

	for ( nodeId = 0; nodeId < s_window.nodeCount; nodeId++ ) {
		s_window.rowPtr[nodeId] = s_window.edgeCount;
		SG_CellFromNode( nodeId, &x, &y );

		for ( n = 0; n < 4; n++ ) {
			int nid = SG_NodeId( x + card[n][0], y + card[n][1] );
			if ( nid >= 0 ) {
				if ( r_graphBlockUnloaded && r_graphBlockUnloaded->integer ) {
					int nx, ny;
					SG_CellFromNode( nid, &nx, &ny );
					if ( !SG_IsLoadedCell( nx, ny ) ) {
						continue;
					}
				}
				s_window.colIdx[s_window.edgeCount++] = nid;
			}
		}
		if ( diagonals ) {
			for ( n = 0; n < 4; n++ ) {
				int nid = SG_NodeId( x + diag[n][0], y + diag[n][1] );
				if ( nid >= 0 ) {
					if ( r_graphBlockUnloaded && r_graphBlockUnloaded->integer ) {
						int nx, ny;
						SG_CellFromNode( nid, &nx, &ny );
						if ( !SG_IsLoadedCell( nx, ny ) ) {
							continue;
						}
					}
					s_window.colIdx[s_window.edgeCount++] = nid;
				}
			}
		}
	}
	s_window.rowPtr[s_window.nodeCount] = s_window.edgeCount;
}

static qboolean SG_RunBfsSources( const int *sourceNodes, int sourceCount, int maxHops )
{
	int queue[SECTOR_GRAPH_MAX_NODES];
	int dist[SECTOR_GRAPH_MAX_NODES];
	int head, tail;
	int i;

	if ( !sourceNodes || sourceCount <= 0 || maxHops < 0 || s_window.nodeCount <= 0 ) {
		SG_ClearReach();
		return qfalse;
	}
	if ( s_window.nodeCount > SECTOR_GRAPH_MAX_NODES ) {
		return qfalse;
	}

	for ( i = 0; i < s_window.nodeCount; i++ ) {
		dist[i] = -1;
		s_hopDist[i] = -1;
	}
	SG_ClearReach();
	head = tail = 0;

	for ( i = 0; i < sourceCount; i++ ) {
		int sid = sourceNodes[i];
		if ( sid < 0 || sid >= s_window.nodeCount ) {
			continue;
		}
		if ( dist[sid] >= 0 ) {
			continue;
		}
		dist[sid] = 0;
		s_hopDist[sid] = 0;
		SG_SetReachNode( sid );
		queue[tail++] = sid;
	}

	while ( head < tail ) {
		int cur = queue[head++];
		int d = dist[cur];
		if ( d >= maxHops ) {
			continue;
		}
		for ( i = s_window.rowPtr[cur]; i < s_window.rowPtr[cur + 1]; i++ ) {
			int nb = s_window.colIdx[i];
			if ( dist[nb] >= 0 ) {
				continue;
			}
			dist[nb] = d + 1;
			s_hopDist[nb] = (int8_t)( d + 1 );
			SG_SetReachNode( nb );
			queue[tail++] = nb;
		}
	}

	s_reachValid = qtrue;
	s_lastMaxHops = maxHops;
	return qtrue;
}

static void SG_LogEnableOnce( void )
{
	if ( s_loggedEnable ) {
		return;
	}
	s_loggedEnable = qtrue;
	Com_Printf( "[sector_graph] stream_reach hops=%d nodes=%d edges=%d compute=%s\n",
		r_graphStreamHops ? r_graphStreamHops->integer : 8,
		s_window.nodeCount,
		s_window.edgeCount,
		( r_graphCompute && r_graphCompute->integer && s_gpuReachFn ) ? "on" : "off" );
}

void SectorGraph_Init( void )
{
	r_graphStreamReach = Cvar_Get( "r_graphStreamReach", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphStreamReach,
		"Filter WorldResidency candidates by k-hop sector graph reachability from player(s)." );
	r_graphStreamHops = Cvar_Get( "r_graphStreamHops", "8", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphStreamHops, "Max BFS hops for sector graph reachability." );
	r_graphSectorDiagonals = Cvar_Get( "r_graphSectorDiagonals", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphSectorDiagonals, "8-neighbor sector graph edges (default 4-neighbor)." );
	r_graphBlockUnloaded = Cvar_Get( "r_graphBlockUnloaded", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphBlockUnloaded,
		"Block edges between unloaded sectors (requires loaded collision/nav state)." );
	r_graphCompute = Cvar_Get( "r_graphCompute", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphCompute, "Run optional Vulkan compute BFS (client GPU path)." );
	r_graphStreamVerify = Cvar_Get( "r_graphStreamVerify", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphStreamVerify,
		"Compare CPU vs GPU reachability bitsets (developer)." );
	r_graphNavInfluence = Cvar_Get( "r_graphNavInfluence", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphNavInfluence,
		"Expose sector hop-distance influence field for nav/AI heatmaps." );

	Com_Memset( &s_window, 0, sizeof( s_window ) );
	SG_ClearReach();
	s_gpuReachFn = NULL;
	s_loggedEnable = qfalse;

	Cmd_AddCommand( "graph_reach_test", SectorGraph_ReachTest_f );
	Cmd_AddCommand( "graph_bfs_status", SectorGraph_Status_f );
	Cmd_AddCommand( "graph_influence_list", SectorGraph_InfluenceList_f );
	Cmd_AddCommand( "graph_bfs_crossover", SectorGraph_CrossoverBench_f );
}

void SectorGraph_Shutdown( void )
{
	Cmd_RemoveCommand( "graph_reach_test" );
	Cmd_RemoveCommand( "graph_bfs_status" );
	Cmd_RemoveCommand( "graph_influence_list" );
	Cmd_RemoveCommand( "graph_bfs_crossover" );
	Com_Memset( &s_window, 0, sizeof( s_window ) );
	SG_ClearReach();
	s_gpuReachFn = NULL;
	s_loggedEnable = qfalse;
	s_cacheCenterX = 0;
	s_cacheCenterY = 0;
	s_cacheRadiusCells = 0;
	s_cacheMaxHops = 0;
	s_cacheOriginHash = 0;
	s_cacheDiagonals = qfalse;
}

void SectorGraph_SetGpuReachFn( sectorGraphGpuReach_f fn )
{
	s_gpuReachFn = fn;
}

qboolean SectorGraph_StreamReachEnabled( void )
{
#ifdef SECTOR_GRAPH_UNIT_TEST
	if ( s_testForceReach ) {
		return qtrue;
	}
#endif
	return r_graphStreamReach && r_graphStreamReach->integer;
}

qboolean SectorGraph_ComputeEnabled( void )
{
	return r_graphCompute && r_graphCompute->integer && s_gpuReachFn != NULL;
}

qboolean SectorGraph_VerifyEnabled( void )
{
	return r_graphStreamVerify && r_graphStreamVerify->integer;
}

qboolean SectorGraph_NavInfluenceEnabled( void )
{
	return r_graphNavInfluence && r_graphNavInfluence->integer;
}

int SectorGraph_GetHopDistance( int cellX, int cellY )
{
	int nodeId;

	if ( !s_reachValid ) {
		return -1;
	}
	nodeId = SG_NodeId( cellX, cellY );
	if ( nodeId < 0 || nodeId >= s_window.nodeCount ) {
		return -1;
	}
	return s_hopDist[nodeId];
}

float SectorGraph_GetInfluence( int cellX, int cellY )
{
	int dist;
	float maxHops;

	if ( !SectorGraph_NavInfluenceEnabled() && !s_reachValid ) {
		return 0.0f;
	}
	dist = SectorGraph_GetHopDistance( cellX, cellY );
	if ( dist < 0 ) {
		return 0.0f;
	}
	maxHops = (float)( s_lastMaxHops > 0 ? s_lastMaxHops : 1 );
	return 1.0f - ( (float)dist / ( maxHops + 0.001f ) );
}

int SectorGraph_GetNodeCount( void )
{
	return s_window.nodeCount;
}

void SectorGraph_GetReachBits( const uint32_t **bits, int *bitWords )
{
	if ( bits ) {
		*bits = s_reachBits;
	}
	if ( bitWords ) {
		*bitWords = SECTOR_GRAPH_BITWORDS;
	}
}

void SectorGraph_UpdateReachability( const vec3_t viewOrigin, const vec3_t *extraOrigins,
	int extraOriginCount, float sectorSize, float unloadRadius, int maxHops )
{
	int sourceNodes[SECTOR_GRAPH_MAX_NODES];
	int sourceCount = 0;
	int centerX, centerY;
	int radiusCells;
	qboolean diagonals;
	unsigned originHash;
	int i;

	if ( !SectorGraph_StreamReachEnabled() && !SectorGraph_ComputeEnabled() ) {
		s_reachValid = qfalse;
		return;
	}

	if ( !viewOrigin || sectorSize < 256.0f ) {
		s_reachValid = qfalse;
		return;
	}

	if ( maxHops < 0 ) {
		maxHops = r_graphStreamHops ? r_graphStreamHops->integer : 8;
	}
	if ( maxHops < 0 ) {
		maxHops = 0;
	}

	CM_Stream_WorldToCell( viewOrigin, sectorSize, &centerX, &centerY );
	radiusCells = (int)ceil( unloadRadius / sectorSize ) + 1;
	if ( radiusCells < 1 ) {
		radiusCells = 1;
	}
	diagonals = r_graphSectorDiagonals && r_graphSectorDiagonals->integer;
	originHash = SG_HashExtraOrigins( extraOrigins, extraOriginCount, sectorSize );

	if ( SG_CacheHit( centerX, centerY, radiusCells, maxHops, diagonals, originHash ) ) {
		return;
	}

	SG_BuildWindow( centerX, centerY, radiusCells, diagonals );
	SG_LogEnableOnce();

	{
		int nid = SG_NodeId( centerX, centerY );
		if ( nid >= 0 ) {
			sourceNodes[sourceCount++] = nid;
		}
	}
	for ( i = 0; i < extraOriginCount && sourceCount < SECTOR_GRAPH_MAX_NODES; i++ ) {
		int cx, cy, nid;
		if ( !extraOrigins ) {
			break;
		}
		CM_Stream_WorldToCell( extraOrigins[i], sectorSize, &cx, &cy );
		nid = SG_NodeId( cx, cy );
		if ( nid >= 0 ) {
			int j;
			qboolean dup = qfalse;
			for ( j = 0; j < sourceCount; j++ ) {
				if ( sourceNodes[j] == nid ) {
					dup = qtrue;
					break;
				}
			}
			if ( !dup ) {
				sourceNodes[sourceCount++] = nid;
			}
		}
	}

	SG_RunBfsSources( sourceNodes, sourceCount, maxHops );

	if ( SectorGraph_ComputeEnabled() ) {
		sectorGraphGpuQuery_t query;

		Com_Memset( &query, 0, sizeof( query ) );
		query.originCellX = s_window.originCellX;
		query.originCellY = s_window.originCellY;
		query.width = s_window.width;
		query.height = s_window.height;
		query.nodeCount = s_window.nodeCount;
		query.maxHops = maxHops;
		query.diagonals = diagonals ? 1 : 0;
		query.sourceNodeIds = sourceNodes;
		query.sourceCount = sourceCount;
		query.rowPtr = s_window.rowPtr;
		query.colIdx = s_window.colIdx;

		Com_Memset( s_gpuReachBits, 0, sizeof( s_gpuReachBits ) );
		if ( s_gpuReachFn( &query, s_gpuReachBits, SECTOR_GRAPH_BITWORDS ) ) {
			if ( SectorGraph_VerifyEnabled() ) {
				for ( i = 0; i < SECTOR_GRAPH_BITWORDS; i++ ) {
					if ( s_reachBits[i] != s_gpuReachBits[i] ) {
						Com_Printf( S_COLOR_YELLOW
							"Warning: [sector_graph] CPU/GPU reach mismatch word %d cpu=0x%x gpu=0x%x\n",
							i, s_reachBits[i], s_gpuReachBits[i] );
						break;
					}
				}
			}
		}
	}
	SG_CacheStore( centerX, centerY, radiusCells, maxHops, diagonals, originHash );
}

qboolean SectorGraph_IsReachable( int cellX, int cellY )
{
	int nodeId;

	if ( !s_reachValid || !SectorGraph_StreamReachEnabled() ) {
		return qtrue;
	}
	nodeId = SG_NodeId( cellX, cellY );
	if ( nodeId < 0 ) {
		return qfalse;
	}
	return SG_GetReachNode( nodeId );
}

qboolean SectorGraph_ReachTest( int srcX, int srcY, int dstX, int dstY, int maxHops )
{
	int sources[1];
	int nid;
	int radiusCells;
	qboolean diagonals;

	if ( maxHops < 0 ) {
		maxHops = r_graphStreamHops ? r_graphStreamHops->integer : 8;
	}

	radiusCells = SECTOR_GRAPH_WINDOW_MAX / 2;
	diagonals = r_graphSectorDiagonals && r_graphSectorDiagonals->integer;
	SG_BuildWindow( srcX, srcY, radiusCells, diagonals );

	sources[0] = SG_NodeId( srcX, srcY );
	if ( sources[0] < 0 ) {
		return qfalse;
	}
	SG_RunBfsSources( sources, 1, maxHops );

	nid = SG_NodeId( dstX, dstY );
	if ( nid < 0 ) {
		return qfalse;
	}
	return SG_GetReachNode( nid );
}

void SectorGraph_ReachTest_f( void )
{
	int srcX, srcY, dstX, dstY, hops;
	qboolean ok;

	if ( Cmd_Argc() < 5 ) {
		Com_Printf( "Usage: graph_reach_test <srcX> <srcY> <dstX> <dstY> [hops]\n" );
		return;
	}
	srcX = atoi( Cmd_Argv( 1 ) );
	srcY = atoi( Cmd_Argv( 2 ) );
	dstX = atoi( Cmd_Argv( 3 ) );
	dstY = atoi( Cmd_Argv( 4 ) );
	hops = r_graphStreamHops ? r_graphStreamHops->integer : 8;
	if ( Cmd_Argc() >= 6 ) {
		hops = atoi( Cmd_Argv( 5 ) );
	}
	ok = SectorGraph_ReachTest( srcX, srcY, dstX, dstY, hops );
	Com_Printf( "[sector_graph] reach (%d,%d)->(%d,%d) hops=%d: %s\n",
		srcX, srcY, dstX, dstY, hops, ok ? "YES" : "NO" );
}

void SectorGraph_Status_f( void )
{
	Com_Printf( "[sector_graph] stream_reach=%d hops=%d diagonals=%d block_unloaded=%d compute=%d verify=%d\n",
		SectorGraph_StreamReachEnabled(),
		r_graphStreamHops ? r_graphStreamHops->integer : 8,
		r_graphSectorDiagonals ? r_graphSectorDiagonals->integer : 0,
		r_graphBlockUnloaded ? r_graphBlockUnloaded->integer : 0,
		r_graphCompute ? r_graphCompute->integer : 0,
		r_graphStreamVerify ? r_graphStreamVerify->integer : 0 );
	Com_Printf( "[sector_graph] window origin=(%d,%d) size=%dx%d nodes=%d edges=%d reach_valid=%s\n",
		s_window.originCellX, s_window.originCellY,
		s_window.width, s_window.height,
		s_window.nodeCount, s_window.edgeCount,
		s_reachValid ? "yes" : "no" );
}

void SectorGraph_InfluenceList_f( void )
{
	int nodeId;
	int listed = 0;

	if ( !s_reachValid ) {
		Com_Printf( "[sector_graph] no reachability field (enable r_graphStreamReach 1 and enter world)\n" );
		return;
	}

	for ( nodeId = 0; nodeId < s_window.nodeCount; nodeId++ ) {
		int cellX, cellY;
		if ( s_hopDist[nodeId] < 0 ) {
			continue;
		}
		SG_CellFromNode( nodeId, &cellX, &cellY );
		Com_Printf( "  cell (%d,%d) hops=%d influence=%.3f\n",
			cellX, cellY, s_hopDist[nodeId],
			SectorGraph_GetInfluence( cellX, cellY ) );
		if ( ++listed >= 64 ) {
			Com_Printf( "  ... truncated (64 cells)\n" );
			break;
		}
	}
	Com_Printf( "[sector_graph] influence list: %d reachable cells (max_hops=%d)\n",
		listed, s_lastMaxHops );
}

static void SG_CrossoverBenchGrid( int gridSize, int iterations, int maxHops, int *totalUs )
{
	int rowPtr[SECTOR_GRAPH_MAX_NODES + 1];
	int colIdx[SECTOR_GRAPH_MAX_NODES * 8];
	int sources[1];
	int x, y, nodeId, e;
	int64_t t0, t1;
	int i;

	if ( gridSize * gridSize > SECTOR_GRAPH_MAX_NODES ) {
		return;
	}

	nodeId = 0;
	e = 0;
	for ( y = 0; y < gridSize; y++ ) {
		for ( x = 0; x < gridSize; x++ ) {
			rowPtr[nodeId] = e;
			if ( x + 1 < gridSize ) {
				colIdx[e++] = nodeId + 1;
			}
			if ( x > 0 ) {
				colIdx[e++] = nodeId - 1;
			}
			if ( y + 1 < gridSize ) {
				colIdx[e++] = nodeId + gridSize;
			}
			if ( y > 0 ) {
				colIdx[e++] = nodeId - gridSize;
			}
			nodeId++;
		}
	}
	rowPtr[gridSize * gridSize] = e;

	s_window.nodeCount = gridSize * gridSize;
	s_window.width = gridSize;
	s_window.height = gridSize;
	s_window.originCellX = 0;
	s_window.originCellY = 0;
	Com_Memcpy( s_window.rowPtr, rowPtr, sizeof( int ) * ( gridSize * gridSize + 1 ) );
	Com_Memcpy( s_window.colIdx, colIdx, sizeof( int ) * e );
	s_window.edgeCount = e;

	sources[0] = 0;
	t0 = Sys_Microseconds();
	for ( i = 0; i < iterations; i++ ) {
		(void)SG_RunBfsSources( sources, 1, maxHops );
	}
	t1 = Sys_Microseconds();
	*totalUs = (int)( ( t1 - t0 ) / 1000 );
	Com_Printf( "[sector_graph] crossover %dx%d x%d iters ~%d ms (avg %d us)\n",
		gridSize, gridSize, iterations, *totalUs,
		iterations > 0 ? ( *totalUs * 1000 ) / iterations : 0 );
}

void SectorGraph_CrossoverBench_f( void )
{
	int ms5, ms32;
	const int crossoverUs = 500;

	SG_CrossoverBenchGrid( 5, 200, 8, &ms5 );
	SG_CrossoverBenchGrid( 32, 50, 8, &ms32 );

	Com_Printf( "[sector_graph] crossover: CPU BFS 5x5=%dms 32x32=%dms\n", ms5, ms32 );
	if ( ms32 * 1000 / 50 > crossoverUs ) {
		Com_Printf( "[sector_graph] recommendation: try r_graphCompute 1 for >=32x32 windows "
			"(avg > %d us/iter)\n", crossoverUs );
	} else {
		Com_Printf( "[sector_graph] recommendation: CPU path sufficient at current window sizes\n" );
	}
	Com_Printf( "[sector_graph] WMMA/BLEST path deferred until GPU bench (graph_bfs_bench) beats CPU here\n" );
}

#ifdef SECTOR_GRAPH_UNIT_TEST
void SectorGraph_ResetForTest( void )
{
	Com_Memset( &s_window, 0, sizeof( s_window ) );
	SG_ClearReach();
	s_testForceReach = qfalse;
	s_cacheCenterX = 0;
	s_cacheCenterY = 0;
	s_cacheRadiusCells = 0;
	s_cacheMaxHops = 0;
	s_cacheOriginHash = 0;
	s_cacheDiagonals = qfalse;
}

void SectorGraph_EnableForTest( qboolean enable )
{
	s_testForceReach = enable;
}

void SectorGraph_BuildWindowForTest( int centerCellX, int centerCellY, int radiusCells, qboolean diagonals )
{
	SG_BuildWindow( centerCellX, centerCellY, radiusCells, diagonals );
}

qboolean SectorGraph_RunBfsCpuSources( const int *sourceNodes, int sourceCount, int maxHops )
{
	return SG_RunBfsSources( sourceNodes, sourceCount, maxHops );
}

int SectorGraph_CellToNode( int cellX, int cellY )
{
	return SG_NodeId( cellX, cellY );
}

int SectorGraph_GetHopDistanceNode( int nodeId )
{
	if ( nodeId < 0 || nodeId >= s_window.nodeCount || !s_reachValid ) {
		return -1;
	}
	return s_hopDist[nodeId];
}
#endif
}
