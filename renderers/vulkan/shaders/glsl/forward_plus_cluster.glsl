/*
 * Shared Forward+ / clustered light-grid helpers.
 * M2: thin wrappers over cluster_contract.glsl (authoritative).
 * Cluster layout: flat 2D tile first (slice 0), then +tilesX*tilesY per Z slice.
 * zSlices==1 preserves historical 2D tiled indexing (tileId * MAX_PER_TILE).
 *
 * Depth: use positive view-space distance (abs of reconstructed view Z / clip.w).
 * Slice modes: 0 = linear in [zNear,zFar], 1 = logarithmic (log2 + zScale/zBias).
 */

#ifndef FORWARD_PLUS_CLUSTER_GLSL
#define FORWARD_PLUS_CLUSTER_GLSL

#include "cluster_contract.glsl"

uint fp_cluster_xy_count( uint tilesX, uint tilesY )
{
	return Cluster_XYCount( tilesX, tilesY );
}

uint fp_view_depth_to_slice( float viewDepth, uint zSlices, uint zMode, float zNear, float zFar )
{
	float zScale = 0.0;
	float zBias = 0.0;
	if ( zMode == 1u && zSlices > 1u ) {
		float zn = max( zNear, 1e-3 );
		float zf = max( zFar, zn + 1e-3 );
		float logNear = log2( zn );
		float logFar = log2( zf );
		float denom = max( logFar - logNear, 1e-5 );
		zScale = float( zSlices ) / denom;
		zBias = -logNear * zScale;
	}
	return Cluster_ViewDepthToSlice( viewDepth, zSlices, zMode, zNear, zFar, zScale, zBias );
}

uint fp_cluster_index( uint tx, uint ty, uint tilesX, uint tilesY, uint slice, uint zSlices )
{
	ClusterParams p;
	p.clusterCountX = tilesX;
	p.clusterCountY = tilesY;
	p.clusterCountZ = zSlices;
	p.tileSizeX = CLUSTER_TILE_SIZE_X;
	p.tileSizeY = CLUSTER_TILE_SIZE_Y;
	p.lightIndexCapacity = 0u;
	p.zNear = 0.0;
	p.zFar = 0.0;
	p.zScale = 0.0;
	p.zBias = 0.0;
	p.generation = 0u;
	p.overflowCount = 0u;
	p.flags = 0u;
	p.reserved = 0u;
	return Cluster_IndexFromTileAndSlice( tx, ty, slice, p );
}

/* Slice [near,far] in positive view depth for slice i of zSlices. */
void fp_slice_depth_range( uint slice, uint zSlices, uint zMode, float zNear, float zFar,
	out float sliceNear, out float sliceFar )
{
	float zScale = 0.0;
	float zBias = 0.0;
	if ( zMode == 1u && zSlices > 1u ) {
		float zn = max( zNear, 1e-3 );
		float zf = max( zFar, zn + 1e-3 );
		float logNear = log2( zn );
		float logFar = log2( zf );
		float denom = max( logFar - logNear, 1e-5 );
		zScale = float( zSlices ) / denom;
		zBias = -logNear * zScale;
	}
	Cluster_SliceDepthRange( slice, zSlices, zMode, zNear, zFar, zScale, zBias, sliceNear, sliceFar );
}

bool fp_light_overlaps_slice( float lightNear, float lightFar, float sliceNear, float sliceFar )
{
	return Cluster_LightOverlapsSlice( lightNear, lightFar, sliceNear, sliceFar );
}

uvec2 fp_light_slice_span( float lightNear, float lightFar, uint zSlices, uint zMode,
	float zNear, float zFar )
{
	ClusterParams p;
	p.clusterCountX = 1u;
	p.clusterCountY = 1u;
	p.clusterCountZ = zSlices;
	p.tileSizeX = CLUSTER_TILE_SIZE_X;
	p.tileSizeY = CLUSTER_TILE_SIZE_Y;
	p.lightIndexCapacity = 0u;
	p.zNear = zNear;
	p.zFar = zFar;
	p.zScale = 0.0;
	p.zBias = 0.0;
	p.generation = 0u;
	p.overflowCount = 0u;
	p.flags = 0u;
	p.reserved = 0u;
	if ( zMode == 1u && zSlices > 1u ) {
		float zn = max( zNear, 1e-3 );
		float zf = max( zFar, zn + 1e-3 );
		float logNear = log2( zn );
		float logFar = log2( zf );
		float denom = max( logFar - logNear, 1e-5 );
		p.zScale = float( zSlices ) / denom;
		p.zBias = -logNear * p.zScale;
	}
	return Cluster_LightSliceSpan( lightNear, lightFar, p, zMode );
}

#endif /* FORWARD_PLUS_CLUSTER_GLSL */
