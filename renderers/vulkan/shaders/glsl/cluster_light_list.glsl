/*
 * Shared light-list fetch for compact (offset/count) and legacy fixed-slot layouts.
 * Expects a flat uint SSBO named via CLUSTER_LIST_CELLS (default: tiles.cells).
 *
 * Compact layout in the tile SSBO:
 *   meta[4]: [0]=nextIndex cursor, [1]=overflowCount, [2]=flags, [3]=reserved
 *   headers: clusterCount * 2 uints (offset, count) starting at uint index 4
 *   indices: uints starting at 4 + clusterCount*2
 */

#ifndef CLUSTER_LIGHT_LIST_GLSL
#define CLUSTER_LIGHT_LIST_GLSL

#include "cluster_contract.glsl"

#ifndef CLUSTER_LIST_CELLS
#error "Define CLUSTER_LIST_CELLS to the uint array (e.g. tiles.cells or fp_tiles.fp_tile_cells)"
#endif

const uint CLUSTER_LIST_META_UINTS = 4u;

uint Cluster_ListIndexBase( uint clusterCount )
{
	return CLUSTER_LIST_META_UINTS + clusterCount * 2u;
}

uint Cluster_FetchLightIndex( uint clusterId, uint k, uint compact, uint maxPerLegacy, uint clusterCount )
{
	if ( compact != 0u ) {
		uint hoff = CLUSTER_LIST_META_UINTS + clusterId * 2u;
		uint offset = CLUSTER_LIST_CELLS[hoff];
		uint count = CLUSTER_LIST_CELLS[hoff + 1u];
		if ( k >= count ) {
			return 0xFFFFFFFFu;
		}
		return CLUSTER_LIST_CELLS[Cluster_ListIndexBase( clusterCount ) + offset + k];
	}
	{
		uint tbase = clusterId * maxPerLegacy;
		return CLUSTER_LIST_CELLS[tbase + k];
	}
}

uint Cluster_FetchLightCount( uint clusterId, uint compact, uint maxPerLegacy, uint clusterCount )
{
	if ( compact != 0u ) {
		uint hoff = CLUSTER_LIST_META_UINTS + clusterId * 2u;
		return CLUSTER_LIST_CELLS[hoff + 1u];
	}
	{
		uint tbase = clusterId * maxPerLegacy;
		uint c = 0u;
		for ( uint k = 0u; k < maxPerLegacy; k++ ) {
			if ( CLUSTER_LIST_CELLS[tbase + k] == 0xFFFFFFFFu ) {
				break;
			}
			c++;
		}
		return c;
	}
}

#endif /* CLUSTER_LIGHT_LIST_GLSL */
