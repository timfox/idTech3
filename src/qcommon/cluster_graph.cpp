/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

/* C++20 migration: extern "C" API boundary preserved. */

extern "C" {
#include "q_shared.h"
#include "qcommon.h"
#include "cm_local.h"
}

#include "cluster_graph.h"

#include <cstdlib>
#include <cstring>
#include <span>

typedef struct {
	int numClusters;
	int edgeCount;
	int rowPtr[CLUSTER_GRAPH_MAX_CLUSTERS + 1];
	int colIdx[CLUSTER_GRAPH_MAX_EDGES];
} clusterGraphCsr_t;

static cvar_t *r_graphClusterReach;
static cvar_t *r_graphClusterHops;

static clusterGraphCsr_t s_csr;
static int8_t s_hopDist[CLUSTER_GRAPH_MAX_CLUSTERS];
static qboolean s_reachValid;
static int s_lastSourceCluster;
static int s_lastMaxHops;
static qboolean s_loggedEnable;
static qboolean s_inited;

[[nodiscard]] static int CG_ClusterForChild( int num )
{
	if ( num < 0 ) {
		int leafnum = -1 - num;
		if ( leafnum < 0 || leafnum >= cm.numLeafs ) {
			return -1;
		}
		return cm.leafs[leafnum].cluster;
	}
	if ( num >= cm.numNodes ) {
		return -1;
	}
	{
		int c0 = CG_ClusterForChild( cm.nodes[num].children[0] );
		if ( c0 >= 0 ) {
			return c0;
		}
		return CG_ClusterForChild( cm.nodes[num].children[1] );
	}
}

[[nodiscard]] static qboolean CG_EdgeExists( std::span<const int> edges, int edgeCount, int a, int b )
{
	for ( int i = 0; i < edgeCount; i++ ) {
		if ( edges[i * 2] == a && edges[i * 2 + 1] == b ) {
			return qtrue;
		}
	}
	return qfalse;
}

#define CG_EDGE_HASH_SIZE 16384

static_assert( ( CG_EDGE_HASH_SIZE & ( CG_EDGE_HASH_SIZE - 1 ) ) == 0,
	"CG_EDGE_HASH_SIZE must be a power of two" );

static uint64_t s_edgeHashKeys[CG_EDGE_HASH_SIZE];

[[nodiscard]] static uint64_t CG_EdgeHashKey( int a, int b )
{
	if ( a > b ) {
		int t = a;
		a = b;
		b = t;
	}
	return ( (uint64_t)(uint32_t)a << 32 ) | (uint32_t)b;
}

static void CG_EdgeHashClear( void )
{
	Com_Memset( s_edgeHashKeys, 0, sizeof( s_edgeHashKeys ) );
}

[[nodiscard]] static qboolean CG_EdgeHashContainsOrInsert( int a, int b )
{
	uint64_t key;
	uint32_t idx;
	uint32_t start;
	int probe;

	key = CG_EdgeHashKey( a, b );
	if ( key == 0 ) {
		key = 1;
	}
	start = idx = (uint32_t)( key ^ ( key >> 33 ) ) & ( CG_EDGE_HASH_SIZE - 1 );
	for ( probe = 0; probe < CG_EDGE_HASH_SIZE; probe++ ) {
		if ( s_edgeHashKeys[idx] == 0 ) {
			s_edgeHashKeys[idx] = key;
			return qfalse;
		}
		if ( s_edgeHashKeys[idx] == key ) {
			return qtrue;
		}
		idx = ( idx + 1 ) & ( CG_EDGE_HASH_SIZE - 1 );
		if ( idx == start ) {
			return qfalse;
		}
	}
	return qfalse;
}

