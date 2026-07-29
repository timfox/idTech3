/*
 * Clustered Hybrid M2 — GLSL mirror of vk_cluster_contract.h
 * viewDepth = positive forward view-space distance.
 * (No #version — included into compute/fragment shaders.)
 */

#ifndef CLUSTER_CONTRACT_GLSL
#define CLUSTER_CONTRACT_GLSL

const uint CLUSTER_TILE_SIZE_X = 16u;
const uint CLUSTER_TILE_SIZE_Y = 16u;
const uint CLUSTER_HEADER_BYTES = 8u;
const uint CLUSTER_FLAG_LOG_Z = 1u;
const uint CLUSTER_FLAG_COMPACT_LISTS = 2u;
const uint CLUSTER_FLAG_OVERFLOW = 4u;
const uint CLUSTER_FLAG_BUILD_FAILED = 8u;
const uint CLUSTER_FLAG_FALLBACK_LEGACY = 16u;

struct ClusterHeader {
	uint offset;
	uint count;
};

struct ClusterParams {
	uint clusterCountX;
	uint clusterCountY;
	uint clusterCountZ;
	uint tileSizeX;
	uint tileSizeY;
	uint lightIndexCapacity;
	float zNear;
	float zFar;
	float zScale;
	float zBias;
	uint generation;
	uint overflowCount;
	uint flags;
	uint reserved;
};

uint Cluster_XYCount( uint cx, uint cy )
{
	return max( cx * cy, 1u );
}

uint Cluster_ViewDepthToSlice( float viewDepth, uint clusterCountZ, uint zMode,
	float zNear, float zFar, float zScale, float zBias )
{
	if ( clusterCountZ <= 1u ) {
		return 0u;
	}
	float zn = max( zNear, 1e-3 );
	float zf = max( zFar, zn + 1e-3 );
	float z = abs( viewDepth );
	if ( !( z > 0.0 ) || z != z ) {
		return 0u;
	}
	z = clamp( z, zn, zf );
	int slice;
	if ( zMode == 1u ) {
		/* log2 mapping: slice = clamp(int(log2(z)*zScale + zBias), 0, Z-1) */
		slice = int( log2( max( z, zn ) ) * zScale + zBias );
	} else {
		float t = ( z - zn ) / max( zf - zn, 1e-5 );
		slice = int( clamp( t, 0.0, 0.9999 ) * float( clusterCountZ ) );
	}
	return uint( clamp( slice, 0, int( clusterCountZ ) - 1 ) );
}

uint Cluster_IndexFromTileAndSlice( uint tx, uint ty, uint slice, ClusterParams p )
{
	uint xy = ty * p.clusterCountX + tx;
	if ( p.clusterCountZ <= 1u ) {
		return xy;
	}
	return xy + slice * Cluster_XYCount( p.clusterCountX, p.clusterCountY );
}

uint Cluster_IndexFromPixelAndViewDepth( uvec2 pixel, float viewDepth, ClusterParams p, uint zMode )
{
	uint tileX = min( pixel.x / max( p.tileSizeX, 1u ), max( p.clusterCountX, 1u ) - 1u );
	uint tileY = min( pixel.y / max( p.tileSizeY, 1u ), max( p.clusterCountY, 1u ) - 1u );
	uint slice = Cluster_ViewDepthToSlice( viewDepth, p.clusterCountZ, zMode,
		p.zNear, p.zFar, p.zScale, p.zBias );
	return Cluster_IndexFromTileAndSlice( tileX, tileY, slice, p );
}

void Cluster_SliceDepthRange( uint slice, uint clusterCountZ, uint zMode,
	float zNear, float zFar, float zScale, float zBias,
	out float sliceNear, out float sliceFar )
{
	float zn = max( zNear, 1e-3 );
	float zf = max( zFar, zn + 1e-3 );
	if ( clusterCountZ <= 1u ) {
		sliceNear = zn;
		sliceFar = zf;
		return;
	}
	if ( zMode == 1u ) {
		/* Inverse of log2 mapping using same scale/bias. */
		float s0 = float( slice );
		float s1 = float( slice + 1u );
		sliceNear = exp2( ( s0 - zBias ) / max( zScale, 1e-5 ) );
		sliceFar = exp2( ( s1 - zBias ) / max( zScale, 1e-5 ) );
		sliceNear = clamp( sliceNear, zn, zf );
		sliceFar = clamp( sliceFar, zn, zf );
		if ( sliceFar < sliceNear ) {
			float tmp = sliceNear;
			sliceNear = sliceFar;
			sliceFar = tmp;
		}
	} else {
		float t0 = float( slice ) / float( clusterCountZ );
		float t1 = float( slice + 1u ) / float( clusterCountZ );
		sliceNear = mix( zn, zf, t0 );
		sliceFar = mix( zn, zf, t1 );
	}
}

bool Cluster_LightOverlapsSlice( float lightNear, float lightFar, float sliceNear, float sliceFar )
{
	float ln = lightNear;
	float lf = lightFar;
	float sn = sliceNear;
	float sf = sliceFar;
	if ( ln != ln || lf != lf || sn != sn || sf != sf ) {
		return false;
	}
	if ( lf < ln ) {
		float t = ln;
		ln = lf;
		lf = t;
	}
	if ( sf < sn ) {
		float t = sn;
		sn = sf;
		sf = t;
	}
	return !( lf < sn || ln > sf );
}

uvec2 Cluster_LightSliceSpan( float lightNear, float lightFar, ClusterParams p, uint zMode )
{
	float ln = lightNear;
	float lf = lightFar;
	if ( lf < ln ) {
		float t = ln;
		ln = lf;
		lf = t;
	}
	uint firstSlice = Cluster_ViewDepthToSlice( ln, p.clusterCountZ, zMode,
		p.zNear, p.zFar, p.zScale, p.zBias );
	uint lastSlice = Cluster_ViewDepthToSlice( lf, p.clusterCountZ, zMode,
		p.zNear, p.zFar, p.zScale, p.zBias );
	if ( lastSlice < firstSlice ) {
		uint t = firstSlice;
		firstSlice = lastSlice;
		lastSlice = t;
	}
	return uvec2( firstSlice, lastSlice );
}

#endif /* CLUSTER_CONTRACT_GLSL */
