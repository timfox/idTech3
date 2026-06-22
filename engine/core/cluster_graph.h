/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

BSP cluster portal graph — CSR adjacency from node splits + k-hop BFS reachability.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define CLUSTER_GRAPH_MAX_CLUSTERS 4096
#define CLUSTER_GRAPH_MAX_EDGES    ( CLUSTER_GRAPH_MAX_CLUSTERS * 64 )

void     ClusterGraph_Init( void );
void     ClusterGraph_Shutdown( void );
void     ClusterGraph_RebuildFromMap( void );

qboolean ClusterGraph_IsBuilt( void );
int      ClusterGraph_GetClusterCount( void );
int      ClusterGraph_GetEdgeCount( void );

qboolean ClusterGraph_ReachEnabled( void );
void     ClusterGraph_UpdateReachability( int sourceCluster, int maxHops );
qboolean ClusterGraph_IsReachable( int cluster );
int      ClusterGraph_GetHopDistance( int cluster );
float    ClusterGraph_GetInfluence( int cluster );

void     ClusterGraph_Status_f( void );
void     ClusterGraph_ReachTest_f( void );

#ifdef CLUSTER_GRAPH_UNIT_TEST
void     ClusterGraph_ResetForTest( void );
void     ClusterGraph_BuildFromTestEdges( int numClusters, const int *pairs, int pairCount );
qboolean ClusterGraph_RunBfsTest( int sourceCluster, int maxHops );
#endif

#ifdef __cplusplus
}
#endif