static void CG_AddUndirectedEdge( int *edges, int *edgeCount, int a, int b )
{
	if ( a < 0 || b < 0 || a >= CLUSTER_GRAPH_MAX_CLUSTERS || b >= CLUSTER_GRAPH_MAX_CLUSTERS ) {
		return;
	}
	if ( a == b ) {
		return;
	}
	if ( CG_EdgeHashContainsOrInsert( a, b ) ||
		CG_EdgeExists( std::span<const int>( edges, (size_t)( *edgeCount ) * 2 ), *edgeCount, a, b ) ) {
		return;
	}
	if ( *edgeCount >= CLUSTER_GRAPH_MAX_EDGES / 2 ) {
		return;
	}
	edges[(*edgeCount) * 2] = a;
	edges[(*edgeCount) * 2 + 1] = b;
	(*edgeCount)++;
}

static void CG_BuildCsrFromEdges( int numClusters, std::span<const int> edges, int edgeCount )
{
	int perCluster[CLUSTER_GRAPH_MAX_CLUSTERS];
	int i, c, e;

	Com_Memset( &s_csr, 0, sizeof( s_csr ) );
	s_csr.numClusters = numClusters;
	Com_Memset( perCluster, 0, sizeof( perCluster ) );

	for ( i = 0; i < edgeCount; i++ ) {
		int a = edges[i * 2];
		int b = edges[i * 2 + 1];
		if ( a >= 0 && a < numClusters ) {
			perCluster[a]++;
		}
		if ( b >= 0 && b < numClusters ) {
			perCluster[b]++;
		}
	}

	s_csr.rowPtr[0] = 0;
	for ( c = 0; c < numClusters; c++ ) {
		s_csr.rowPtr[c + 1] = s_csr.rowPtr[c] + perCluster[c];
	}
	s_csr.edgeCount = s_csr.rowPtr[numClusters];
	if ( s_csr.edgeCount > CLUSTER_GRAPH_MAX_EDGES ) {
		Com_Printf( S_COLOR_YELLOW
			"Warning: [cluster_graph] edge overflow (%d > %d), graph truncated\n",
			s_csr.edgeCount, CLUSTER_GRAPH_MAX_EDGES );
		s_csr.edgeCount = CLUSTER_GRAPH_MAX_EDGES;
		s_csr.rowPtr[numClusters] = CLUSTER_GRAPH_MAX_EDGES;
	}

	Com_Memset( perCluster, 0, sizeof( perCluster ) );
	for ( i = 0; i < edgeCount; i++ ) {
		int a = edges[i * 2];
		int b = edges[i * 2 + 1];
		if ( a >= 0 && a < numClusters ) {
			e = s_csr.rowPtr[a] + perCluster[a]++;
			if ( e < CLUSTER_GRAPH_MAX_EDGES ) {
				s_csr.colIdx[e] = b;
			}
		}
		if ( b >= 0 && b < numClusters ) {
			e = s_csr.rowPtr[b] + perCluster[b]++;
			if ( e < CLUSTER_GRAPH_MAX_EDGES ) {
				s_csr.colIdx[e] = a;
			}
		}
	}
}

static void CG_ClearReach( void )
{
	for ( int i = 0; i < CLUSTER_GRAPH_MAX_CLUSTERS; i++ ) {
		s_hopDist[i] = -1;
	}
	s_reachValid = qfalse;
}

[[nodiscard]] static qboolean CG_RunBfs( int sourceCluster, int maxHops )
{
	int queue[CLUSTER_GRAPH_MAX_CLUSTERS];
	int head, tail;

	if ( sourceCluster < 0 || sourceCluster >= s_csr.numClusters || maxHops < 0 ) {
		CG_ClearReach();
		return qfalse;
	}

	CG_ClearReach();
	head = tail = 0;
	s_hopDist[sourceCluster] = 0;
	queue[tail++] = sourceCluster;

	while ( head < tail ) {
		int cur = queue[head++];
		int d = s_hopDist[cur];
		if ( d >= maxHops ) {
			continue;
		}
		for ( int i = s_csr.rowPtr[cur]; i < s_csr.rowPtr[cur + 1]; i++ ) {
			int nb = s_csr.colIdx[i];
			if ( nb < 0 || nb >= s_csr.numClusters ) {
				continue;
			}
			if ( s_hopDist[nb] >= 0 ) {
				continue;
			}
			s_hopDist[nb] = (int8_t)( d + 1 );
			queue[tail++] = nb;
		}
	}

	s_reachValid = qtrue;
	s_lastSourceCluster = sourceCluster;
	s_lastMaxHops = maxHops;
	return qtrue;
}

