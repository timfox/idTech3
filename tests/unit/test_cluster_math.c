/*
 * Unit test: Clustered Hybrid M2 log-Z / cluster index math (CPU reference).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "vk_cluster_contract.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

#define ASSERTF(cond, msg, ...) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: " msg "\n", __VA_ARGS__); return 1; } \
} while (0)

int main( void )
{
	float zScale = 0.0f, zBias = 0.0f;
	const float zNear = 4.0f;
	const float zFar = 4096.0f;
	const uint32_t Z = 8u;
	uint32_t i;

	Cluster_DeriveLogZScaleBias( zNear, zFar, Z, &zScale, &zBias );
	ASSERT( zScale > 0.0f, "log zScale > 0" );

	/* Boundaries: zNear → slice 0, just below zFar → last slice. */
	ASSERT( Cluster_ViewDepthToSlice( zNear, Z, 1u, zNear, zFar, zScale, zBias ) == 0u,
		"zNear maps to slice 0" );
	ASSERT( Cluster_ViewDepthToSlice( zFar * 0.999f, Z, 1u, zNear, zFar, zScale, zBias ) == Z - 1u,
		"near-zFar maps to last slice" );
	ASSERT( Cluster_ViewDepthToSlice( zFar, Z, 1u, zNear, zFar, zScale, zBias ) == Z - 1u,
		"zFar clamps to last slice" );
	ASSERT( Cluster_ViewDepthToSlice( zNear * 0.5f, Z, 1u, zNear, zFar, zScale, zBias ) == 0u,
		"below zNear clamps to 0" );
	ASSERT( Cluster_ViewDepthToSlice( -100.0f, Z, 1u, zNear, zFar, zScale, zBias ) ==
		Cluster_ViewDepthToSlice( 100.0f, Z, 1u, zNear, zFar, zScale, zBias ),
		"negative viewDepth uses abs" );
	ASSERT( Cluster_ViewDepthToSlice( 0.0f, Z, 1u, zNear, zFar, zScale, zBias ) == 0u,
		"zero depth → slice 0" );

	/* Slice ranges abut without gaps (log mode). */
	for ( i = 0u; i < Z; i++ ) {
		float sn = 0.0f, sf = 0.0f, sn2 = 0.0f, sf2 = 0.0f;
		Cluster_SliceDepthRange( i, Z, 1u, zNear, zFar, zScale, zBias, &sn, &sf );
		ASSERTF( sf >= sn, "slice %u far>=near", i );
		if ( i + 1u < Z ) {
			Cluster_SliceDepthRange( i + 1u, Z, 1u, zNear, zFar, zScale, zBias, &sn2, &sf2 );
			ASSERTF( fabsf( sf - sn2 ) < 1e-2f * ( sf + 1.0f ),
				"slice %u/%u boundary gap/overlap (%.6f vs %.6f)", i, i + 1u, sf, sn2 );
		}
		/* Midpoint of slice maps back to same slice. */
		{
			float mid = 0.5f * ( sn + sf );
			uint32_t got = Cluster_ViewDepthToSlice( mid, Z, 1u, zNear, zFar, zScale, zBias );
			ASSERTF( got == i, "midpoint depth %.3f expected slice %u got %u", mid, i, got );
		}
	}

	/* Linear mode debug path. */
	{
		uint32_t s0 = Cluster_ViewDepthToSlice( zNear, Z, 0u, zNear, zFar, 0.0f, 0.0f );
		uint32_t s1 = Cluster_ViewDepthToSlice( zFar - 1.0f, Z, 0u, zNear, zFar, 0.0f, 0.0f );
		ASSERT( s0 == 0u, "linear zNear → 0" );
		ASSERT( s1 == Z - 1u, "linear near-far → last" );
	}

	/* Pixel → cluster index (2D when Z=1). */
	{
		gpuClusterParams_t p;
		memset( &p, 0, sizeof( p ) );
		p.clusterCountX = 10;
		p.clusterCountY = 8;
		p.clusterCountZ = 1;
		p.tileSizeX = 16;
		p.tileSizeY = 16;
		p.zNear = zNear;
		p.zFar = zFar;
		ASSERT( Cluster_IndexFromPixelAndViewDepth( 0, 0, 100.0f, &p, 1u ) == 0u, "origin tile" );
		ASSERT( Cluster_IndexFromPixelAndViewDepth( 16, 0, 100.0f, &p, 1u ) == 1u, "tile x=1" );
		ASSERT( Cluster_IndexFromPixelAndViewDepth( 0, 16, 100.0f, &p, 1u ) == 10u, "tile y=1" );
		ASSERT( Cluster_TotalCount( &p ) == 80u, "2D total" );
	}

	/* 3D index: slice * XY + tile */
	{
		gpuClusterParams_t p;
		memset( &p, 0, sizeof( p ) );
		p.clusterCountX = 4;
		p.clusterCountY = 2;
		p.clusterCountZ = Z;
		p.tileSizeX = 16;
		p.tileSizeY = 16;
		p.zNear = zNear;
		p.zFar = zFar;
		Cluster_DeriveLogZScaleBias( zNear, zFar, Z, &p.zScale, &p.zBias );
		{
			uint32_t midSlice = Cluster_ViewDepthToSlice(
				0.5f * ( zNear + zFar ), Z, 1u, zNear, zFar, p.zScale, p.zBias );
			uint32_t idx = Cluster_IndexFromPixelAndViewDepth( 0, 0,
				0.5f * ( zNear + zFar ), &p, 1u );
			ASSERT( idx == midSlice * 8u, "3D cluster index = slice*XY" );
			ASSERT( Cluster_TotalCount( &p ) == 4u * 2u * Z, "3D total" );
		}
	}

	ASSERT( sizeof( gpuClusterHeader_t ) == 8, "header size" );
	ASSERT( sizeof( gpuClusterParams_t ) == 56, "params size" );

	printf( "unit_cluster_math: PASS\n" );
	return 0;
}
