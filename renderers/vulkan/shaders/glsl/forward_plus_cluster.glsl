/*
 * Shared Forward+ / clustered light-grid helpers.
 * Cluster layout: flat 2D tile first (slice 0), then +tilesX*tilesY per Z slice.
 * zSlices==1 preserves historical 2D tiled indexing (tileId * MAX_PER_TILE).
 *
 * Depth: use positive view-space distance (abs of reconstructed view Z / clip.w).
 * Slice modes: 0 = linear in [zNear,zFar], 1 = logarithmic.
 */

#ifndef FORWARD_PLUS_CLUSTER_GLSL
#define FORWARD_PLUS_CLUSTER_GLSL

uint fp_cluster_xy_count( uint tilesX, uint tilesY )
{
	return max( tilesX * tilesY, 1u );
}

uint fp_view_depth_to_slice( float viewDepth, uint zSlices, uint zMode, float zNear, float zFar )
{
	if ( zSlices <= 1u ) {
		return 0u;
	}
	float zn = max( zNear, 1e-3 );
	float zf = max( zFar, zn + 1e-3 );
	float z = clamp( abs( viewDepth ), zn, zf );
	float t;
	if ( zMode == 1u ) {
		t = log( z / zn ) / max( log( zf / zn ), 1e-5 );
	} else {
		t = ( z - zn ) / ( zf - zn );
	}
	t = clamp( t, 0.0, 0.9999 );
	return min( uint( t * float( zSlices ) ), zSlices - 1u );
}

uint fp_cluster_index( uint tx, uint ty, uint tilesX, uint tilesY, uint slice, uint zSlices )
{
	uint xy = ty * tilesX + tx;
	if ( zSlices <= 1u ) {
		return xy;
	}
	return xy + slice * fp_cluster_xy_count( tilesX, tilesY );
}

/* Slice [near,far] in positive view depth for slice i of zSlices. */
void fp_slice_depth_range( uint slice, uint zSlices, uint zMode, float zNear, float zFar,
	out float sliceNear, out float sliceFar )
{
	float zn = max( zNear, 1e-3 );
	float zf = max( zFar, zn + 1e-3 );
	if ( zSlices <= 1u ) {
		sliceNear = zn;
		sliceFar = zf;
		return;
	}
	float t0 = float( slice ) / float( zSlices );
	float t1 = float( slice + 1u ) / float( zSlices );
	if ( zMode == 1u ) {
		sliceNear = zn * pow( zf / zn, t0 );
		sliceFar = zn * pow( zf / zn, t1 );
	} else {
		sliceNear = mix( zn, zf, t0 );
		sliceFar = mix( zn, zf, t1 );
	}
}

bool fp_light_overlaps_slice( float lightNear, float lightFar, float sliceNear, float sliceFar )
{
	return !( lightFar < sliceNear || lightNear > sliceFar );
}

#endif /* FORWARD_PLUS_CLUSTER_GLSL */