extern "C" {

void ClusterGraph_Init( void )
{
	if ( s_inited ) {
		return;
	}
	s_inited = qtrue;

	r_graphClusterReach = Cvar_Get( "r_graphClusterReach", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphClusterReach,
		"Enable BSP cluster portal graph k-hop reachability queries." );
	r_graphClusterHops = Cvar_Get( "r_graphClusterHops", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( r_graphClusterHops, "Max BFS hops on cluster portal graph." );

	Com_Memset( &s_csr, 0, sizeof( s_csr ) );
	CG_ClearReach();
	s_loggedEnable = qfalse;

	Cmd_AddCommand( "cluster_graph_status", ClusterGraph_Status_f );
	Cmd_AddCommand( "cluster_graph_reach", ClusterGraph_ReachTest_f );
}

void ClusterGraph_Shutdown( void )
{
	if ( !s_inited ) {
		return;
	}
	Cmd_RemoveCommand( "cluster_graph_status" );
	Cmd_RemoveCommand( "cluster_graph_reach" );
	Com_Memset( &s_csr, 0, sizeof( s_csr ) );
	CG_ClearReach();
	s_loggedEnable = qfalse;
	s_inited = qfalse;
}

void ClusterGraph_RebuildFromMap( void )
{
	int edges[CLUSTER_GRAPH_MAX_EDGES * 2];
	int edgeCount = 0;
	int numClusters;
	int i;

	Com_Memset( &s_csr, 0, sizeof( s_csr ) );
	CG_ClearReach();
	CG_EdgeHashClear();

	numClusters = cm.numClusters;
	if ( numClusters <= 0 || numClusters > CLUSTER_GRAPH_MAX_CLUSTERS ) {
		return;
	}

	for ( i = 0; i < cm.numNodes; i++ ) {
		int c0 = CG_ClusterForChild( cm.nodes[i].children[0] );
		int c1 = CG_ClusterForChild( cm.nodes[i].children[1] );
		if ( c0 >= 0 && c1 >= 0 && c0 != c1 ) {
			CG_AddUndirectedEdge( edges, &edgeCount, c0, c1 );
		}
	}

	CG_BuildCsrFromEdges( numClusters, std::span<const int>( edges, (size_t)edgeCount * 2 ), edgeCount );

	if ( !s_loggedEnable && r_graphClusterReach && r_graphClusterReach->integer ) {
		s_loggedEnable = qtrue;
		Com_Printf( "[cluster_graph] rebuilt clusters=%d edges=%d hops=%d\n",
			s_csr.numClusters, s_csr.edgeCount,
			r_graphClusterHops ? r_graphClusterHops->integer : 4 );
	}
}

qboolean ClusterGraph_IsBuilt( void )
{
	return s_csr.numClusters > 0 && s_csr.edgeCount > 0;
}

int ClusterGraph_GetClusterCount( void )
{
	return s_csr.numClusters;
}

int ClusterGraph_GetEdgeCount( void )
{
	return s_csr.edgeCount;
}

qboolean ClusterGraph_ReachEnabled( void )
{
	return r_graphClusterReach && r_graphClusterReach->integer;
}

void ClusterGraph_UpdateReachability( int sourceCluster, int maxHops )
{
	if ( !ClusterGraph_ReachEnabled() || !ClusterGraph_IsBuilt() ) {
		CG_ClearReach();
		return;
	}
	if ( maxHops < 0 ) {
		maxHops = r_graphClusterHops ? r_graphClusterHops->integer : 4;
	}
	if ( s_reachValid && s_lastSourceCluster == sourceCluster && s_lastMaxHops == maxHops ) {
		return;
	}
	CG_RunBfs( sourceCluster, maxHops );
}

qboolean ClusterGraph_IsReachable( int cluster )
{
	if ( !ClusterGraph_ReachEnabled() || !s_reachValid ) {
		return qtrue;
	}
	if ( cluster < 0 || cluster >= s_csr.numClusters ) {
		return qfalse;
	}
	return s_hopDist[cluster] >= 0;
}

int ClusterGraph_GetHopDistance( int cluster )
{
	if ( !s_reachValid || cluster < 0 || cluster >= s_csr.numClusters ) {
		return -1;
	}
	return s_hopDist[cluster];
}

float ClusterGraph_GetInfluence( int cluster )
{
	int dist;
	float maxHops;

	if ( !s_reachValid || cluster < 0 || cluster >= s_csr.numClusters ) {
		return 0.0f;
	}
	dist = s_hopDist[cluster];
	if ( dist < 0 ) {
		return 0.0f;
	}
	maxHops = (float)( s_lastMaxHops > 0 ? s_lastMaxHops : 1 );
	return 1.0f - ( (float)dist / ( maxHops + 0.001f ) );
}

void ClusterGraph_Status_f( void )
{
	Com_Printf( "[cluster_graph] reach=%d hops=%d built=%s clusters=%d edges=%d reach_valid=%s\n",
		ClusterGraph_ReachEnabled(),
		r_graphClusterHops ? r_graphClusterHops->integer : 4,
		ClusterGraph_IsBuilt() ? "yes" : "no",
		s_csr.numClusters,
		s_csr.edgeCount,
		s_reachValid ? "yes" : "no" );
	if ( s_reachValid ) {
		Com_Printf( "[cluster_graph] last source=%d max_hops=%d\n",
			s_lastSourceCluster, s_lastMaxHops );
	}
}

void ClusterGraph_ReachTest_f( void )
{
	int src, hops, c, reachable = 0;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: cluster_graph_reach <sourceCluster> [hops]\n" );
		return;
	}
	if ( !ClusterGraph_IsBuilt() ) {
		Com_Printf( "[cluster_graph] no map graph (load a BSP first)\n" );
		return;
	}

	src = atoi( Cmd_Argv( 1 ) );
	hops = r_graphClusterHops ? r_graphClusterHops->integer : 4;
	if ( Cmd_Argc() >= 3 ) {
		hops = atoi( Cmd_Argv( 2 ) );
	}

	CG_RunBfs( src, hops );
	for ( c = 0; c < s_csr.numClusters; c++ ) {
		if ( s_hopDist[c] >= 0 ) {
			reachable++;
		}
	}
	Com_Printf( "[cluster_graph] source=%d hops=%d reachable=%d/%d\n",
		src, hops, reachable, s_csr.numClusters );
}

#ifdef CLUSTER_GRAPH_UNIT_TEST
void ClusterGraph_ResetForTest( void )
{
	Com_Memset( &s_csr, 0, sizeof( s_csr ) );
	CG_ClearReach();
}

void ClusterGraph_BuildFromTestEdges( int numClusters, const int *pairs, int pairCount )
{
	int edges[CLUSTER_GRAPH_MAX_EDGES * 2];
	int edgeCount = 0;

	if ( numClusters <= 0 || numClusters > CLUSTER_GRAPH_MAX_CLUSTERS ) {
		return;
	}
	for ( int i = 0; i < pairCount; i++ ) {
		CG_AddUndirectedEdge( edges, &edgeCount, pairs[i * 2], pairs[i * 2 + 1] );
	}
	CG_BuildCsrFromEdges( numClusters, std::span<const int>( edges, (size_t)edgeCount * 2 ), edgeCount );
}

qboolean ClusterGraph_RunBfsTest( int sourceCluster, int maxHops )
{
	return CG_RunBfs( sourceCluster, maxHops );
}
#endif

} /* extern "C" */
