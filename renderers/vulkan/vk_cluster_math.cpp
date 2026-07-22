/* C++20 migration: mechanical conversion; C ABI preserved (extern "C"). */

extern "C" {

/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Clustered Hybrid M2 — CPU reference for cluster Z slicing / indexing.
===========================================================================
*/

#include "vk_cluster_contract.h"
#include <math.h>

#ifndef M_LN2
#define M_LN2 0.693147180559945309417
#endif

void Cluster_DeriveLogZScaleBias( float zNear, float zFar, uint32_t clusterCountZ,
	float *outScale, float *outBias )
{
	float zn = zNear > 1e-3f ? zNear : 1e-3f;
	float zf = zFar > zn + 1e-3f ? zFar : zn + 1e-3f;
	uint32_t Z = clusterCountZ > 0u ? clusterCountZ : 1u;

	if ( Z <= 1u ) {
		if ( outScale ) {
			*outScale = 0.0f;
		}
		if ( outBias ) {
			*outBias = 0.0f;
		}
		return;
	}

	/*
	 * Map log2(zn) → 0, log2(zf) → Z (exclusive upper), so
	 * slice = clamp(int(log2(z)*scale + bias), 0, Z-1)
	 * with scale = Z / (log2(zf)-log2(zn)), bias = -log2(zn)*scale.
	 */
	{
		float logNear = logf( zn ) / (float)M_LN2;
		float logFar = logf( zf ) / (float)M_LN2;
		float denom = logFar - logNear;
		float scale = ( denom > 1e-5f ) ? ( (float)Z / denom ) : 0.0f;
		float bias = -logNear * scale;
		if ( outScale ) {
			*outScale = scale;
		}
		if ( outBias ) {
			*outBias = bias;
		}
	}
}

uint32_t Cluster_ViewDepthToSlice( float viewDepth, uint32_t clusterCountZ, uint32_t zMode,
	float zNear, float zFar, float zScale, float zBias )
{
	float zn, zf, z;
	int slice;
	uint32_t Z = clusterCountZ > 0u ? clusterCountZ : 1u;

	if ( Z <= 1u ) {
		return 0u;
	}
	zn = zNear > 1e-3f ? zNear : 1e-3f;
	zf = zFar > zn + 1e-3f ? zFar : zn + 1e-3f;
	z = viewDepth < 0.0f ? -viewDepth : viewDepth;
	if ( !( z > 0.0f ) || z != z ) {
		return 0u;
	}
	if ( z < zn ) {
		z = zn;
	}
	if ( z > zf ) {
		z = zf;
	}

	if ( zMode == 1u ) {
		slice = (int)( logf( z ) / (float)M_LN2 * zScale + zBias );
	} else {
		float t = ( z - zn ) / ( zf - zn );
		if ( t < 0.0f ) {
			t = 0.0f;
		}
		if ( t > 0.9999f ) {
			t = 0.9999f;
		}
		slice = (int)( t * (float)Z );
	}
	if ( slice < 0 ) {
		slice = 0;
	}
	if ( slice >= (int)Z ) {
		slice = (int)Z - 1;
	}
	return (uint32_t)slice;
}

uint32_t Cluster_IndexFromPixelAndViewDepth( uint32_t pixelX, uint32_t pixelY, float viewDepth,
	const gpuClusterParams_t *params, uint32_t zMode )
{
	uint32_t tx, ty, slice;
	uint32_t cx, cy, cz;

	if ( !params ) {
		return 0u;
	}
	cx = params->clusterCountX > 0u ? params->clusterCountX : 1u;
	cy = params->clusterCountY > 0u ? params->clusterCountY : 1u;
	cz = params->clusterCountZ > 0u ? params->clusterCountZ : 1u;
	{
		uint32_t tsx = params->tileSizeX > 0u ? params->tileSizeX : VK_CLUSTER_TILE_SIZE_X;
		uint32_t tsy = params->tileSizeY > 0u ? params->tileSizeY : VK_CLUSTER_TILE_SIZE_Y;
		tx = pixelX / tsx;
		ty = pixelY / tsy;
		if ( tx >= cx ) {
			tx = cx - 1u;
		}
		if ( ty >= cy ) {
			ty = cy - 1u;
		}
	}
	slice = Cluster_ViewDepthToSlice( viewDepth, cz, zMode, params->zNear, params->zFar,
		params->zScale, params->zBias );
	if ( cz <= 1u ) {
		return ty * cx + tx;
	}
	return ( ty * cx + tx ) + slice * ( cx * cy );
}

void Cluster_SliceDepthRange( uint32_t slice, uint32_t clusterCountZ, uint32_t zMode,
	float zNear, float zFar, float zScale, float zBias,
	float *outNear, float *outFar )
{
	float zn = zNear > 1e-3f ? zNear : 1e-3f;
	float zf = zFar > zn + 1e-3f ? zFar : zn + 1e-3f;
	uint32_t Z = clusterCountZ > 0u ? clusterCountZ : 1u;
	float sn, sf;

	if ( Z <= 1u ) {
		sn = zn;
		sf = zf;
	} else if ( zMode == 1u ) {
		float invScale = ( zScale > 1e-5f ) ? ( 1.0f / zScale ) : 0.0f;
		sn = exp2f( ( (float)slice - zBias ) * invScale );
		sf = exp2f( ( (float)( slice + 1u ) - zBias ) * invScale );
		if ( sn < zn ) {
			sn = zn;
		}
		if ( sf > zf ) {
			sf = zf;
		}
		if ( sf < sn ) {
			float t = sn;
			sn = sf;
			sf = t;
		}
	} else {
		float t0 = (float)slice / (float)Z;
		float t1 = (float)( slice + 1u ) / (float)Z;
		sn = zn + ( zf - zn ) * t0;
		sf = zn + ( zf - zn ) * t1;
	}
	if ( outNear ) {
		*outNear = sn;
	}
	if ( outFar ) {
		*outFar = sf;
	}
}

uint32_t Cluster_TotalCount( const gpuClusterParams_t *params )
{
	if ( !params ) {
		return 0u;
	}
	{
		uint32_t cx = params->clusterCountX > 0u ? params->clusterCountX : 1u;
		uint32_t cy = params->clusterCountY > 0u ? params->clusterCountY : 1u;
		uint32_t cz = params->clusterCountZ > 0u ? params->clusterCountZ : 1u;
		return cx * cy * cz;
	}
}

} /* extern "C" */
